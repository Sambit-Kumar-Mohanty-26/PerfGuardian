#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>
#include <filesystem>
#include "perfguardian/version.hpp"
#include "perfguardian/project_loader.hpp"
#include "perfguardian/symbol_db.hpp"
#include "perfguardian/rules.hpp"
#include "perfguardian/pg001.hpp"
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/severity.hpp"
#include "perfguardian/hotspot.hpp"
#include "perfguardian/json_report.hpp"
#include "perfguardian/html_report.hpp"
#include "perfguardian/config.hpp"
#include "perfguardian/sarif_report.hpp"
#include "perfguardian/baseline.hpp"

#ifdef PERFGUARDIAN_CLANG_ENABLED
#include "perfguardian/clang_parser.hpp"
#endif

namespace fs = std::filesystem;

// helpers

static std::string truncate(const std::string& s, std::size_t max_len = 60) {
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len - 3) + "...";
}

// subcommand handlers

static int cmd_list_rules() {
    std::cout << "PerfGuardian " << perfguardian::version_str << " — Rule Catalog\n";
    std::cout << "─────────────────────────────────────────────────────────\n";
    auto rules = perfguardian::make_default_rules();
    for (const auto& r : rules) {
        std::cout << "  " << r->rule_id() << "  " << r->rule_name() << "\n";
    }
    std::cout << "\nPlanned (Phase 5+):\n";
    std::cout << "  (hotspot ranker, JSON/HTML/SARIF reports, .yaml config)\n";
    return 0;
}

static int cmd_dump_ast(const std::string& filepath) {
#ifndef PERFGUARDIAN_CLANG_ENABLED
    std::cerr << "dump-ast: Clang integration not enabled in this build.\n";
    std::cerr << "Rebuild with -DPERFGUARDIAN_ENABLE_CLANG=ON\n";
    return 1;
#else
    if (!fs::exists(filepath)) {
        std::cerr << "File not found: " << filepath << "\n";
        return 1;
    }

    std::cout << "Parsing: " << filepath << "\n";

    // Try to load compile_commands.json from the same directory or parent
    std::vector<std::string> compile_args;
    fs::path file_dir = fs::path(filepath).parent_path();
    try {
        auto cmds = perfguardian::load_compile_commands(file_dir.string());
        for (const auto& cmd : cmds) {
            // Normalize paths for comparison
            fs::path cmd_path = fs::path(cmd.file).lexically_normal();
            fs::path req_path = fs::path(filepath).lexically_normal();
            if (cmd_path == req_path && cmd.arguments.size() > 1) {
                // Skip argv[0] (compiler name)
                compile_args.assign(cmd.arguments.begin() + 1, cmd.arguments.end());
                break;
            }
        }
    } catch (...) {
        // No compile_commands.json — use default C++20 flags
        compile_args = {"-std=c++20"};
    }

    auto result = perfguardian::parse_file(filepath, compile_args);

    if (!result.ok) {
        std::cerr << "Parse errors:\n";
        for (const auto& e : result.errors) {
            std::cerr << "  " << e << "\n";
        }
        return 1;
    }

    // Print types
    if (!result.types.empty()) {
        std::cout << "\nTypes (" << result.types.size() << "):\n";
        for (const auto& t : result.types) {
            std::cout << "  struct/class " << t.qualified_name;
            if (t.size_bytes >= 0) {
                std::cout << "  [" << t.size_bytes << " bytes]";
            }
            std::cout << "  @ " << fs::path(t.file).filename().string()
                      << ":" << t.line << "\n";
        }
    }

    // Print functions
    if (!result.functions.empty()) {
        std::cout << "\nFunctions (" << result.functions.size() << "):\n";
        for (const auto& fn : result.functions) {
            std::cout << "  " << truncate(fn.qualified_name)
                      << "  @ " << fs::path(fn.file).filename().string()
                      << ":" << fn.line << "\n";

            for (const auto& p : fn.params) {
                std::string qualifiers;
                if (p.is_rvalue_ref) qualifiers = "&&";
                else if (p.is_reference) qualifiers = "&";
                else if (p.is_pointer)  qualifiers = "*";

                std::string size_str;
                if (!p.is_reference && !p.is_pointer && !p.is_rvalue_ref) {
                    size_str = " [" + std::to_string(p.type_size_bytes) + " bytes]";
                }

                std::cout << "    param: " << (p.name.empty() ? "(unnamed)" : p.name)
                          << " : " << truncate(p.type_spelling, 40)
                          << qualifiers << size_str << "\n";
            }
        }
    }

    std::cout << "\nDone.\n";
    return 0;
#endif
}

static int cmd_analyze(const std::string& path,
                       const std::string& json_out,
                       const std::string& html_out,
                       const std::string& sarif_out,
                       const std::string& fail_on,
                       const std::string& baseline_path) {
    spdlog::info("PerfGuardian {} — starting analysis", perfguardian::version_str);
    std::cout << "PerfGuardian " << perfguardian::version_str << "\n";

    if (!fs::exists(path)) {
        std::cerr << "Path not found: " << path << "\n";
        return 1;
    }

    // Phase 1: enumerate sources
    std::vector<std::string> sources;
    std::vector<perfguardian::CompileCommand> compile_commands;

    bool has_compile_commands = false;
    try {
        compile_commands = perfguardian::load_compile_commands(path);
        has_compile_commands = true;
        std::cout << "Loaded compile_commands.json: "
                  << compile_commands.size() << " translation units\n";
    } catch (...) {
        sources = perfguardian::enumerate_sources(path);
        std::cout << "No compile_commands.json — found "
                  << sources.size() << " source files\n";
    }

#ifdef PERFGUARDIAN_CLANG_ENABLED
    if (has_compile_commands) {
        for (const auto& cmd : compile_commands) {
            sources.push_back(cmd.file);
        }
    }

    // Phase 1: parse all translation units
    perfguardian::SymbolDB db;
    int parsed = 0, failed = 0;
    for (const auto& src : sources) {
        std::vector<std::string> args = {"-std=c++20"};
        if (has_compile_commands) {
            for (const auto& cmd : compile_commands) {
                if (cmd.file == src && cmd.arguments.size() > 1) {
                    args.assign(cmd.arguments.begin() + 1, cmd.arguments.end());
                    break;
                }
            }
        }
        auto result = perfguardian::parse_file(src, args);
        if (result.ok) {
            ++parsed;
            db.add(result);
            std::cout << "  [ok]   " << fs::path(src).filename().string()
                      << "  (" << result.functions.size() << " fns, "
                      << result.types.size() << " types)\n";
        } else {
            ++failed;
            std::cout << "  [err]  " << fs::path(src).filename().string() << "\n";
        }
    }
    std::cout << "\nParsed: " << parsed << " ok, " << failed << " failed"
              << "  |  DB: " << db.function_count() << " functions, "
              << db.type_count() << " types\n";

    // Phase 8: load .perfguardian.yaml config (walk up from analysis path)
    auto cfg_path = perfguardian::find_config(path);
    perfguardian::PerfGuardianConfig cfg;
    if (!cfg_path.empty()) {
        std::cout << "Config: " << cfg_path << "\n";
        cfg = perfguardian::load_config(cfg_path);
    }
    auto rule_cfg = cfg.to_rule_config();

    // Phases 3-5: run rule engine + hotspot ranker
    auto rules = cfg.filter_rules(perfguardian::make_default_rules());
    perfguardian::DiagnosticSink sink;
    perfguardian::run_rules(db, sink, rules, rule_cfg);

    // Phase 8: apply suppressions after running rules
    cfg.apply_suppressions(sink);

    auto report = perfguardian::rank_hotspots(sink);
    perfguardian::print_report(report, db);

    // Phase 6: JSON report
    if (!json_out.empty()) {
        try {
            perfguardian::write_json_report(json_out, report, sink);
            std::cout << "\nJSON report written to: " << json_out << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << "\n";
        }
    }

    // Phase 7: HTML dashboard
    if (!html_out.empty()) {
        try {
            perfguardian::write_html_report(html_out, report, sink);
            std::cout << "HTML dashboard written to: " << html_out << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << "\n";
        }
    }

    // Phase 9: SARIF report
    if (!sarif_out.empty()) {
        try {
            perfguardian::write_sarif_report(sarif_out, report, sink, path);
            std::cout << "SARIF report written to: " << sarif_out << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << "\n";
        }
    }

    // Phase 10: baseline diff
    bool has_new_issues = false;
    if (!baseline_path.empty()) {
        try {
            auto baseline = perfguardian::load_baseline(baseline_path);
            auto diff = perfguardian::diff_baseline(sink, baseline);
            perfguardian::print_baseline_diff(diff);
            has_new_issues = diff.new_count() > 0;
        } catch (const std::exception& e) {
            std::cerr << "Warning: baseline diff failed: " << e.what() << "\n";
        }
    }

    // Detailed issue list
    if (!sink.empty()) {
        std::cout << "\nAll issues:\n";
        std::cout << "─────────────────────────────────────────────────────────\n";
        for (const auto& d : sink.all()) {
            std::cout << "[" << d.rule_id << "] "
                      << fs::path(d.location.file).filename().string()
                      << ":" << d.location.line
                      << "  " << d.message << "\n"
                      << "  fix: " << d.suggested_fix << "\n";
        }
    }

    // Honour --fail-on severity threshold.
    // With --baseline, fail only when there are NEW issues at that severity.
    if (!fail_on.empty() && !sink.empty()) {
        try {
            auto min_sev = perfguardian::severity_from_string(fail_on);
            if (!baseline_path.empty()) {
                if (has_new_issues) return 1;
            } else {
                if (!sink.with_severity(min_sev).empty()) return 1;
            }
        } catch (...) {
            std::cerr << "Unknown severity '" << fail_on
                      << "'. Valid: info low medium high critical\n";
            return 2;
        }
    }
#else
    std::cout << "(Clang integration not enabled — rebuild with -DPERFGUARDIAN_ENABLE_CLANG=ON)\n";
#endif

    return 0;
}

// main

int main(int argc, char** argv) {
    CLI::App app{"C++ performance analyzer and automated code reviewer"};
    app.name("perfguardian");
    app.set_version_flag("-V,--version", perfguardian::version_string());
    app.require_subcommand(0, 1);

    // analyze 
    auto* analyze_cmd = app.add_subcommand("analyze", "Analyze a C++ project for performance issues");
    std::string analyze_path = ".";
    std::string json_out, html_out, sarif_out, fail_on, baseline;

    analyze_cmd->add_option("path", analyze_path, "Project directory to analyze")->default_val(".");
    analyze_cmd->add_option("--json",     json_out,  "Write JSON report to FILE");
    analyze_cmd->add_option("--html",     html_out,  "Write HTML dashboard to FILE");
    analyze_cmd->add_option("--sarif",    sarif_out, "Write SARIF report to FILE");
    analyze_cmd->add_option("--fail-on",  fail_on,   "Exit non-zero if any issue at or above SEVERITY");
    analyze_cmd->add_option("--baseline", baseline,  "Compare against a previous JSON report");

    // list-rules 
    auto* list_rules_cmd = app.add_subcommand("list-rules", "List all available analysis rules");

    // dump-ast
    auto* dump_ast_cmd = app.add_subcommand(
        "dump-ast", "Dump AST of a source file (requires compile_commands.json)");
    std::string dump_ast_file;
    dump_ast_cmd->add_option("file", dump_ast_file, "Source file to parse")->required();

    CLI11_PARSE(app, argc, argv);

    if (*analyze_cmd) {
        return cmd_analyze(analyze_path, json_out, html_out, sarif_out, fail_on, baseline);
    }
    if (*list_rules_cmd) {
        return cmd_list_rules();
    }
    if (*dump_ast_cmd) {
        return cmd_dump_ast(dump_ast_file);
    }

    std::cout << app.help();
    return 0;
}
