#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG001 — large-object-by-value
//
// Fires when a function parameter is passed by value and its size (as
// reported by libclang's sizeof) exceeds RuleConfig::pg001_size_threshold_bytes.
// Passes by const-ref, pointer, or rvalue-ref are not flagged.
class RulePG001 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG001"; }
    std::string_view rule_name() const override { return "large-object-by-value"; }
};

}  // namespace perfguardian
