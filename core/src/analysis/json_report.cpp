#include "perfguardian/json_report.hpp"
#include "perfguardian/version.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace perfguardian {

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string iso8601_now() {
    auto now   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// serialisation

std::string to_json_string(const AnalysisReport& report,
                            const DiagnosticSink& sink) {
    using json = nlohmann::json;

    // summary.by_rule
    json by_rule = json::array();
    for (const auto& rs : report.rule_summary) {
        by_rule.push_back({
            {"rule_id",   rs.rule_id},
            {"rule_name", rs.rule_name},
            {"count",     rs.count}
        });
    }

    // hotspots.functions
    json fn_hotspots = json::array();
    for (const auto& fh : report.top_functions) {
        json rids = json::array();
        for (const auto& r : fh.rule_ids) rids.push_back(r);
        fn_hotspots.push_back({
            {"function_name", fh.function_name},
            {"file",          fh.file},
            {"line",          fh.line},
            {"issue_count",   fh.issue_count},
            {"score",         fh.score},
            {"rule_ids",      rids}
        });
    }

    // hotspots.files
    json file_hotspots = json::array();
    for (const auto& fh : report.top_files) {
        file_hotspots.push_back({
            {"file",        fh.file},
            {"issue_count", fh.issue_count},
            {"score",       fh.score}
        });
    }

    // diagnostics array
    json diags = json::array();
    for (const auto& d : sink.all()) {
        diags.push_back({
            {"rule_id",       d.rule_id},
            {"rule_name",     d.rule_name},
            {"severity",      std::string(to_string(d.severity))},
            {"confidence",    std::string(to_string(d.confidence))},
            {"function_name", d.function_name},
            {"location", {
                {"file",   d.location.file},
                {"line",   d.location.line},
                {"column", d.location.column}
            }},
            {"message",       d.message},
            {"suggested_fix", d.suggested_fix},
            {"metrics", {
                {"type_size_bytes", d.metrics.type_size_bytes},
                {"copy_cost_bytes", d.metrics.copy_cost_bytes},
                {"loop_depth",      d.metrics.loop_depth},
                {"lookup_count",    d.metrics.lookup_count}
            }}
        });
    }

    json root = {
        {"version",   version_str},
        {"timestamp", iso8601_now()},
        {"summary", {
            {"total_issues",         report.total_issues},
            {"total_severity_score", report.total_severity_score},
            {"by_rule",              by_rule}
        }},
        {"hotspots", {
            {"functions", fn_hotspots},
            {"files",     file_hotspots}
        }},
        {"diagnostics", diags}
    };

    return root.dump(2);
}

void write_json_report(const std::string& path,
                       const AnalysisReport& report,
                       const DiagnosticSink& sink) {
    std::string content = to_json_string(report, sink);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open JSON report file: " + path);
    }
    out << content;
    if (!out) {
        throw std::runtime_error("Failed to write JSON report to: " + path);
    }
}

}  // namespace perfguardian
