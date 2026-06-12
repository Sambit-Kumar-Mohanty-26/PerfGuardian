#include "perfguardian/baseline.hpp"
#include "perfguardian/severity.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

using json = nlohmann::json;

namespace perfguardian {

// fingerprint

static std::string fingerprint(const std::string& rule_id,
                                const std::string& file,
                                const std::string& message) {
    return rule_id + '\x01' + file + '\x01' + message;
}

// load_baseline

std::vector<BaselineIssue> load_baseline(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open())
        throw std::runtime_error("Cannot open baseline file: " + path);

    json root;
    try { root = json::parse(in); }
    catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("Baseline JSON parse error: ") + e.what());
    }

    std::vector<BaselineIssue> issues;
    if (!root.contains("diagnostics") || !root["diagnostics"].is_array())
        return issues;

    for (const auto& d : root["diagnostics"]) {
        BaselineIssue bi;
        bi.rule_id  = d.value("rule_id",  "");
        bi.message  = d.value("message",  "");
        bi.line     = 0;

        if (d.contains("location") && d["location"].is_object()) {
            bi.file = d["location"].value("file", "");
            bi.line = d["location"].value("line", 0);
        }

        std::string sev_str = d.value("severity", "info");
        try { bi.severity = severity_from_string(sev_str); }
        catch (...) { bi.severity = Severity::Info; }

        if (!bi.rule_id.empty())
            issues.push_back(std::move(bi));
    }
    return issues;
}

// diff_baseline

BaselineDiff diff_baseline(const DiagnosticSink& current,
                            const std::vector<BaselineIssue>& baseline) {
    // Build fingerprint sets
    std::unordered_set<std::string> baseline_fps;
    for (const auto& b : baseline)
        baseline_fps.insert(fingerprint(b.rule_id, b.file, b.message));

    std::unordered_set<std::string> current_fps;
    for (const auto& d : current.all())
        current_fps.insert(fingerprint(d.rule_id, d.location.file, d.message));

    BaselineDiff diff;

    for (const auto& d : current.all()) {
        std::string fp = fingerprint(d.rule_id, d.location.file, d.message);
        BaselineIssue bi{ d.rule_id, d.location.file, d.location.line,
                          d.message, d.severity };
        if (baseline_fps.count(fp))
            diff.persisting_issues.push_back(bi);
        else
            diff.new_issues.push_back(bi);
    }

    for (const auto& b : baseline) {
        std::string fp = fingerprint(b.rule_id, b.file, b.message);
        if (!current_fps.count(fp))
            diff.fixed_issues.push_back(b);
    }

    return diff;
}

// print_baseline_diff

void print_baseline_diff(const BaselineDiff& diff) {
    std::cout << "\n━━━ Baseline Diff ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  New issues      : " << diff.new_count()        << "\n";
    std::cout << "  Fixed issues    : " << diff.fixed_count()      << "\n";
    std::cout << "  Persisting      : " << diff.persisting_count() << "\n";

    if (!diff.new_issues.empty()) {
        std::cout << "\n[NEW]\n";
        for (const auto& i : diff.new_issues)
            std::cout << "  + [" << i.rule_id << "] " << i.file
                      << ":" << i.line << "  " << i.message << "\n";
    }
    if (!diff.fixed_issues.empty()) {
        std::cout << "\n[FIXED]\n";
        for (const auto& i : diff.fixed_issues)
            std::cout << "  - [" << i.rule_id << "] " << i.file
                      << ":" << i.line << "  " << i.message << "\n";
    }
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
}

}  // namespace perfguardian
