#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include "perfguardian/parse_result.hpp"

namespace perfguardian {

// A compact, cross-TU summary of one function. Holds only what whole-program
// analysis needs — not the full body — so the index stays small even on large
// repositories (consistent with the Phase 16 memory discipline).
struct SymbolSummary {
    std::string usr;
    std::string qualified_name;
    std::string display_name;
    std::string file;
    int         line = 0;
    bool        is_definition = false;
    int         param_count   = 0;
    long long   max_param_size = 0;  // largest copyable by-value parameter
    std::string max_param_type;      // its type spelling, e.g. "Player"
    std::string max_param_name;      // its name, for the suggested fix
};

// Merges function symbols from every translation unit into one USR-keyed view,
// so a function defined in one file can be resolved from a call in another.
class GlobalSymbolIndex {
public:
    // Merge a translation unit's functions. When the same USR appears in more
    // than one TU, a definition takes precedence over a forward declaration.
    void add(const ParseResult& result);

    // Merge a pre-summarised symbol (e.g. from another shard's artifact), using
    // the same definition-supersedes-declaration rule as add().
    void merge(const SymbolSummary& s);

    // Resolve a symbol by USR, or nullptr if it is not in the index.
    const SymbolSummary* find(const std::string& usr) const;

    std::size_t size() const { return by_usr_.size(); }
    std::size_t definition_count() const;

    const std::unordered_map<std::string, SymbolSummary>& all() const {
        return by_usr_;
    }

private:
    std::unordered_map<std::string, SymbolSummary> by_usr_;
};

}  // namespace perfguardian
