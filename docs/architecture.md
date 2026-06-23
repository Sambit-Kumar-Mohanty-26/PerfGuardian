# PerfGuardian — Architecture

PerfGuardian is a whole-program static performance analyzer for C++20. It parses a
project through libclang, runs per-translation-unit rules, builds a cross-TU symbol
index and call graph, runs whole-program rules over them, and emits gated reports.
This document describes the design as built (roadmap phases 0–23).

## Component map

```
perfguardian/
├── cli/main.cpp              # Subcommand dispatch: analyze · merge · list-rules · dump-ast
├── core/
│   ├── include/perfguardian/ # Public headers (one per component below)
│   └── src/
│       ├── project_loader/   # compile_commands.json, Bazel aquery, folder scan, arg sanitising
│       ├── parser/           # libclang AST walk → ParseResult (clang_parser.cpp)
│       ├── symbol_db/        # SymbolDB: one TU's functions, types, sizes, call sites
│       ├── rules/            # IRule + PG001–PG006 (intra-TU) and cross_tu_rules (PG007)
│       ├── analysis/         # symbol_index · call_graph · hotspot · parse_cache · autofix ·
│       │                     #   shard · baseline · json/html/sarif reporters
│       └── config/           # .perfguardian.yaml loader/merge · inline NOLINT
├── samples/                  # Demo projects with known issues
├── tests/                    # GoogleTest unit + integration suite (smoke_test.cpp)
└── docs/                     # This file
```

## Data flow

```
 compile_commands.json | bazel aquery jsonproto | bare folder
        │
        ▼
   ProjectLoader ─► [(source, compile args)]         (optional --shard slice, sorted + round-robin)
        │
        ▼
 ┌──────────────────────── parallel thread pool over TUs ────────────────────────┐
 │  ClangParser (own CXIndex per thread)        ◄──►  ParseCache (FNV-1a on disk)  │
 │        │ ParseResult { FunctionDecl[], TypeDecl[], errors }                     │
 │        ├─► run_rules(SymbolDB)  → DiagnosticSink     (PG001–PG006, per TU)      │
 │        ├─► GlobalSymbolIndex.add()  (USR → SymbolSummary, defs win)             │
 │        └─► CallGraph.add()          (caller USR → callee USR edges)             │
 │  Parsed body is discarded as soon as its findings are collected (bounded mem)  │
 └────────────────────────────────────────────────────────────────────────────────┘
        │                                   ── shard mode: write ShardArtifact and stop ──┐
        ▼                                                                                  │
   CallGraph.prune_to(index)   (drop edges into the std lib, keep project-internal)        │
        │                                                                                  │
        ▼                                            merge: read N artifacts, union ◄───────┘
   run_cross_tu_rules(index, graph, sink)            index + edges, then prune + cross-TU
        │  (PG007 hot-pass-by-value)
        ▼
   suppressions: config globs + inline // NOLINT
        │
        ▼
   confidence filter (--min-confidence) ─► stable sort ─► HotspotRanker
        │
        ├─► reporters: text · JSON · HTML · SARIF 2.1.0
        ├─► autofix (--fix): apply FixIt spans to source files
        └─► CI gate: --fail-on severity · baseline diff (auto-seeded)
```

## Key data structures

- **ParseResult** (`parse_result.hpp`) — what one TU yields: `FunctionDecl`s (each with
  `ParamInfo` params carrying type, size, mutation/move-only flags, and a source span for
  autofix), `CallSite`s (callee USR for the call graph), `LocalVar`s, and `TypeDecl`s. This is
  the unit the cache serializes.
- **SymbolDB** (`symbol_db/`) — a single TU's symbols, the input every intra-TU rule reads.
- **GlobalSymbolIndex** (`analysis/symbol_index.cpp`) — `unordered_map<USR, SymbolSummary>`
  merged across all TUs; a definition supersedes a declaration. `SymbolSummary` is compact
  (no bodies) so the index stays small on big repos.
- **CallGraph** (`analysis/call_graph.cpp`) — deduplicated caller→callee edges keyed by USR,
  with `callers_of` / `callees_of` queries and `prune_to(index)` to keep project-internal edges.
- **Diagnostic / DiagnosticSink** (`diagnostic.hpp`) — a finding (rule, severity, confidence,
  location, message, suggested fix, optional `FixIt` edits) and the collector, which supports
  filtering, deterministic `sort()`, and `remove_if` for suppression.
- **ShardArtifact** (`analysis/shard.cpp`) — JSON-serializable `{ findings, symbols, edges }`
  exchanged between `analyze --shard-out` and `merge`.

## Rule interface

Intra-TU rules implement `IRule`:

```cpp
class IRule {
public:
    virtual ~IRule() = default;
    virtual void             run(const SymbolDB& db, DiagnosticSink& sink,
                                 const RuleConfig& cfg) const = 0;
    virtual std::string_view rule_id()   const = 0;   // "PG001"
    virtual std::string_view rule_name() const = 0;   // "large-object-by-value"
};
```

They run once per translation unit during the parallel parse. `RuleConfig` carries the tunable
thresholds (sizes, repeat counts) resolved from `.perfguardian.yaml`.

Whole-program rules are different: they need the *complete* index and graph, so they can't run
per-TU. They run once after parsing via `run_cross_tu_rules(index, graph, sink, cfg)` — PG007
lives here.

## Severity and confidence

| Severity | Gate behavior | Examples |
|---|---|---|
| Info / Low | never fails | style notes, minor inefficiencies |
| Medium | fails with `--fail-on medium` | missing `reserve()`, missing `const&` |
| High | fails with `--fail-on high` | large by-value copies, find-in-loop |
| Critical | always contributes | reserved for the most severe patterns |

Each finding also carries a **confidence** (Low/Medium/High). `--min-confidence` filters noisy
findings so CI can gate on only clear-cut ones (e.g. PG001 is High; PG002 is Medium because its
mutation analysis is deliberately conservative).

## Cross-cutting properties

- **Determinism** — output is sorted by `(file, line, column, rule)`, so results are identical
  across thread counts, cache hits, and shard layouts.
- **Bounded memory** — rules execute per-TU and parsed bodies are freed immediately; only the
  compact symbol index, call graph, and findings persist.
- **Incrementality** — `--cache-dir` keys each TU by a hash of its content, compile args, and the
  tool version; unchanged TUs are reloaded from disk instead of reparsed.
- **Suppression layers** — repo-wide and per-directory `.perfguardian.yaml` (merged, deeper wins),
  plus inline `// NOLINT` / `// NOLINTNEXTLINE`, plus a baseline that blocks only new issues.
