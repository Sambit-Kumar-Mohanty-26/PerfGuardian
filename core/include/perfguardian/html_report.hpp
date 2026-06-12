#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/hotspot.hpp"
#include <string>

namespace perfguardian {

// Generate a self-contained HTML dashboard (embedded CSS + JS, no CDN deps).
std::string to_html_string(const AnalysisReport& report,
                            const DiagnosticSink& sink);

// Write the HTML dashboard to a file. Throws std::runtime_error on I/O failure.
void write_html_report(const std::string& path,
                       const AnalysisReport& report,
                       const DiagnosticSink& sink);

}  // namespace perfguardian
