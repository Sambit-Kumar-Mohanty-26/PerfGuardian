# PerfGuardian — Architecture

## Component Map

```
perfguardian/
├── cli/                    # Entry point, argument parsing, subcommand dispatch
├── core/
│   ├── include/            # Public headers
│   ├── project_loader/     # Reads compile_commands.json, enumerates TUs   [Phase 1]
│   ├── parser/             # Clang LibTooling FrontendAction + ASTVisitor   [Phase 1]
│   ├── symbols/            # SymbolDB: types, functions, sizes, call graph  [Phase 2]
│   ├── rules/              # Rule interface + per-rule implementations       [Phase 3+]
│   ├── metrics/            # MetricsAggregator, hotspot ranker              [Phase 5]
│   ├── suggestions/        # Code fix text generator                        [Phase 3]
│   └── diagnostics/        # Diagnostic struct, sink, severity              [Phase 3]
├── report/
│   ├── json/               # JSON schema emitter                            [Phase 6]
│   ├── html/               # Self-contained HTML dashboard                  [Phase 7]
│   └── sarif/              # SARIF 2.1.0 emitter                           [Phase 9]
├── config/                 # .perfguardian.yaml loader                      [Phase 8]
├── ci/                     # GitHub Action action.yml, sample workflow      [Phase 9]
├── samples/                # Demo projects with known issues
├── tests/                  # Unit + integration tests
└── docs/                   # This file, JSON schema spec, rule catalog
```

## Data Flow

```
Source files
    │
    ▼
compile_commands.json ──► ProjectLoader
                               │ translation units
                               ▼
                          ClangParser (LibTooling)
                               │ AST nodes
                               ▼
                          SymbolDB
                          ├─ TypeInfo (name, size, trivially_copyable)
                          ├─ FunctionInfo (params, return type, location)
                          └─ CallGraph (caller → callee edges)
                               │
                               ▼
                          RuleEngine  ◄──── Rule registry
                               │ RuleMatch[]
                               ▼
                          MetricsEngine
                               │ Diagnostic[] with quantified metrics
                               ▼
                          SuggestionEngine
                               │ Diagnostic[] with suggested_fix text
                               ▼
                     ┌─────────┴─────────┐
                     ▼         ▼         ▼
                  Terminal   JSON      HTML/SARIF
```

## Severity Levels

| Level    | Exit code contribution | Typical rule examples           |
|----------|------------------------|---------------------------------|
| Info     | 0                      | Style notes                     |
| Low      | 0 (default)            | Minor inefficiencies            |
| Medium   | 0 (default)            | Missing reserve(), const&       |
| High     | 1 (with --fail-on high)| Large copies, find-in-loop      |
| Critical | 1 always               | Memory leaks, data races        |

## Rule Interface (Phase 3+)

```cpp
class Rule {
public:
    virtual ~Rule() = default;
    virtual std::string_view id() const = 0;
    virtual std::string_view name() const = 0;
    virtual Severity default_severity() const = 0;
    virtual void check(const SymbolDB& db, DiagnosticSink& sink) const = 0;
};
```

Rules are registered at startup via a factory and run sequentially by the RuleEngine.
Each rule is fully self-contained in `core/rules/<rule_id>.cpp`.
