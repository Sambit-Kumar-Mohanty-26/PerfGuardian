# PerfGuardian Roadmap

Path from a working single-project analyzer toward a large-scale, low-false-positive
performance analysis tool. Phases 0–10 (engine, six rules, text/JSON/HTML/SARIF
reports, baseline diffing, real multi-file project parsing) are complete.

The remaining work is grouped into **four pillars**, sequenced by leverage. Do them
roughly in order — each makes the next more valuable.

---

## Pillar A — Precision (make results trustworthy)

A tool that flags correct code gets switched off. Nothing else matters until the
signal-to-noise ratio is high. This is mostly self-contained rule work.

| Phase | Goal | Key work | Done when |
|---|---|---|---|
| **11. Mutation analysis** ✅ | PG002 stops flagging references that are written to (streams, sinks, out-params) | Track per-parameter "is mutated" in the body visitor: assignment, `++`/`--`, address-of, non-const member call, passing to a non-const reference/pointer. PG002 skips mutated params. | **Done.** PG002 on a self-scan dropped 58 → 4; the 4 residuals are reference-capturing constructors (`m_x(x)`), deferred to Phase 12. |
| **12. Move-only & owning types** ✅ | PG001 ignores move-only types; PG002 handles reference capture | Detect move-only types (`unique_ptr`, `mutex`, `atomic`, … and user types with a deleted copy ctor); PG001 skips them since by-value is the sink idiom. Treat a parameter bound to a reference member (capturing constructor) as non-const-able. | **Done.** On a self-scan, PG002 4 → 2 and PG001 4 → 3; remaining findings are all legitimate. |
| **13. Confidence levels** ✅ | Each finding carries High/Medium/Low confidence | `Confidence` on `Diagnostic`, per-rule levels, `--min-confidence` flag, confidence in text + JSON reports | **Done.** `--min-confidence high` gates CI to clear-cut findings (PG001); reported inline. |
| **13b. Skip macro misparses** ✅ | No false positives from macro-generated / undefined-macro code (e.g. gtest `TEST_F`) | Skip macro-expanded declarations (spelling vs expansion location) and undefined function-like macros error-recovered as functions (first token == function name) | **Done.** leveldb scan dropped 58 phantom `TEST_F` findings (PG001 76 → 10). |

**Pillar A done:** < 5% false positives on a 50-finding hand-labeled sample. ✅ Pillars 11–13 complete; **13b queued** (found via the leveldb validation below).

### Validation — leveldb (Google C++, 132 files)

A folder scan of [google/leveldb](https://github.com/google/leveldb) parsed all 76 TUs
(29 ok + 47 partial, **0 hard failures**), indexed 1389 functions, and produced 142
findings across all six rules with no crashes. PG002 fired only twice on 1389
functions, confirming the mutation analysis. Two issues surfaced: a single-threaded
runtime of ~80 s (→ Phase 14) and false positives from gtest macro expansion (→ Phase 13b).

---

## Pillar B — Scale (parallel + incremental)

Once results are trustworthy, make it fast enough for large repos. Contained,
high-impact changes.

| Phase | Goal | Key work | Done when |
|---|---|---|---|
| **14. Parallel parsing** ✅ | Use all CPU cores | Thread pool over an atomic work index in `cli/main.cpp`; `parse_file` keeps its own `CXIndex`; `SymbolDB` merge under a mutex; `DiagnosticSink::sort()` for reproducible output | **Done.** leveldb 76 TUs: 80 s → 6 s (~13× on 18 cores); output deterministic across runs. |
| **15. Incremental cache** | Re-analyze only changed files | Hash each TU's source + args; cache results on disk keyed by hash; skip unchanged | Second run on an unchanged repo is near-instant |
| **16. Memory bounds** | Don't hold the whole repo in RAM | Process and discard per-TU; keep only the symbol summary needed for cross-TU | Constant memory on a 100k-file repo |

**Pillar B done:** analyze a 5,000-file project in under 30 s warm.

---

## Pillar C — Whole-program (cross-translation-unit analysis)

The architectural leap that enables the rules large-scale tools are known for.
Biggest effort.

| Phase | Goal | Key work | Done when |
|---|---|---|---|
| **17. Global symbol index** | One merged view of all types/functions | Persist `SymbolDB` across TUs with stable USRs (`clang_getCursorUSR`); dedup declarations | Resolve a function defined in another file |
| **18. Call graph** | Know who calls whom, across files | Build edges from `call_sites` to resolved callees by USR | "Callers of f()" spans files |
| **19. Cross-TU rules** | Rules that reason globally | e.g. hot function takes a big object by value, called 1M× from three files; propagate cost through the graph | A finding impossible to produce from a single file |

**Pillar C done:** at least one finding that requires whole-program reasoning.

---

## Pillar D — Integration & operations

Makes it deployable across an organization, not just runnable.

| Phase | Goal | Key work |
|---|---|---|
| **20. Build-graph input** | Work without `compile_commands.json` | Adapter that extracts compile actions from Bazel `aquery` |
| **21. Autofix** | Apply suggested edits | Emit clang-style fixits; `--fix` writes them |
| **22. Suppression at scale** | Manage findings across a big repo | Inline `// NOLINT(PG002)`, per-directory configs, baseline as the default gate |
| **23. Distributed sharding** | Truly large-scale | Shard TUs across machines, merge symbol indexes |

---

## Release plan

Cut a release at the end of each pillar (or a meaningful sub-milestone), not after
every commit. Commit freely in between — only a version bump + tag triggers the
binary release.

| Version | Cut after | Why |
|---|---|---|
| **v0.2.1** (patch) | Phase 11 (mutation analysis) | Bundles the real-project parsing + the rule-accuracy fixes already on `main`; first build that is genuinely usable on outside projects |
| **v0.3.0** (minor) | Pillar A complete (Phases 12–13) | Trustworthy results: confidence levels + precise PG001/PG002 |
| **v0.4.0** (minor) | Pillar B complete (Phases 14–16) | Fast enough for large repos: parallel + incremental |
| **v1.0.0** (major) | Pillar C complete (Phases 17–19) | Whole-program analysis — the milestone that makes it a "real" tool |
| **v1.x** | Pillar D milestones | Bazel, autofix, scale-out, as each lands |

**Release mechanics:** bump `core/include/perfguardian/version.hpp` + `CMakeLists.txt`
→ commit → `git tag vX.Y.Z` → push tag → release CI builds all four platform binaries
→ refresh the Homebrew formula and Scoop manifest SHA256 hashes.

---

## Current status

- ✅ Phases 0–10 complete
- ✅ Real multi-file project parsing (compile_commands.json + inferred includes + partial recovery)
- ✅ Rule-accuracy fixes (PG002/005/006 false positives, PG006 missed detection)
- ✅ **Phase 11 — mutation analysis** (PG002 false positives 58 → 4 on self-scan)
- ✅ **Phase 12 — move-only types + reference capture** (PG002 → 2, PG001 → 3; remaining findings legitimate)
- ✅ **Phase 13 — confidence levels** (`--min-confidence`, per-rule High/Medium/Low) — **Pillar A complete**
- ✅ **Phase 13b — skip macro misparses** (leveldb: 58 phantom `TEST_F` findings removed)
- ✅ **Phase 14 — parallel parsing** (leveldb: 80 s → 6 s, deterministic)
- ⏳ **Next: cut v0.3.0**, then Phase 15 (incremental cache)
