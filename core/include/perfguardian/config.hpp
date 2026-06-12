#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/rules.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace perfguardian {

// Per-rule overrides loaded from .perfguardian.yaml.
struct RuleOverride {
    bool enabled             = true;
    int  size_threshold      = -1;  // PG001 / PG002: -1 → use RuleConfig default
    int  copy_size_threshold = -1;  // PG005
    int  min_repeat_count    = -1;  // PG006
};

// A single suppression entry: suppress rule in matching function/file.
// Empty pattern means "match anything".
struct Suppression {
    std::string rule;      // rule ID, or "*" to suppress all rules
    std::string function;  // glob pattern for function_name (empty = any)
    std::string file;      // glob pattern for location.file  (empty = any)
};

// Parsed content of a .perfguardian.yaml file.
struct PerfGuardianConfig {
    int                                        schema_version = 1;
    std::unordered_map<std::string, RuleOverride> rule_overrides;
    std::vector<Suppression>                   suppressions;

    // True if the given rule is enabled (defaults to true for unknown rules).
    bool rule_enabled(const std::string& rule_id) const;

    // Build a RuleConfig applying all numeric overrides.
    RuleConfig to_rule_config() const;

    // Remove diagnostics from sink that match any suppression entry.
    void apply_suppressions(DiagnosticSink& sink) const;

    // Filter a rule list by the enabled flags in this config.
    std::vector<std::unique_ptr<IRule>>
    filter_rules(std::vector<std::unique_ptr<IRule>> rules) const;
};

// Parse config from a YAML string (useful for testing without file I/O).
PerfGuardianConfig parse_config(const std::string& yaml_content);

// Load config from a file path. Returns a default config if the file does not
// exist; throws std::runtime_error if the file exists but cannot be parsed.
PerfGuardianConfig load_config(const std::string& path);

// Walk up from start_dir looking for .perfguardian.yaml.
// Returns the path of the first file found, or empty string if none.
std::string find_config(const std::string& start_dir);

}  // namespace perfguardian
