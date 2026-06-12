#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/hotspot.hpp"
#include <string>

namespace perfguardian {

// Serialise the full analysis result to a pretty-printed JSON string.
// The JSON schema is:
//   {
//     "version":     "0.1.0",
//     "timestamp":   "2026-06-12T14:30:00Z",
//     "summary":     { total_issues, total_severity_score, by_rule:[...] },
//     "hotspots":    { functions:[...], files:[...] },
//     "diagnostics": [ { rule_id, severity, function_name, location,
//                         message, suggested_fix, metrics }, ... ]
//   }
std::string to_json_string(const AnalysisReport& report,
                            const DiagnosticSink& sink);

// Write the JSON report to a file.  Throws std::runtime_error on I/O failure.
void write_json_report(const std::string& path,
                       const AnalysisReport& report,
                       const DiagnosticSink& sink);

}  // namespace perfguardian
