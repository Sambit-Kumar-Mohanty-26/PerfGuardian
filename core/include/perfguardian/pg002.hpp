#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG002 — missing-const-ref
//
// Fires when a function parameter is a non-const lvalue reference (T&) and the
// referenced type is large enough that the caller almost certainly intended
// const T&.  A non-const ref forces the argument to be an lvalue and prevents
// passing temporaries, so this is both a performance smell and an API hazard.
class RulePG002 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG002"; }
    std::string_view rule_name() const override { return "missing-const-ref"; }
};

}  // namespace perfguardian
