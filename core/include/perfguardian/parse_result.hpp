#pragma once
#include <string>
#include <vector>

namespace perfguardian {

// A single function parameter as seen in the AST
struct ParamInfo {
    std::string name;
    std::string type_spelling;   // human-readable type, e.g. "Player"
    long long   type_size_bytes; // sizeof(type), -1 if not computable
    bool        is_reference;    // true for T& or const T&
    bool        is_pointer;      // true for T*
    bool        is_const;        // true for const T, const T&, etc.
    bool        is_rvalue_ref;   // true for T&&
};

// A function or method declaration
struct FunctionDecl {
    std::string qualified_name;  // e.g. "ns::MyClass::method"
    std::string display_name;    // e.g. "method(Player, int)"
    std::string file;
    int         line    = 0;
    int         column  = 0;
    std::vector<ParamInfo> params;
};

// A user-defined type (class/struct)
struct TypeDecl {
    std::string qualified_name;
    std::string file;
    int         line           = 0;
    long long   size_bytes     = -1;  // -1 if incomplete
    bool        trivially_copyable = false;
};

// Result of parsing a single translation unit (source file)
struct ParseResult {
    std::string               source_file;
    std::vector<FunctionDecl> functions;
    std::vector<TypeDecl>     types;
    std::vector<std::string>  errors;   // parse-time diagnostics
    bool                      ok = true;
};

}  // namespace perfguardian
