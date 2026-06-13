#include "perfguardian/pg005.hpp"

namespace perfguardian {

void RulePG005::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& cfg) const {
    const long long threshold = cfg.pg005_copy_size_threshold;

    for (const auto& fn : db.functions()) {
        for (const auto& lv : fn.local_vars) {
            if (lv.is_reference || lv.is_pointer) continue;
            // Only an avoidable copy if it's initialized from an existing object.
            // Default/value/direct construction is not a copy.
            if (!lv.is_copy_initialized) continue;
            const long long sz = lv.type_size_bytes;
            if (sz < 0 || sz <= threshold) continue;

            const std::string vname = lv.name.empty() ? "(unnamed)" : lv.name;

            Diagnostic d;
            d.rule_id   = std::string(rule_id());
            d.rule_name = std::string(rule_name());
            d.severity      = Severity::Low;
            d.location      = {fn.file, lv.line > 0 ? lv.line : fn.line, 0};
            d.function_name = fn.qualified_name;
            d.message   = "Local variable '" + vname + "' of type '" +
                          lv.type_spelling + "' is a by-value copy (" +
                          std::to_string(sz) + " bytes); consider 'const " +
                          lv.bare_type_spelling + "&' if mutation is not needed";
            d.suggested_fix       = "const " + lv.bare_type_spelling + "& " + vname;
            d.metrics.type_size_bytes = sz;
            d.metrics.copy_cost_bytes = sz;

            sink.emit(std::move(d));
        }
    }
}

}  // namespace perfguardian
