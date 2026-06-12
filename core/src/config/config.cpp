#include "perfguardian/config.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

namespace perfguardian {

// glob matching
// Supports * (matches any sequence of chars) and ? (matches one char).

static bool glob_match(std::string_view pattern, std::string_view text) {
    if (pattern.empty())       return text.empty();
    if (pattern == "*")        return true;
    if (pattern[0] == '*') {
        for (std::size_t i = 0; i <= text.size(); ++i) {
            if (glob_match(pattern.substr(1), text.substr(i))) return true;
        }
        return false;
    }
    if (text.empty())          return false;
    if (pattern[0] == '?' || pattern[0] == text[0]) {
        return glob_match(pattern.substr(1), text.substr(1));
    }
    return false;
}

static bool suppression_matches(const Suppression& s, const Diagnostic& d) {
    // Rule match
    if (s.rule != "*" && s.rule != d.rule_id) return false;
    // Function match
    if (!s.function.empty() && !glob_match(s.function, d.function_name))
        return false;
    // File match
    if (!s.file.empty() && !glob_match(s.file, d.location.file) &&
        !glob_match(s.file, fs::path(d.location.file).filename().string()))
        return false;
    return true;
}

// PerfGuardianConfig methods

bool PerfGuardianConfig::rule_enabled(const std::string& rule_id) const {
    auto it = rule_overrides.find(rule_id);
    return (it == rule_overrides.end()) ? true : it->second.enabled;
}

RuleConfig PerfGuardianConfig::to_rule_config() const {
    RuleConfig cfg;
    auto apply = [&](const std::string& id, auto fn) {
        auto it = rule_overrides.find(id);
        if (it != rule_overrides.end()) fn(it->second, cfg);
    };
    apply("PG001", [](const RuleOverride& o, RuleConfig& c) {
        if (o.size_threshold > 0) c.pg001_size_threshold_bytes = o.size_threshold;
    });
    apply("PG002", [](const RuleOverride& o, RuleConfig& c) {
        if (o.size_threshold > 0) c.pg002_size_threshold_bytes = o.size_threshold;
    });
    apply("PG005", [](const RuleOverride& o, RuleConfig& c) {
        if (o.copy_size_threshold > 0) c.pg005_copy_size_threshold = o.copy_size_threshold;
    });
    apply("PG006", [](const RuleOverride& o, RuleConfig& c) {
        if (o.min_repeat_count > 0) c.pg006_min_repeat_count = o.min_repeat_count;
    });
    return cfg;
}

void PerfGuardianConfig::apply_suppressions(DiagnosticSink& sink) const {
    if (suppressions.empty()) return;
    sink.remove_if([this](const Diagnostic& d) {
        for (const auto& s : suppressions) {
            if (suppression_matches(s, d)) return true;
        }
        return false;
    });
}

std::vector<std::unique_ptr<IRule>>
PerfGuardianConfig::filter_rules(std::vector<std::unique_ptr<IRule>> rules) const {
    rules.erase(
        std::remove_if(rules.begin(), rules.end(),
            [this](const std::unique_ptr<IRule>& r) {
                return !rule_enabled(std::string(r->rule_id()));
            }),
        rules.end());
    return rules;
}

// ── YAML parsing ──────────────────────────────────────────────────────────────

PerfGuardianConfig parse_config(const std::string& yaml_content) {
    PerfGuardianConfig cfg;
    YAML::Node root;
    try {
        root = YAML::Load(yaml_content);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(
            std::string("YAML parse error: ") + e.what());
    }

    if (root["version"] && root["version"].IsScalar()) {
        cfg.schema_version = root["version"].as<int>(1);
    }

    // rules section
    if (root["rules"] && root["rules"].IsMap()) {
        for (const auto& kv : root["rules"]) {
            std::string rule_id = kv.first.as<std::string>();
            RuleOverride ovr;
            const auto& node = kv.second;
            if (node["enabled"])            ovr.enabled             = node["enabled"].as<bool>(true);
            if (node["size_threshold_bytes"])  ovr.size_threshold      = node["size_threshold_bytes"].as<int>(-1);
            if (node["copy_size_threshold"])   ovr.copy_size_threshold = node["copy_size_threshold"].as<int>(-1);
            if (node["min_repeat_count"])      ovr.min_repeat_count    = node["min_repeat_count"].as<int>(-1);
            cfg.rule_overrides[rule_id] = ovr;
        }
    }

    // suppressions section
    if (root["suppressions"] && root["suppressions"].IsSequence()) {
        for (const auto& entry : root["suppressions"]) {
            Suppression s;
            if (entry["rule"])     s.rule     = entry["rule"].as<std::string>();
            if (entry["function"]) s.function = entry["function"].as<std::string>();
            if (entry["file"])     s.file      = entry["file"].as<std::string>();
            if (!s.rule.empty()) {
                cfg.suppressions.push_back(std::move(s));
            }
        }
    }

    return cfg;
}

PerfGuardianConfig load_config(const std::string& path) {
    if (path.empty() || !fs::exists(path)) {
        return {};  // default config
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    return parse_config(content);
}

std::string find_config(const std::string& start_dir) {
    fs::path dir = fs::absolute(start_dir);
    while (true) {
        fs::path candidate = dir / ".perfguardian.yaml";
        if (fs::exists(candidate)) return candidate.string();
        fs::path parent = dir.parent_path();
        if (parent == dir) break;  // reached filesystem root
        dir = parent;
    }
    return {};
}

}  // namespace perfguardian
