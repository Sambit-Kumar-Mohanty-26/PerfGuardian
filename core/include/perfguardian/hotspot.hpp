#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/symbol_db.hpp"
#include <string>
#include <vector>

namespace perfguardian {

// A function ranked by its combined diagnostic severity score.
struct FunctionHotspot {
    std::string              function_name;
    std::string              file;
    int                      line         = 0;
    int                      issue_count  = 0;
    int                      score        = 0;  // sum of severity_weight per issue
    std::vector<std::string> rule_ids;           // de-duplicated rule IDs that fired
};

// A source file ranked by its combined diagnostic severity score.
struct FileHotspot {
    std::string file;
    int         issue_count = 0;
    int         score       = 0;
};

// Per-rule issue count across the whole project.
struct RuleSummary {
    std::string rule_id;
    std::string rule_name;
    int         count = 0;
};

// Aggregated result of a full analysis run.
struct AnalysisReport {
    std::vector<FunctionHotspot> top_functions;   // sorted by score desc
    std::vector<FileHotspot>     top_files;        // sorted by score desc
    std::vector<RuleSummary>     rule_summary;     // sorted by count desc
    int                          total_issues        = 0;
    int                          total_severity_score = 0;
};

// Build an AnalysisReport by aggregating all diagnostics in sink.
// top_n: maximum entries in the ranked hotspot lists (0 = unlimited).
AnalysisReport rank_hotspots(const DiagnosticSink& sink, int top_n = 10);

// Print a human-readable hotspot summary to stdout.
void print_report(const AnalysisReport& report,
                  const SymbolDB& db,
                  int top_n = 5);

}  // namespace perfguardian
