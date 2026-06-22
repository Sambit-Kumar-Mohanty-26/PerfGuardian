#pragma once
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/symbol_index.hpp"
#include "perfguardian/call_graph.hpp"
#include <string>
#include <vector>

namespace perfguardian {

// The partial result one shard produces for a subset of a project's translation
// units: its intra-TU findings (already final), the symbol summaries it saw, and
// its call-graph edges. Merging the artifacts of every shard reconstructs the
// whole-program view needed for cross-TU rules.
struct ShardArtifact {
    int shard_index = 0;
    int shard_count = 1;
    std::vector<Diagnostic>    findings;
    std::vector<SymbolSummary> symbols;
    std::vector<std::pair<std::string, std::string>> edges;  // caller_usr, callee_usr
};

// Serialize / parse a shard artifact as JSON. write_shard throws on I/O failure;
// read_shard throws std::runtime_error on missing file or malformed JSON.
void          write_shard(const std::string& path, const ShardArtifact& a);
ShardArtifact read_shard(const std::string& path);

// Partition predicate: does translation unit number `tu_index` belong to shard
// `shard_index` of `shard_count`? Deterministic round-robin by position.
inline bool tu_in_shard(std::size_t tu_index, int shard_index, int shard_count) {
    return shard_count <= 1 ||
           static_cast<int>(tu_index % static_cast<std::size_t>(shard_count)) == shard_index;
}

}  // namespace perfguardian
