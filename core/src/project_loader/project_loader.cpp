#include "perfguardian/project_loader.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <set>
#include <string_view>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace perfguardian {

std::vector<CompileCommand> load_compile_commands(const std::string& build_dir) {
    // Look for compile_commands.json in the given directory
    fs::path path = fs::path(build_dir) / "compile_commands.json";
    if (!fs::exists(path)) {
        // Also try the directory itself as the JSON file
        fs::path direct(build_dir);
        if (direct.extension() == ".json" && fs::exists(direct)) {
            path = direct;
        } else {
            throw std::runtime_error("compile_commands.json not found in: " + build_dir);
        }
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open: " + path.string());
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    std::vector<CompileCommand> result;
    result.reserve(j.size());

    for (const auto& entry : j) {
        CompileCommand cmd;
        cmd.file      = entry.value("file",      "");
        cmd.directory = entry.value("directory", "");

        if (entry.contains("arguments") && entry["arguments"].is_array()) {
            for (const auto& arg : entry["arguments"]) {
                cmd.arguments.push_back(arg.get<std::string>());
            }
        } else if (entry.contains("command") && entry["command"].is_string()) {
            // Split "command" string by spaces (naive split — handles most cases)
            std::string command = entry["command"].get<std::string>();
            std::istringstream iss(command);
            std::string token;
            while (iss >> token) {
                cmd.arguments.push_back(token);
            }
        }

        if (!cmd.file.empty()) {
            result.push_back(std::move(cmd));
        }
    }

    spdlog::debug("Loaded {} compile commands from {}", result.size(), path.string());
    return result;
}

std::vector<std::string> enumerate_sources(const std::string& source_dir) {
    static const std::vector<std::string> cpp_exts = {".cpp", ".cc", ".cxx", ".c++"};

    std::vector<std::string> sources;
    std::error_code ec;

    for (const auto& entry : fs::recursive_directory_iterator(source_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        for (const auto& e : cpp_exts) {
            if (ext == e) {
                sources.push_back(entry.path().string());
                break;
            }
        }
    }

    spdlog::debug("Enumerated {} source files in {}", sources.size(), source_dir);
    return sources;
}

namespace {

bool is_source_path(std::string_view s) {
    static const char* exts[] = {".cpp", ".cc", ".cxx", ".c++", ".c"};
    for (const char* e : exts) {
        if (s.size() >= std::string_view(e).size() &&
            s.substr(s.size() - std::string_view(e).size()) == e) {
            return true;
        }
    }
    return false;
}

// Make a relative include path absolute against the compile entry's directory.
std::string resolve_dir(const std::string& dir, const std::string& p) {
    fs::path path(p);
    if (path.is_absolute() || dir.empty()) return p;
    std::error_code ec;
    fs::path abs = fs::weakly_canonical(fs::path(dir) / path, ec);
    return ec ? (fs::path(dir) / path).string() : abs.string();
}

}  // namespace

std::vector<std::string> sanitize_compile_args(const CompileCommand& cmd) {
    // Flags that carry a separate following argument we must also drop.
    static const std::set<std::string> kDropWithArg = {
        "-o", "-MF", "-MT", "-MQ", "-MJ", "-c"  // -c has no arg but harmless here
    };
    // Standalone flags to drop (dependency generation, compile-only, etc.).
    static const std::set<std::string> kDropFlag = {
        "-c", "-MD", "-MMD", "-MP", "-MG", "-M", "-MM", "-Werror", "-pipe"
    };
    // Include-style flags whose path argument may need resolving.
    static const std::set<std::string> kIncWithArg = {
        "-I", "-isystem", "-iquote", "-idirafter"
    };

    std::vector<std::string> out;
    bool has_std = false;

    const auto& args = cmd.arguments;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];

        if (i == 0) continue;                 // compiler executable
        if (a == cmd.file) continue;          // the source file itself
        if (is_source_path(a)) continue;      // any stray source path
        if (a.rfind("-fdeps", 0) == 0) continue;       // GCC P1689 dep scanning
        if (a.rfind("-Werror=", 0) == 0) continue;     // keep warnings non-fatal

        if (kDropWithArg.count(a)) {
            if (a != "-c") ++i;               // also skip the following argument
            continue;
        }
        if (kDropFlag.count(a)) continue;

        // Separated include flag: "-I" "path"
        if (kIncWithArg.count(a) && i + 1 < args.size()) {
            out.push_back(a);
            out.push_back(resolve_dir(cmd.directory, args[++i]));
            continue;
        }
        // Joined include flag: "-Ipath" / "-isystempath"
        for (const std::string& inc : kIncWithArg) {
            if (a.rfind(inc, 0) == 0 && a.size() > inc.size()) {
                out.push_back(inc + resolve_dir(cmd.directory, a.substr(inc.size())));
                goto next_arg;
            }
        }

        if (a.rfind("-std", 0) == 0) has_std = true;
        out.push_back(a);
    next_arg:;
    }

    if (!has_std) out.push_back("-std=c++20");
    return out;
}

std::vector<std::string> infer_include_dirs(const std::string& source_dir) {
    static const std::set<std::string> hdr_exts = {
        ".h", ".hpp", ".hh", ".hxx", ".h++", ".inl"
    };

    std::set<std::string> dirs;
    std::error_code ec;

    fs::path root(source_dir);
    dirs.insert(fs::absolute(root, ec).string());

    for (const auto& entry : fs::recursive_directory_iterator(source_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(c));
        if (hdr_exts.count(ext)) {
            dirs.insert(entry.path().parent_path().string());
            // Also expose the parent (so `#include "lib/foo.hpp"` resolves).
            dirs.insert(entry.path().parent_path().parent_path().string());
        }
    }

    std::vector<std::string> flags;
    flags.reserve(dirs.size());
    for (const auto& d : dirs) {
        if (!d.empty()) flags.push_back("-I" + d);
    }
    spdlog::debug("Inferred {} include dirs under {}", flags.size(), source_dir);
    return flags;
}

}  // namespace perfguardian
