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

// Turn a raw compile_commands.json entry into a clean argument list for
// libclang: drops the compiler, the source file, output/-c flags, and
// dependency-generation flags that libclang rejects; resolves relative
// include paths against the entry's working directory. Ensures a -std flag.
std::vector<std::string> sanitize_compile_args(const CompileCommand& cmd);

// Fallback for folder scans with no compile database: infer -I flags from the
// tree by adding the root and every directory that contains a header file.
std::vector<std::string> infer_include_dirs(const std::string& source_dir);

}  // namespace perfguardian
