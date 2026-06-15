#include "perfguardian/symbol_index.hpp"

namespace perfguardian {

void GlobalSymbolIndex::add(const ParseResult& result) {
    for (const auto& fn : result.functions) {
        if (fn.usr.empty()) continue;

        SymbolSummary s;
        s.usr            = fn.usr;
        s.qualified_name = fn.qualified_name;
        s.display_name   = fn.display_name;
        s.file           = fn.file;
        s.line           = fn.line;
        s.is_definition  = fn.is_definition;
        s.param_count    = static_cast<int>(fn.params.size());
        for (const auto& p : fn.params) {
            // Largest copyable by-value parameter — the unit of avoidable copy
            // cost amplified across call sites (mirrors PG001's eligibility).
            if (!p.is_reference && !p.is_pointer && !p.is_rvalue_ref &&
                !p.is_move_only && p.type_size_bytes > s.max_param_size) {
                s.max_param_size = p.type_size_bytes;
                s.max_param_type = p.bare_type_spelling.empty()
                                   ? p.type_spelling : p.bare_type_spelling;
                s.max_param_name = p.name;
            }
        }

        auto it = by_usr_.find(fn.usr);
        if (it == by_usr_.end()) {
            by_usr_.emplace(fn.usr, std::move(s));
        } else if (s.is_definition && !it->second.is_definition) {
            it->second = std::move(s);  // a definition supersedes a declaration
        }
    }
}

const SymbolSummary* GlobalSymbolIndex::find(const std::string& usr) const {
    auto it = by_usr_.find(usr);
    return it == by_usr_.end() ? nullptr : &it->second;
}

std::size_t GlobalSymbolIndex::definition_count() const {
    std::size_t n = 0;
    for (const auto& [usr, s] : by_usr_) {
        if (s.is_definition) ++n;
    }
    return n;
}

}  // namespace perfguardian
