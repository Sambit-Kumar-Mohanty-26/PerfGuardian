#pragma once
#include "perfguardian/rules.hpp"

namespace perfguardian {

// PG006 — repeated-map-lookup
//
// Fires when a map-style lookup (find, operator[], at, count) is called two or
// more times within the same function.  Each call traverses the tree or probes
// the hash table independently; caching the result in a local iterator or
// reference eliminates the redundant work.
class RulePG006 final : public IRule {
public:
    void run(const SymbolDB& db, DiagnosticSink& sink,
             const RuleConfig& cfg) const override;

    std::string_view rule_id()   const override { return "PG006"; }
    std::string_view rule_name() const override { return "repeated-map-lookup"; }
};

}  // namespace perfguardian
