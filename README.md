# PerfGuardian

A static performance analyzer for C++20 codebases. PerfGuardian scans your project
through the Clang AST, flags performance-sensitive patterns — large objects passed
by value, missing `const&`, containers grown without `reserve()`, lookups inside
loops — and reports them with the cost rationale and a concrete fix. It emits plain
text, JSON, HTML, and SARIF, and integrates with CI to fail a build when regressions
appear.

[![CI](https://github.com/Sambit-Kumar-Mohanty-26/PerfGuardian/actions/workflows/ci.yml/badge.svg)](https://github.com/Sambit-Kumar-Mohanty-26/PerfGuardian/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

---

## Features

- **Seven performance rules** over the real Clang AST — by-value copies, missing `const&`,
  missing `reserve()`, lookups in loops, redundant map lookups, and a whole-program rule.
- **Whole-program analysis** — a USR-keyed symbol index and cross-file call graph let it
  answer "who calls this, across translation units" and flag costs a single-file pass can't see.
- **Built for large repos** — parses translation units in parallel across all cores, with an
  incremental on-disk cache so unchanged files are skipped on the next run.
- **Distributed sharding** — split the work across machines (`--shard K/N`) and `merge` the
  partial results back into one whole-program report.
- **Autofix** — `--fix` rewrites source in place (e.g. `Player p` → `const Player& p`); SARIF
  carries the same machine-applicable edits.
- **Suppression at scale** — inline `// NOLINT`, hierarchical per-directory configs, and a
  baseline gate that blocks only *new* issues.
- **Drop-in for any build** — `compile_commands.json`, a Bazel `aquery` graph, or a bare folder.
- **CI-native** — text, JSON, HTML, and SARIF 2.1.0 output; exit codes gate the build.

---

## Install

### macOS / Linux (Homebrew)

```bash
brew tap Sambit-Kumar-Mohanty-26/tap
brew install perfguardian
```

### Windows (Scoop)

```powershell
scoop bucket add perfguardian https://github.com/Sambit-Kumar-Mohanty-26/homebrew-tap
scoop install perfguardian
```

### Direct download

Grab a prebuilt binary from the [latest release](https://github.com/Sambit-Kumar-Mohanty-26/PerfGuardian/releases/latest):

| Platform | Asset |
|---|---|
| Linux x86_64 | `perfguardian-linux-x86_64.tar.gz` |
| macOS arm64 | `perfguardian-macos-arm64.tar.gz` |
| macOS x86_64 | `perfguardian-macos-x86_64.tar.gz` |
| Windows x86_64 | `perfguardian-windows-x86_64.zip` |

Each asset ships with a matching `.sha256` file. Verify before use:

```bash
sha256sum -c perfguardian-linux-x86_64.tar.gz.sha256
```

---

## Quick Start

```bash
# Analyze a project and print findings to the terminal
perfguardian analyze /path/to/project

# Analyze the current directory and emit every report format
perfguardian analyze . --json report.json --html report.html --sarif report.sarif

# Fail the command (non-zero exit) if any HIGH-or-worse issue is found
perfguardian analyze . --fail-on high
```

PerfGuardian discovers translation units through `compile_commands.json`. Generate one
with `cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ...`, or point the tool at a directory
that contains it. Without a compilation database it falls back to scanning `.cpp`/`.hpp`
files directly.

**Bazel projects** have no `compile_commands.json`. Feed the build graph directly:

```bash
bazel aquery 'mnemonic("CppCompile", //...)' --output=jsonproto > actions.json
perfguardian analyze . --bazel-aquery actions.json --exec-root "$(bazel info execution_root)"
```

Each `CppCompile` action becomes a translation unit. (Toolchains that route the
command line through a param file instead of inlining it are skipped, with a warning.)

---

## Usage

```
perfguardian analyze <path> [options]     Analyze a C++ project
perfguardian merge <artifacts...>         Merge shard artifacts into one report (see Distributed analysis)
perfguardian list-rules                   List all available rules
perfguardian dump-ast <file>              Dump the Clang AST for one file (debugging)
perfguardian --version                    Print version
perfguardian --help                       Full help text
```

### `analyze` options

| Option | Description |
|---|---|
| `<path>` | Project directory to analyze (default: `.`) |
| `--json FILE` | Write a JSON report to `FILE` |
| `--html FILE` | Write an HTML dashboard to `FILE` |
| `--sarif FILE` | Write a SARIF 2.1.0 report (GitHub code scanning) to `FILE` |
| `--fail-on SEVERITY` | Exit non-zero if any issue at or above `SEVERITY` is found (`low`, `medium`, `high`, `critical`) |
| `--min-confidence LEVEL` | Only report findings at or above `LEVEL` (`low`, `medium`, `high`); gate CI to clear-cut findings |
| `--cache-dir DIR` | Cache parsed files in `DIR`; unchanged files are reused on the next run (large repos re-scan near-instantly) |
| `--bazel-aquery FILE` | Load compile actions from a Bazel `aquery --output=jsonproto` dump instead of `compile_commands.json` |
| `--exec-root DIR` | Bazel execution root for resolving relative paths in `--bazel-aquery` (defaults to `<path>`) |
| `--fix` | Rewrite source files in place, applying each finding's suggested fix (currently PG001/PG002 parameter edits, e.g. `Player p` → `const Player& p`) |
| `--baseline FILE` | Compare against a previous JSON report; with `--fail-on`, only **new** issues fail the run. If `FILE` doesn't exist it is seeded from this run (the run passes), so it works as a drop-in CI gate |
| `--shard K/N` | Analyze only shard `K` of `N` (0-based) — one slice of the translation units, for distributed runs |
| `--shard-out FILE` | Write this shard's partial artifact (findings + symbols + call edges) to `FILE`, to be combined with `merge` |

---

## Rules

| ID | Name | What it catches |
|---|---|---|
| PG001 | `large-object-by-value` | A parameter larger than the size threshold passed by value instead of `const&` |
| PG002 | `missing-const-ref` | A reference parameter that is never mutated and should be `const&` |
| PG003 | `reserve-before-loop` | A container grown in a loop with no preceding `reserve()` |
| PG004 | `find-in-loop` | A linear `find()` / lookup performed inside a loop |
| PG005 | `large-local-copy` | A large local variable copied where a reference would do |
| PG006 | `repeated-map-lookup` | The same key looked up in a map more than once |
| PG007 | `hot-pass-by-value` | A large by-value parameter on a function called widely across files (whole-program) |

Run `perfguardian list-rules` for the live catalog.

### Example finding

```
[HIGH] src/game.cpp:45:12
  Rule: PG001 large-object-by-value
  Parameter 'p' of type 'Player' is 800 bytes, passed by value
  Suggested fix: const Player&
```

---

## Whole-program analysis

Most lint rules see one file at a time. PerfGuardian also builds a **cross-translation-unit
view** of the whole project:

- a **global symbol index** keyed by Clang USR, so a function defined in one file is the same
  symbol as a call to it in another (definitions supersede forward declarations); and
- a **call graph** of caller→callee edges across files, pruned to the project's own functions.

This is what powers **PG007 (`hot-pass-by-value`)**: a function that takes a large object by
value isn't just a local smell — if it's called from many sites across many files, the copy
cost is multiplied program-wide and a single `const&` fix pays off everywhere. The caller count
and file spread come from the call graph, so this finding is **impossible to produce from a
single file**.

On [google/leveldb](https://github.com/google/leveldb) (76 translation units) the index resolves
**1,086 functions** across TUs and the call graph holds **1,238 internal edges**; the busiest
function, `ToString()`, resolves to **27 callers across 11 files**.

---

## Performance

PerfGuardian is built to stay fast on large repositories:

| Stage | Mechanism | leveldb (76 TUs, 18 cores) |
|---|---|---|
| Parallel parsing | Thread pool over translation units; each keeps its own libclang index | ~80 s → **~6 s** (~13×) |
| Incremental cache | `--cache-dir`: a content+args+version hash reuses unchanged TUs from disk | warm run **8.2 s → 0.16 s** (~53×) |
| Bounded memory | Rules run per-TU and parsed bodies are discarded immediately | peak set by in-flight TUs, not repo size |

Findings are byte-identical regardless of thread count, cache state, or shard layout — output is
deterministically sorted by `(file, line, column, rule)`.

---

## Distributed analysis

For very large codebases, split the translation units across machines and merge the results.
Each shard analyzes a deterministic slice and writes a partial artifact (its findings, symbol
summaries, and call-graph edges):

```bash
# Machine 0 of 3
perfguardian analyze . --shard 0/3 --shard-out shard0.json
# Machine 1 of 3
perfguardian analyze . --shard 1/3 --shard-out shard1.json
# Machine 2 of 3
perfguardian analyze . --shard 2/3 --shard-out shard2.json
```

Then `merge` unions the symbol indexes and call graphs, prunes to the project, and runs the
cross-TU rules over the reconstructed whole-program view — recovering findings (like PG007) that
no single shard could see. The merge step needs **no compiler**, only the JSON artifacts:

```bash
perfguardian merge shard0.json shard1.json shard2.json --sarif results.sarif --fail-on high
```

The merged result is identical to a single monolithic run. (On leveldb, a 3-way shard merges
back to the same 1,086 symbols, 1,238 edges, and 69 findings.)

---

## Configuration

Drop a `.perfguardian.yaml` file in your project root. Every key is optional;
unset rules use their built-in defaults.

```yaml
version: 1

rules:
  PG001:
    enabled: true
    size_threshold_bytes: 64      # flag parameters larger than this
  PG003:
    enabled: true
  PG005:
    copy_size_threshold: 128      # flag local copies larger than this
  PG006:
    min_repeat_count: 2           # flag keys looked up at least this many times

# Silence specific findings. A suppression matches when every field it
# specifies matches the diagnostic. `function` and `file` accept * and ? globs.
suppressions:
  - rule: PG001
    file: third_party/*
  - rule: PG004
    function: legacy_*
```

To disable a rule entirely, set `enabled: false` under its ID.

Configs nest: a `.perfguardian.yaml` in a subdirectory is merged with those above it,
so a repo-wide config and per-directory overrides both apply (deeper wins for rule
settings; suppressions from every level accumulate).

### Inline suppression

Silence a single finding right where it occurs, clang-tidy style:

```cpp
void render(Mesh m);                       // NOLINT            — silence all rules here
void render(Mesh m);                       // NOLINT(PG001)     — silence one rule
void render(Mesh m);                       // NOLINT(PG001,PG002)
// NOLINTNEXTLINE(PG001)
void render(Mesh m);                                            // silence the next line
```

A bare `NOLINT` (no parentheses) matches every rule.

---

## CI integration

PerfGuardian emits SARIF 2.1.0, which GitHub Code Scanning ingests directly. A minimal
workflow step:

```yaml
- name: Run PerfGuardian
  run: |
    perfguardian analyze . --sarif results.sarif --fail-on high

- name: Upload results to code scanning
  uses: github/codeql-action/upload-sarif@v3
  with:
    sarif_file: results.sarif
    category: perfguardian
```

Findings then appear inline on pull requests under the **Security** tab.

### Baseline workflow

To allow existing issues but block new ones, save a JSON report from your main branch
and diff against it:

```bash
# On a PR, fail only if the PR introduces new findings.
# The first run auto-seeds baseline.json from the current state and passes;
# every run after that gates on new issues only.
perfguardian analyze . --baseline baseline.json --fail-on high
```

(You can still pre-record a baseline explicitly with `--json baseline.json` if you
prefer to commit it from a known-good branch.)

Baseline matching is line-number-stable: a finding that merely shifts to a different
line is treated as the same issue, not a new one.

---

## Build from source

**Requirements**

- CMake 3.20+
- A C++20 compiler (GCC 12+, Clang 16+, or MSVC 2022+)
- LLVM/Clang development libraries (libclang)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPERFGUARDIAN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

All other dependencies (CLI11, nlohmann/json, spdlog, yaml-cpp, GoogleTest) are fetched
automatically via CMake `FetchContent`.

<details>
<summary>Windows (MSYS2 / MinGW64)</summary>

PerfGuardian builds on Windows with the MSYS2 MinGW64 toolchain:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja mingw-w64-x86_64-clang \
          mingw-w64-x86_64-llvm

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The MinGW64 `bin` directory must be on `PATH` at runtime so the bundled
`libclang.dll` and GCC runtime DLLs resolve.
</details>

---

## How it works

```
  compile_commands.json · Bazel aquery · or a bare folder
        │
        ▼
  Project loader ──► sources + compile args   (optional --shard slice)
        │
        ▼
  ┌─────────────────────── parallel over TUs ───────────────────────┐
  │  Clang AST parser (libclang)        ◄──► incremental cache (disk) │
  │        │ per-TU ParseResult                                       │
  │        ├─► intra-TU rules (PG001–PG006) ──► DiagnosticSink        │
  │        ├─► global symbol index   (USR → SymbolSummary)            │
  │        └─► call graph            (caller → callee edges)          │
  └──────────────────────────────────────────────────────────────────┘
        │
        ▼                                  (--shard-out writes a partial
  cross-TU rules (PG007) over the           artifact here; `merge` rejoins
  merged symbol index + call graph          shards before this step)
        │
        ▼
  suppressions (config · NOLINT) ──► confidence filter ──► hotspot ranker
        │
        ▼
  reporters: text · JSON · HTML · SARIF      autofix (--fix)
        │
        ▼
  CI gate: exit codes · baseline diff
```

See [docs/architecture.md](docs/architecture.md) for the full design.

---

## License

MIT — see [LICENSE](LICENSE).
