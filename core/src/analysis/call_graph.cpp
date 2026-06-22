#include "perfguardian/call_graph.hpp"
#include <algorithm>

namespace perfguardian {

void CallGraph::add_edge(const std::string& caller, const std::string& callee) {
    if (caller.empty() || callee.empty() || caller == callee) return;
    bool inserted = callees_[caller].insert(callee).second;
    callers_[callee].insert(caller);
    if (inserted) ++edge_count_;
}

void CallGraph::add(const ParseResult& result) {
    for (const auto& fn : result.functions) {
        if (fn.usr.empty()) continue;
        for (const auto& cs : fn.call_sites) {
            add_edge(fn.usr, cs.callee_usr);
        }
    }
}

std::vector<std::pair<std::string, std::string>> CallGraph::edges() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(edge_count_);
    for (const auto& [caller, callees] : callees_)
        for (const auto& callee : callees) out.emplace_back(caller, callee);
    std::sort(out.begin(), out.end());
    return out;
}

void CallGraph::prune_to(const GlobalSymbolIndex& index) {
    std::size_t edges = 0;
    for (auto it = callers_.begin(); it != callers_.end();) {
        if (index.find(it->first) == nullptr) {
            it = callers_.erase(it);          // callee not part of the project
        } else {
            edges += it->second.size();
            ++it;
        }
    }
    // Rebuild callees_ from the surviving caller→callee relationships.
    std::unordered_map<std::string, std::unordered_set<std::string>> kept;
    for (const auto& [callee, callers] : callers_) {
        for (const auto& caller : callers) kept[caller].insert(callee);
    }
    callees_ = std::move(kept);
    edge_count_ = edges;
}

static std::vector<std::string> sorted(
    const std::unordered_map<std::string, std::unordered_set<std::string>>& m,
    const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) return {};
    std::vector<std::string> out(it->second.begin(), it->second.end());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> CallGraph::callers_of(const std::string& usr) const {
    return sorted(callers_, usr);
}

std::vector<std::string> CallGraph::callees_of(const std::string& usr) const {
    return sorted(callees_, usr);
}

std::size_t CallGraph::caller_count(const std::string& usr) const {
    auto it = callers_.find(usr);
    return it == callers_.end() ? 0 : it->second.size();
}

}  // namespace perfguardian
