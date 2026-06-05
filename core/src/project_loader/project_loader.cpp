#include "perfguardian/project_loader.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <stdexcept>
#include <filesystem>

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

}  // namespace perfguardian
