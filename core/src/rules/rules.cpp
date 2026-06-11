#include "perfguardian/rules.hpp"
#include "perfguardian/pg001.hpp"
#include "perfguardian/pg002.hpp"
#include "perfguardian/pg003.hpp"
#include "perfguardian/pg004.hpp"
#include "perfguardian/pg005.hpp"
#include "perfguardian/pg006.hpp"

namespace perfguardian {

std::vector<std::unique_ptr<IRule>> make_default_rules() {
    std::vector<std::unique_ptr<IRule>> rules;
    rules.push_back(std::make_unique<RulePG001>());
    rules.push_back(std::make_unique<RulePG002>());
    rules.push_back(std::make_unique<RulePG003>());
    rules.push_back(std::make_unique<RulePG004>());
    rules.push_back(std::make_unique<RulePG005>());
    rules.push_back(std::make_unique<RulePG006>());
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
