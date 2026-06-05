#pragma once
#include "perfguardian/symbol_db.hpp"
#include "perfguardian/diagnostic.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace perfguardian {

// Per-run tuning knobs; all rules read only the fields they care about.
struct RuleConfig {
    // PG001: flag by-value parameters whose type exceeds this size (bytes).
    int pg001_size_threshold_bytes = 16;
};

// Interface every rule must implement.
class IRule {
public:
    virtual ~IRule() = default;
    virtual void             run(const SymbolDB& db, DiagnosticSink& sink,
                                 const RuleConfig& cfg) const = 0;
    virtual std::string_view rule_id()   const = 0;
    virtual std::string_view rule_name() const = 0;
};

// Build the default rule set (all enabled rules).
std::vector<std::unique_ptr<IRule>> make_default_rules();

// Run every rule in the set and collect results into sink.
void run_rules(const SymbolDB& db,
               DiagnosticSink& sink,
               const std::vector<std::unique_ptr<IRule>>& rules,
               const RuleConfig& cfg = {});

}  // namespace perfguardian
