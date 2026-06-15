#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "perfguardian/parse_result.hpp"
#include "perfguardian/symbol_index.hpp"

namespace perfguardian {

// A whole-program call graph keyed by USR, so "who calls f()" can be answered
// across translation units. Edges are deduplicated caller→callee relationships;
// call_site multiplicity is not retained here.
class CallGraph {
public:
    // Add every call edge in a translation unit (caller USR → callee USR).
    void add(const ParseResult& result);

    // Drop edges whose callee is not a known function in `index` (e.g. calls
    // into the standard library), leaving the call graph of the project itself.
    void prune_to(const GlobalSymbolIndex& index);

    // Functions that call `usr`, sorted for determinism (may span files).
    std::vector<std::string> callers_of(const std::string& usr) const;
    // Functions that `usr` calls, sorted for determinism.
    std::vector<std::string> callees_of(const std::string& usr) const;

    std::size_t caller_count(const std::string& usr) const;
    std::size_t edge_count() const { return edge_count_; }

private:
    void add_edge(const std::string& caller, const std::string& callee);

    std::unordered_map<std::string, std::unordered_set<std::string>> callers_;
    std::unordered_map<std::string, std::unordered_set<std::string>> callees_;
    std::size_t edge_count_ = 0;
};

}  // namespace perfguardian
