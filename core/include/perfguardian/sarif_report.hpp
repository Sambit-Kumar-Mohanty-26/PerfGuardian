#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/hotspot.hpp"
#include <string>

namespace perfguardian {

// Produce a SARIF 2.1.0 JSON string from analysis results.
// repo_root is used to make artifact URIs relative (pass "" to use absolute paths).
std::string to_sarif_string(const AnalysisReport& report,
                             const DiagnosticSink& sink,
                             const std::string& repo_root = "");

// Write SARIF to a file; throws std::runtime_error on I/O failure.
void write_sarif_report(const std::string& path,
                        const AnalysisReport& report,
                        const DiagnosticSink& sink,
                        const std::string& repo_root = "");

}  // namespace perfguardian
