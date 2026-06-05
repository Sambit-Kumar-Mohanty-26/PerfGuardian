#include "perfguardian/symbol_db.hpp"

namespace perfguardian {

void SymbolDB::add(const ParseResult& result) {
    for (const auto& fn : result.functions) {
        functions_.push_back(fn);
    }
    for (const auto& td : result.types) {
        if (type_index_.find(td.qualified_name) == type_index_.end()) {
            type_index_[td.qualified_name] = types_.size();
            types_.push_back(td);
        }
    }
}

const TypeDecl* SymbolDB::find_type(const std::string& name) const {
    auto it = type_index_.find(name);
    if (it == type_index_.end()) return nullptr;
    return &types_[it->second];
}

long long SymbolDB::type_size(const std::string& name) const {
    const TypeDecl* td = find_type(name);
    return td ? td->size_bytes : -1LL;
}

}  // namespace perfguardian
