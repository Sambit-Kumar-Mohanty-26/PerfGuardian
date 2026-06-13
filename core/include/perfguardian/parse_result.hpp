#pragma once
#include <string>
#include <vector>

namespace perfguardian {

// A single function parameter as seen in the AST
struct ParamInfo {
    std::string name;
    std::string type_spelling;   // human-readable type, e.g. "const Player &"
    std::string bare_type_spelling; // referent type without const/&, e.g. "Player"
    long long   type_size_bytes; // sizeof(type), -1 if not computable
    bool        is_reference;    // true for T& or const T&
    bool        is_pointer;      // true for T*
    bool        is_const;        // true for const T, const T&, etc.
    bool        is_rvalue_ref;   // true for T&&
    bool        is_mutated = false; // written to in the body (assignment, non-const
                                    // method call, passed to non-const ref, etc.)
};

// A call expression captured inside a function body (Phase 4)
struct CallSite {
    std::string callee;           // method/function name, e.g. "push_back", "find"
    std::string lookup_target;    // container+key signature, ignoring the operation
                                  // (e.g. "key|m"); empty if not computed
    bool        inside_loop = false;
    int         loop_depth  = 0;
    std::string file;
    int         line = 0;
};

// A local variable declaration inside a function body (Phase 4)
struct LocalVar {
    std::string name;
    std::string type_spelling;
    std::string bare_type_spelling;       // type without const/&, e.g. "Player"
    long long   type_size_bytes = -1;
    bool        is_reference    = false;
    bool        is_pointer      = false;
    bool        is_copy_initialized = false; // initialized by copying an existing object
    std::string file;
    int         line = 0;
};

// A function or method declaration
struct FunctionDecl {
    std::string qualified_name;  // e.g. "ns::MyClass::method"
    std::string display_name;    // e.g. "method(Player, int)"
    std::string file;
    int         line    = 0;
    int         column  = 0;
    std::vector<ParamInfo> params;
    std::vector<CallSite>  call_sites;  // Phase 4: body-level call expressions
    std::vector<LocalVar>  local_vars;  // Phase 4: local variable declarations
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
