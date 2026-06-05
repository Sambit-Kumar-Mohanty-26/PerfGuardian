#pragma once
#include "perfguardian/parse_result.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace perfguardian {

// Index of all symbols collected across one or more translation units.
// Populated by calling add() for each ParseResult; provides O(1) type lookup.
class SymbolDB {
public:
    // Merge one translation unit's results into the database.
    // Duplicate type names are silently deduplicated (first definition wins).
    void add(const ParseResult& result);

    // Look up a type by its qualified name. Returns nullptr if not found.
    const TypeDecl* find_type(const std::string& name) const;

    // Size in bytes for a type spelling; returns -1 if unknown.
    long long type_size(const std::string& name) const;

    const std::vector<FunctionDecl>& functions() const { return functions_; }
    const std::vector<TypeDecl>&     types()     const { return types_; }

    std::size_t function_count() const { return functions_.size(); }
    std::size_t type_count()     const { return types_.size(); }
    bool        empty()          const { return functions_.empty() && types_.empty(); }

private:
    std::vector<FunctionDecl>                    functions_;
    std::vector<TypeDecl>                        types_;
    std::unordered_map<std::string, std::size_t> type_index_; // name → index in types_
};

}  // namespace perfguardian
