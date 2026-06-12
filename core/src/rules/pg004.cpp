#include "perfguardian/pg004.hpp"
#include <unordered_set>
#include <string>

namespace perfguardian {

static const std::unordered_set<std::string> kLookupCallees = {
    "find", "count", "contains", "lower_bound", "upper_bound", "equal_range"
};

void RulePG004::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& /*cfg*/) const {
    for (const auto& fn : db.functions()) {
        for (const auto& cs : fn.call_sites) {
            if (!cs.inside_loop) continue;
            if (kLookupCallees.find(cs.callee) == kLookupCallees.end()) continue;

            Diagnostic d;
            d.rule_id   = std::string(rule_id());
            d.rule_name = std::string(rule_name());
            d.severity      = Severity::High;
            d.location      = {fn.file, cs.line > 0 ? cs.line : fn.line, fn.column};
            d.function_name = fn.qualified_name;
            d.message   = "'" + cs.callee + "' called inside loop in '" +
                          fn.qualified_name + "' — repeated container lookups"
                          " compound to O(N²) or O(N log N)";
            d.suggested_fix = "Hoist the lookup before the loop or cache the result";
            d.metrics.loop_depth = cs.loop_depth;

            sink.emit(std::move(d));
        }
    }
}

}  // namespace perfguardian
