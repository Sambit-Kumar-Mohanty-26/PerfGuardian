#include "perfguardian/rules.hpp"
#include "perfguardian/pg001.hpp"

namespace perfguardian {

std::vector<std::unique_ptr<IRule>> make_default_rules() {
    std::vector<std::unique_ptr<IRule>> rules;
    rules.push_back(std::make_unique<RulePG001>());
    return rules;
}

void run_rules(const SymbolDB& db,
               DiagnosticSink& sink,
               const std::vector<std::unique_ptr<IRule>>& rules,
               const RuleConfig& cfg) {
    for (const auto& rule : rules) {
        rule->run(db, sink, cfg);
    }
}

}  // namespace perfguardian
