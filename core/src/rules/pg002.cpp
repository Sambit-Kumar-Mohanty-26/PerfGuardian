#include "perfguardian/pg002.hpp"

namespace perfguardian {

void RulePG002::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& cfg) const {
    const long long threshold = cfg.pg002_size_threshold_bytes;

    for (const auto& fn : db.functions()) {
        for (const auto& param : fn.params) {
            // Only flag non-const lvalue references of large types that the
            // function never modifies — a mutated reference can't become const.
            if (!param.is_reference || param.is_const || param.is_rvalue_ref ||
                param.is_mutated) {
                continue;
            }
            const long long sz = param.type_size_bytes;
            if (sz < 0 || sz <= threshold) {
                continue;
            }

            const std::string pname = param.name.empty() ? "(unnamed)" : param.name;

            Diagnostic d;
            d.rule_id   = std::string(rule_id());
            d.rule_name = std::string(rule_name());
            d.severity      = Severity::Medium;
            d.confidence    = Confidence::Medium;  // mutation analysis is conservative
            d.location      = {fn.file, fn.line, fn.column};
            d.function_name = fn.qualified_name;
            d.message   = "Parameter '" + pname + "' of type '" +
                          param.type_spelling + "' is a non-const reference (" +
                          std::to_string(sz) + " bytes); use 'const " +
                          param.bare_type_spelling +
                          "&' if the parameter is not modified";
            d.suggested_fix       = "const " + param.bare_type_spelling + "& " + pname;
            d.metrics.type_size_bytes = sz;

            sink.emit(std::move(d));
        }
    }
}

}  // namespace perfguardian
