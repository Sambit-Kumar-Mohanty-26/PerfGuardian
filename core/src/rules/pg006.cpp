#include "perfguardian/pg006.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace perfguardian {

static const std::unordered_set<std::string> kMapCallees = {
    "find", "operator[]", "at", "count", "contains"
};

void RulePG006::run(const SymbolDB& db, DiagnosticSink& sink,
                    const RuleConfig& cfg) const {
    const int min_repeat = cfg.pg006_min_repeat_count;

    for (const auto& fn : db.functions()) {
        // Group lookups by what they look up (container + key), not by the
        // operation. `m.count(key)` then `m[key]` share a target and so count
        // as the same key looked up twice; `m[x]` and `m[y]` stay distinct.
        // Synthetic call sites with no target fall back to the callee name.
        std::unordered_map<std::string, int> counts;
        std::unordered_map<std::string, int> first_line;
        for (const auto& cs : fn.call_sites) {
            if (kMapCallees.count(cs.callee) == 0) continue;
            const std::string& key =
                cs.lookup_target.empty() ? cs.callee : cs.lookup_target;
            int& n = counts[key];
            if (n == 0) first_line[key] = cs.line;
            ++n;
        }

        for (const auto& [key, n] : counts) {
            if (n < min_repeat) continue;

            Diagnostic d;
            d.rule_id   = std::string(rule_id());
            d.rule_name = std::string(rule_name());
            d.severity      = Severity::Medium;
            int fl          = first_line.count(key) ? first_line[key] : fn.line;
            d.location      = {fn.file, fl > 0 ? fl : fn.line, fn.column};
            d.function_name = fn.qualified_name;
            d.message   = "The same container key is looked up " +
                          std::to_string(n) + " times in '" + fn.qualified_name +
                          "' — cache the result to avoid redundant lookups";
            d.suggested_fix      = "auto it = container.find(key); "
                                   "reuse 'it' instead of looking up again";
            d.metrics.lookup_count = n;

            sink.emit(std::move(d));
        }
    }
}

}  // namespace perfguardian
