#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG003 — reserve-before-loop
//
// Fires when a function calls push_back (or emplace_back) inside a loop body
// without a prior reserve() call in the same function.  Each reallocation
// copies all elements; pre-reserving eliminates O(N log N) copies.
class RulePG003 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG003"; }
    std::string_view rule_name() const override { return "reserve-before-loop"; }
};

}  // namespace perfguardian
