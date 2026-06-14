#include "perfguardian/pg001.hpp"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace perfguardian {

void RulePG001::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& cfg) const {
    const long long threshold = cfg.pg001_size_threshold_bytes;

    for (const auto& fn : db.functions()) {
        for (const auto& param : fn.params) {
            // Only flag true by-value parameters
            if (param.is_reference || param.is_pointer || param.is_rvalue_ref) {
                continue;
            }
            // Move-only types (unique_ptr, mutex, …) can't be copied — taking
            // them by value is the sink idiom, not an accidental copy.
            if (param.is_move_only) {
                continue;
            }

            const long long sz = param.type_size_bytes;
            if (sz < 0 || sz <= threshold) {
                continue;
            }

            const std::string param_name =
                param.name.empty() ? "(unnamed)" : param.name;

            Diagnostic d;
            d.rule_id   = std::string(rule_id());
            d.rule_name = std::string(rule_name());
            d.severity       = Severity::High;
            d.location       = {fn.file, fn.line, fn.column};
            d.function_name  = fn.qualified_name;
            d.message   = "Parameter '" + param_name + "' of type '" +
                          param.type_spelling + "' is passed by value (" +
                          std::to_string(sz) + " bytes); use 'const " +
                          param.type_spelling + "&' to avoid the copy";
            d.suggested_fix    = "const " + param.type_spelling + "& " + param_name;
            d.metrics.type_size_bytes = sz;
            d.metrics.copy_cost_bytes = sz;

            sink.emit(std::move(d));
        }
    }
}

}  // namespace perfguardian
