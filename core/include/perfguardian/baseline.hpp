#pragma once
#include "perfguardian/diagnostic.hpp"
#include <string>
#include <vector>

namespace perfguardian {

struct BaselineIssue {
    std::string rule_id;
    std::string file;
    int         line    = 0;
    std::string message;
    Severity    severity = Severity::Info;
};

struct BaselineDiff {
    std::vector<BaselineIssue> new_issues;
    std::vector<BaselineIssue> fixed_issues;
    std::vector<BaselineIssue> persisting_issues;

    int new_count()        const { return static_cast<int>(new_issues.size());        }
    int fixed_count()      const { return static_cast<int>(fixed_issues.size());      }
    int persisting_count() const { return static_cast<int>(persisting_issues.size()); }
};

// Load a previous JSON report produced by write_json_report() and return its
// diagnostics as BaselineIssue entries.  Throws std::runtime_error on parse failure.
std::vector<BaselineIssue> load_baseline(const std::string& path);

// Diff current DiagnosticSink against a baseline loaded with load_baseline().
// Fingerprint: rule_id + file + message  (line-number-stable across edits).
BaselineDiff diff_baseline(const DiagnosticSink& current,
                            const std::vector<BaselineIssue>& baseline);

// Print a human-readable diff summary to stdout.
void print_baseline_diff(const BaselineDiff& diff);

}  // namespace perfguardian
