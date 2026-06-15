#pragma once
#include <optional>
#include <string>
#include <vector>
#include "perfguardian/parse_result.hpp"

namespace perfguardian {

// On-disk cache of parsed translation units, keyed by a hash of the source
// content, the compile arguments, and the tool version. Lets repeat runs skip
// re-parsing files that have not changed.
//
// Note: the key covers the source file and its arguments, not the transitive
// set of included headers — a header-only change is not detected. Clear the
// cache directory (or omit --cache-dir) when headers change.

// Stable hash of (source content + args + version) → hex string used as the key.
std::string cache_key(const std::string& source_content,
                      const std::vector<std::string>& args,
                      const std::string& version);

// Load a previously cached ParseResult for `key`, or std::nullopt on a miss.
std::optional<ParseResult> cache_load(const std::string& cache_dir,
                                      const std::string& key);

// Store a ParseResult under `key`. Best-effort: failures are silently ignored.
void cache_store(const std::string& cache_dir, const std::string& key,
                 const ParseResult& result);

}  // namespace perfguardian
