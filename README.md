# PerfGuardian

**AI-assisted C++ performance analyzer and automated code reviewer.**

PerfGuardian analyzes entire C++ projects for performance bottlenecks, memory inefficiencies,
algorithmic complexity issues, and STL misuse — then quantifies the impact and suggests exact fixes.
It integrates with CI/CD pipelines to block inefficient code before merge.

---

## Quick Start

```bash
git clone https://github.com/your-username/PerfGuardian
cd PerfGuardian
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/perfguardian analyze samples/large-copy-demo/
```

---

## Features

| Feature | Status |
|---|---|
| Large object passed by value (PG001) | Phase 3 |
| Missing `const&` (PG002) | Phase 4 |
| Missing `reserve()` (PG003) | Phase 4 |
| `find()` inside loop (PG004) | Phase 4 |
| Repeated temporaries (PG005) | Phase 4 |
| Repeated map lookups (PG006) | Phase 4 |
| Whole-project aggregation | Phase 5 |
| JSON report | Phase 6 |
| HTML dashboard | Phase 7 |
| `.perfguardian.yaml` config | Phase 8 |
| SARIF + GitHub Actions | Phase 9 |
| Baseline comparison | Phase 10 |

---

## CLI Usage

```bash
perfguardian analyze .                         # Analyze current directory
perfguardian analyze . --html report.html      # + HTML dashboard
perfguardian analyze . --json report.json      # + JSON report
perfguardian analyze . --sarif report.sarif    # + SARIF (GitHub code scanning)
perfguardian analyze . --fail-on high          # Exit non-zero if HIGH+ issues found
perfguardian analyze . --baseline prev.json    # Compare against previous run
perfguardian list-rules                        # Show all available rules
perfguardian dump-ast src/game.cpp             # Dump Clang AST (debug)
```

---

## Example Output

```
[HIGH] src/game.cpp:45:12
  Rule: PG001 large-object-by-value
  Parameter 'p' of type 'Player' is 800 bytes, passed by value
  Estimated copy cost: 80 MB/sec at 100k calls/sec
  Suggested fix: const Player&
  Expected improvement: 10-30%
```

---

## Configuration

Create `.perfguardian.yaml` in your project root:

```yaml
size_threshold_bytes: 64
fail_on_severity: high
html_report: report.html
json_report: report.json
enabled_rules:
  - pass_by_value
  - reserve_vector
suppressed_paths:
  - third_party/
```

---

## Building from Source

**Requirements:**
- CMake 3.20+
- C++20 compiler (GCC 12+, Clang 16+, MSVC 2022+)
- LLVM/Clang development libraries (Phase 1+)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPERFGUARDIAN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

---

## Architecture

```
C++ Project
    |
compile_commands.json
    |
CLI Frontend (cli/)
    |
Project Loader --> Clang AST Parser
    |
Symbol Database (types, functions, sizes, call graph)
    |
Rule Engine --> Metrics Engine --> Suggestion Engine
    |
Report Generator (CLI / JSON / HTML / SARIF)
    |
CI/CD Integration (GitHub Actions, exit codes)
```

See [docs/architecture.md](docs/architecture.md) for the full design.

---

## Tech Stack

- **C++20** - core language
- **LLVM/Clang LibTooling** - AST parsing and type sizes
- **CMake** - build system with FetchContent deps
- **nlohmann/json** - JSON report emission
- **yaml-cpp** - config file parsing
- **spdlog** - structured logging
- **CLI11** - command-line argument parsing
- **GoogleTest** - unit and integration tests
- **GitHub Actions** - CI matrix (Ubuntu + macOS)

---

## License

MIT - see [LICENSE](LICENSE).
