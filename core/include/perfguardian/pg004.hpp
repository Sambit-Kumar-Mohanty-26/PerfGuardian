#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG004 — find-in-loop
//
// Fires when a container lookup (find, count, contains) is called inside a
// loop body.  Repeated O(log N) or O(N) lookups on the same container compound
// to O(N log N) or O(N²); hoisting or caching the result is usually trivial.
class RulePG004 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG004"; }
    std::string_view rule_name() const override { return "find-in-loop"; }
};

}  // namespace perfguardian
