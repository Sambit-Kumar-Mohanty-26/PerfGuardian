#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG005 — large-local-copy
//
// Fires when a function declares a local variable of a large value type.
// This typically indicates an unintentional copy (e.g. `auto x = getPlayer()`
// instead of `const auto& x = getPlayer()`).  References and pointers are
// not flagged.
class RulePG005 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG005"; }
    std::string_view rule_name() const override { return "large-local-copy"; }
};

}  // namespace perfguardian
