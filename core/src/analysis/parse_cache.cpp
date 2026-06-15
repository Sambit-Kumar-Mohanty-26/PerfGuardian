#include "perfguardian/parse_cache.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace perfguardian {

// JSON (de)serialization for the parse structures, used by the on-disk cache.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ParamInfo, name, type_spelling,
    bare_type_spelling, type_size_bytes, is_reference, is_pointer, is_const,
    is_rvalue_ref, is_mutated, is_move_only)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CallSite, callee, callee_usr, lookup_target,
    inside_loop, loop_depth, file, line)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LocalVar, name, type_spelling,
    bare_type_spelling, type_size_bytes, is_reference, is_pointer,
    is_copy_initialized, file, line)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FunctionDecl, qualified_name, display_name,
    usr, is_definition, file, line, column, params, call_sites, local_vars)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TypeDecl, qualified_name, usr, file, line,
    size_bytes, trivially_copyable)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ParseResult, source_file, functions, types,
    errors, ok)

std::string cache_key(const std::string& source_content,
                      const std::vector<std::string>& args,
                      const std::string& version) {
    // FNV-1a 64-bit over content + args + version. Deterministic and fast.
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&](const std::string& s) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        h ^= 0xff;            // separator so "ab"+"c" != "a"+"bc"
        h *= 1099511628211ULL;
    };
    mix(source_content);
    for (const auto& a : args) mix(a);
    mix(version);

    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

std::optional<ParseResult> cache_load(const std::string& cache_dir,
                                      const std::string& key) {
    fs::path path = fs::path(cache_dir) / (key + ".json");
    std::error_code ec;
    if (!fs::exists(path, ec)) return std::nullopt;

    std::ifstream in(path);
    if (!in.is_open()) return std::nullopt;
    try {
        json j;
        in >> j;
        return j.get<ParseResult>();
    } catch (const std::exception& e) {
        spdlog::debug("cache_load failed for {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

void cache_store(const std::string& cache_dir, const std::string& key,
                 const ParseResult& result) {
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) return;

    fs::path final_path = fs::path(cache_dir) / (key + ".json");
    // Write to a unique temp file then rename, so concurrent writers and
    // interrupted runs never leave a half-written cache entry.
    fs::path tmp_path = fs::path(cache_dir) /
        (key + "." + std::to_string(reinterpret_cast<std::uintptr_t>(&result)) + ".tmp");
    try {
        {
            std::ofstream out(tmp_path, std::ios::trunc);
            if (!out.is_open()) return;
            out << json(result).dump();
        }
        fs::rename(tmp_path, final_path, ec);
        if (ec) fs::remove(tmp_path, ec);
    } catch (const std::exception& e) {
        spdlog::debug("cache_store failed for {}: {}", final_path.string(), e.what());
        fs::remove(tmp_path, ec);
    }
}

}  // namespace perfguardian
