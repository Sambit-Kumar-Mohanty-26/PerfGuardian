#pragma once
#include <string>
#include <vector>

namespace perfguardian {

// One entry from compile_commands.json
struct CompileCommand {
    std::string file;
    std::string directory;
    std::vector<std::string> arguments;  // argv[0] is the compiler
};

// Load all entries from compile_commands.json in the given directory.
// Throws std::runtime_error on failure.
std::vector<CompileCommand> load_compile_commands(const std::string& build_dir);

// Walk a source tree to collect .cpp/.cc/.cxx files when
// compile_commands.json is not available (fallback mode).
std::vector<std::string> enumerate_sources(const std::string& source_dir);

}  // namespace perfguardian
