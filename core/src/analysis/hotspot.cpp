#include "perfguardian/hotspot.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace perfguardian {

AnalysisReport rank_hotspots(const DiagnosticSink& sink, int top_n) {
    AnalysisReport report;
    report.total_issues = static_cast<int>(sink.count());

    // Accumulate per-function, per-file, and per-rule stats
    struct FnAcc {
        std::string file;
        int         line        = 0;
        int         issue_count = 0;
        int         score       = 0;
        std::vector<std::string> rule_ids;
    };

    std::unordered_map<std::string, FnAcc>  fn_map;   // function_name → acc
    std::unordered_map<std::string, FileHotspot> file_map; // file → acc
    std::unordered_map<std::string, RuleSummary> rule_map; // rule_id → summary

    for (const auto& d : sink.all()) {
        const int w = severity_weight(d.severity);
        report.total_severity_score += w;

        // Function accumulator
        auto& fa = fn_map[d.function_name];
        fa.file = d.location.file;
        if (fa.line == 0) fa.line = d.location.line;
        ++fa.issue_count;
        fa.score += w;
        // Append rule_id only if not already present
        if (std::find(fa.rule_ids.begin(), fa.rule_ids.end(), d.rule_id)
                == fa.rule_ids.end()) {
            fa.rule_ids.push_back(d.rule_id);
        }

        // File accumulator
        auto& fh = file_map[d.location.file];
        fh.file = d.location.file;
        ++fh.issue_count;
        fh.score += w;

        // Rule accumulator
        auto& rs = rule_map[d.rule_id];
        rs.rule_id   = d.rule_id;
        rs.rule_name = d.rule_name;
        ++rs.count;
    }

    // Build sorted function hotspot list
    for (auto& [name, fa] : fn_map) {
        FunctionHotspot fh;
        fh.function_name = name;
        fh.file          = fa.file;
        fh.line          = fa.line;
        fh.issue_count   = fa.issue_count;
        fh.score         = fa.score;
        fh.rule_ids      = std::move(fa.rule_ids);
        report.top_functions.push_back(std::move(fh));
    }
    std::sort(report.top_functions.begin(), report.top_functions.end(),
        [](const FunctionHotspot& a, const FunctionHotspot& b) {
            return a.score != b.score ? a.score > b.score
                                      : a.issue_count > b.issue_count;
        });

    // Build sorted file hotspot list
    for (auto& [path, fh] : file_map) {
        report.top_files.push_back(std::move(fh));
    }
    std::sort(report.top_files.begin(), report.top_files.end(),
        [](const FileHotspot& a, const FileHotspot& b) {
            return a.score != b.score ? a.score > b.score
                                      : a.issue_count > b.issue_count;
        });

    // Build sorted rule summary list
    for (auto& [id, rs] : rule_map) {
        report.rule_summary.push_back(std::move(rs));
    }
    std::sort(report.rule_summary.begin(), report.rule_summary.end(),
        [](const RuleSummary& a, const RuleSummary& b) {
            return a.count != b.count ? a.count > b.count : a.rule_id < b.rule_id;
        });

    // Trim to top_n if requested
    if (top_n > 0) {
        if (static_cast<int>(report.top_functions.size()) > top_n)
            report.top_functions.resize(static_cast<std::size_t>(top_n));
        if (static_cast<int>(report.top_files.size()) > top_n)
            report.top_files.resize(static_cast<std::size_t>(top_n));
    }

    return report;
}

void print_report(const AnalysisReport& report,
                  const SymbolDB& /*db*/,
                  int top_n) {
    if (report.total_issues == 0) {
        std::cout << "\nNo issues found.\n";
        return;
    }

    std::cout << "\n" << report.total_issues << " issue(s) found"
              << "  (severity score: " << report.total_severity_score << ")\n";

    // Rule summary
    std::cout << "\nBy rule:\n";
    for (const auto& rs : report.rule_summary) {
        std::cout << "  " << rs.rule_id << "  " << rs.rule_name
                  << "  — " << rs.count << " issue(s)\n";
    }

    // Top functions
    int shown = 0;
    std::cout << "\nTop functions:\n";
    for (const auto& fh : report.top_functions) {
        if (top_n > 0 && shown >= top_n) break;
        std::cout << "  [score=" << fh.score << "]  "
                  << fh.function_name
                  << "  (" << fh.issue_count << " issue(s))  "
                  << fs::path(fh.file).filename().string()
                  << ":" << fh.line << "\n";
        ++shown;
    }

    // Top files
    shown = 0;
    std::cout << "\nTop files:\n";
    for (const auto& fh : report.top_files) {
        if (top_n > 0 && shown >= top_n) break;
        std::cout << "  [score=" << fh.score << "]  "
                  << fs::path(fh.file).filename().string()
                  << "  (" << fh.issue_count << " issue(s))\n";
        ++shown;
    }
}

}  // namespace perfguardian
