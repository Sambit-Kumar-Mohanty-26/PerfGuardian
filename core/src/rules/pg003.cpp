#include "perfguardian/pg003.hpp"
#include <algorithm>

namespace perfguardian {

void RulePG003::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& /*cfg*/) const {
    for (const auto& fn : db.functions()) {
        // Check if there is any push_back / emplace_back inside a loop
        bool has_push_in_loop = false;
        int  push_line        = 0;
        for (const auto& cs : fn.call_sites) {
            if (cs.inside_loop &&
                (cs.callee == "push_back" || cs.callee == "emplace_back")) {
                has_push_in_loop = true;
                push_line        = cs.line;
                break;
            }
        }
        if (!has_push_in_loop) continue;

        // If the function also calls reserve() anywhere, it's fine
        bool has_reserve = std::any_of(fn.call_sites.begin(), fn.call_sites.end(),
            [](const CallSite& cs) { return cs.callee == "reserve"; });
        if (has_reserve) continue;

        Diagnostic d;
        d.rule_id   = std::string(rule_id());
        d.rule_name = std::string(rule_name());
        d.severity      = Severity::Medium;
        d.confidence    = Confidence::Medium;  // loop iteration count unknown
        d.location      = {fn.file, push_line > 0 ? push_line : fn.line, fn.column};
        d.function_name = fn.qualified_name;
        d.message   = "push_back inside loop in '" + fn.qualified_name +
                      "' without a prior reserve() — repeated reallocations may"
                      " copy all elements on each growth";
        d.suggested_fix = "Call vec.reserve(expected_size) before the loop";

        sink.emit(std::move(d));
    }
}

}  // namespace perfguardian
