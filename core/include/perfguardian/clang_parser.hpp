#pragma once
#include <string>
#include <vector>
#include "perfguardian/parse_result.hpp"

namespace perfguardian {

struct ClangParserOptions {
    std::vector<std::string> extra_args;   // additional -I, -D flags
    bool                     verbose       = false;
    bool                     parse_bodies  = true;  // collect call sites + local vars
};

// Parse a single C++ source file using libclang.
// compile_args: the flags from compile_commands.json for this file.
// Returns a ParseResult with all function/type declarations found.
ParseResult parse_file(const std::string& filepath,
                       const std::vector<std::string>& compile_args,
                       const ClangParserOptions& opts = {});

}  // namespace perfguardian
