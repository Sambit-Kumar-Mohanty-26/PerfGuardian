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

---

## Usage

```
perfguardian analyze <path> [options]     Analyze a C++ project
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
| `--baseline FILE` | Compare against a previous JSON report; with `--fail-on`, only **new** issues fail the run |

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

Run `perfguardian list-rules` for the live catalog.

### Example finding

```
[HIGH] src/game.cpp:45:12
  Rule: PG001 large-object-by-value
  Parameter 'p' of type 'Player' is 800 bytes, passed by value
  Suggested fix: const Player&
```

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
# On main, record the current state
perfguardian analyze . --json baseline.json

# On a PR, fail only if the PR introduces new findings
perfguardian analyze . --baseline baseline.json --fail-on high
```

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
C++ project + compile_commands.json
        │
        ▼
   Project loader ──► Clang AST parser (libclang)
        │
        ▼
   Symbol database  (types, sizes, functions, call sites)
        │
        ▼
   Rule engine  ──►  Hotspot ranker
        │
        ▼
   Reporters  ──►  text · JSON · HTML · SARIF
        │
        ▼
   CI gate (exit codes, baseline diff)
```

See [docs/architecture.md](docs/architecture.md) for the full design.

---

## License

MIT — see [LICENSE](LICENSE).
