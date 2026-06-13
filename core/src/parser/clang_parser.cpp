#include "perfguardian/clang_parser.hpp"
#include <clang-c/Index.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <unordered_set>
#include <filesystem>

namespace perfguardian {

// helpers

static std::string cx_to_string(CXString cxs) {
    std::string result;
    if (const char* cstr = clang_getCString(cxs)) {
        result = cstr;
    }
    clang_disposeString(cxs);
    return result;
}

// Spelling of a type with any leading "const " and trailing reference/pointer
// markers removed, e.g. "const Player &" -> "Player". Used to build correct
// "const T&" suggestions without doubling qualifiers.
static std::string bare_type_name(CXType type) {
    std::string s = cx_to_string(clang_getTypeSpelling(type));
    // trim trailing whitespace, '&' and '*'
    while (!s.empty() && (s.back() == '&' || s.back() == '*' || s.back() == ' ')) {
        s.pop_back();
    }
    // strip a single leading "const "
    constexpr std::string_view kConst = "const ";
    if (s.rfind(kConst, 0) == 0) {
        s.erase(0, kConst.size());
    }
    return s;
}

static ParamInfo make_param_info(CXCursor param_cursor) {
    ParamInfo p;
    p.name = cx_to_string(clang_getCursorSpelling(param_cursor));

    CXType cxtype = clang_getCursorType(param_cursor);
    p.type_spelling = cx_to_string(clang_getTypeSpelling(cxtype));

    CXTypeKind kind = cxtype.kind;
    p.is_reference   = (kind == CXType_LValueReference);
    p.is_rvalue_ref  = (kind == CXType_RValueReference);
    p.is_pointer     = (kind == CXType_Pointer);

    // A reference type is never itself const-qualified — the const lives on the
    // pointee (e.g. `const T&`). Check the referent for references/pointers.
    CXType referent = cxtype;
    if (p.is_reference || p.is_rvalue_ref || p.is_pointer) {
        referent = clang_getPointeeType(cxtype);
    }
    p.is_const            = clang_isConstQualifiedType(referent) != 0;
    p.bare_type_spelling  = bare_type_name(referent);

    // Unwrap reference/pointer to get the pointee type for size computation
    CXType canonical = clang_getCanonicalType(cxtype);
    if (p.is_reference || p.is_rvalue_ref) {
        canonical = clang_getCanonicalType(clang_getPointeeType(cxtype));
    }

    long long sz = clang_Type_getSizeOf(canonical);
    p.type_size_bytes = (sz >= 0) ? sz : -1;

    return p;
}

// Body visitor

struct BodyVisitorData {
    FunctionDecl*         fn;
    CXTranslationUnit     tu = nullptr;
    int                   loop_depth = 0;
    bool                  in_main_file = true; // already filtered by caller
    std::set<std::string> param_names;         // names of the function's params
    std::set<std::string> mutated;             // params written to in the body
};

// Identifier tokens inside a cursor's extent that name one of `params`.
static std::set<std::string> param_idents_in(CXTranslationUnit tu, CXCursor c,
                                             const std::set<std::string>& params) {
    std::set<std::string> found;
    CXToken* toks = nullptr; unsigned n = 0;
    clang_tokenize(tu, clang_getCursorExtent(c), &toks, &n);
    for (unsigned i = 0; i < n; ++i) {
        if (clang_getTokenKind(toks[i]) != CXToken_Identifier) continue;
        std::string s = cx_to_string(clang_getTokenSpelling(tu, toks[i]));
        if (params.count(s)) found.insert(std::move(s));
    }
    clang_disposeTokens(tu, toks, n);
    return found;
}

// Detects parameters mutated by an assignment / increment / address-of cursor.
// Token-based: the write target is whatever appears before the operator.
static void mark_assignment(CXTranslationUnit tu, CXCursor c, CXCursorKind kind,
                            const std::set<std::string>& params,
                            std::set<std::string>& mutated) {
    static const std::set<std::string> kCompound = {
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="
    };
    static const std::set<std::string> kIncDec = { "++", "--" };

    CXToken* toks = nullptr; unsigned n = 0;
    clang_tokenize(tu, clang_getCursorExtent(c), &toks, &n);

    int op = -1;          // index of the assignment operator, if any
    bool unary_mut = false;
    for (unsigned i = 0; i < n; ++i) {
        if (clang_getTokenKind(toks[i]) != CXToken_Punctuation) continue;
        std::string s = cx_to_string(clang_getTokenSpelling(tu, toks[i]));
        if (kind == CXCursor_CompoundAssignOperator && kCompound.count(s)) { op = (int)i; break; }
        if (kind == CXCursor_BinaryOperator && s == "=") { op = (int)i; break; }
        if (kind == CXCursor_UnaryOperator && (kIncDec.count(s) || s == "&")) { unary_mut = true; }
    }

    for (unsigned i = 0; i < n; ++i) {
        if (clang_getTokenKind(toks[i]) != CXToken_Identifier) continue;
        // Assignment/compound: only the left-hand side (before the operator).
        // Unary ++/--/&: any param in the (small) extent.
        if (op >= 0 ? (int)i < op : unary_mut) {
            std::string s = cx_to_string(clang_getTokenSpelling(tu, toks[i]));
            if (params.count(s)) mutated.insert(std::move(s));
        }
    }
    clang_disposeTokens(tu, toks, n);
}

// Detects parameters mutated by a call: a non-const member call on the param,
// passing the param to a non-const reference/pointer, or a mutating operator
// (`out << x`, compound assignment) whose left operand is the param.
static void mark_call(CXTranslationUnit tu, CXCursor cursor,
                      const std::set<std::string>& params,
                      std::set<std::string>& mutated) {
    CXCursor callee = clang_getCursorReferenced(cursor);

    // Non-const member call → the receiver (first identifier token) is mutated.
    if (clang_getCursorKind(callee) == CXCursor_CXXMethod &&
        clang_CXXMethod_isConst(callee) == 0) {
        CXToken* toks = nullptr; unsigned n = 0;
        clang_tokenize(tu, clang_getCursorExtent(cursor), &toks, &n);
        for (unsigned i = 0; i < n; ++i) {
            if (clang_getTokenKind(toks[i]) != CXToken_Identifier) continue;
            std::string s = cx_to_string(clang_getTokenSpelling(tu, toks[i]));
            if (params.count(s)) mutated.insert(s);
            break; // only the receiver (leading identifier)
        }
        clang_disposeTokens(tu, toks, n);
    }

    // Mutating operator (stream insertion/extraction, compound assignment):
    // the left operand is written. Covers `out << x` regardless of overload.
    std::string cname = cx_to_string(clang_getCursorSpelling(callee));
    if (cname.rfind("operator", 0) == 0) {
        static const std::set<std::string> kMutOps = {
            "operator<<", "operator>>", "operator=", "operator+=", "operator-=",
            "operator*=", "operator/=", "operator%=", "operator&=", "operator|=",
            "operator^=", "operator[]"
        };
        if (kMutOps.count(cname)) {
            CXToken* toks = nullptr; unsigned n = 0;
            clang_tokenize(tu, clang_getCursorExtent(cursor), &toks, &n);
            for (unsigned i = 0; i < n; ++i) {
                if (clang_getTokenKind(toks[i]) != CXToken_Identifier) continue;
                std::string s = cx_to_string(clang_getTokenSpelling(tu, toks[i]));
                if (params.count(s)) mutated.insert(s);
                break; // left operand only
            }
            clang_disposeTokens(tu, toks, n);
        }
    }

    // Argument passed to a non-const reference or pointer parameter.
    int na = clang_Cursor_getNumArguments(cursor);
    CXType ft = clang_getCursorType(callee);
    for (int i = 0; i < na; ++i) {
        CXType pt = clang_getArgType(ft, i);
        if (pt.kind == CXType_Invalid) continue;
        bool nonconst_ref = (pt.kind == CXType_LValueReference) &&
                            clang_isConstQualifiedType(clang_getPointeeType(pt)) == 0;
        bool nonconst_ptr = (pt.kind == CXType_Pointer) &&
                            clang_isConstQualifiedType(clang_getPointeeType(pt)) == 0;
        if (!nonconst_ref && !nonconst_ptr) continue;
        CXCursor arg = clang_Cursor_getArgument(cursor, i);
        for (auto& p : param_idents_in(tu, arg, params)) mutated.insert(p);
    }
}

// Builds a signature for a lookup call expression from its source tokens:
// the set of identifier/literal tokens with the lookup operation names removed,
// sorted and joined. So `m.count(key)` and `m[key]` both yield "key|m", while
// `m[x]` and `m[y]` differ. Lets PG006 group lookups by target, not operation.
static std::string lookup_signature(CXTranslationUnit tu, CXCursor cursor) {
    static const std::unordered_set<std::string> kOps = {
        "find", "at", "count", "contains", "operator"
    };
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXToken* tokens = nullptr;
    unsigned num = 0;
    clang_tokenize(tu, range, &tokens, &num);

    std::set<std::string> parts;
    for (unsigned i = 0; i < num; ++i) {
        CXTokenKind tk = clang_getTokenKind(tokens[i]);
        if (tk != CXToken_Identifier && tk != CXToken_Literal) continue;
        std::string s = cx_to_string(clang_getTokenSpelling(tu, tokens[i]));
        if (kOps.count(s)) continue;
        parts.insert(std::move(s));
    }
    clang_disposeTokens(tu, tokens, num);

    std::string sig;
    for (const auto& p : parts) {
        if (!sig.empty()) sig += '|';
        sig += p;
    }
    return sig;
}

// Detects whether a VarDecl is copy-initialized from an existing named object,
// e.g. `Player p = other;` or `auto x = vec[i];`. A reference to another
// variable/parameter in the initializer is the signal. Default/value/direct
// construction (`Player p{};`, `Player p(1,2);`) has no such reference and is
// therefore not treated as an avoidable copy.
struct CopyInitProbe { bool found = false; };

static CXChildVisitResult probe_copy_init(CXCursor cursor, CXCursor /*parent*/,
                                          CXClientData client_data) {
    auto* probe = static_cast<CopyInitProbe*>(client_data);
    if (clang_getCursorKind(cursor) == CXCursor_DeclRefExpr) {
        CXCursorKind rk = clang_getCursorKind(clang_getCursorReferenced(cursor));
        if (rk == CXCursor_VarDecl || rk == CXCursor_ParmDecl) {
            probe->found = true;
            return CXChildVisit_Break;
        }
    }
    return CXChildVisit_Recurse;
}

// Forward-declare so visit_body can call itself recursively for loops
static CXChildVisitResult visit_body(CXCursor cursor, CXCursor parent,
                                      CXClientData client_data);

static CXChildVisitResult visit_body(CXCursor cursor, CXCursor /*parent*/,
                                      CXClientData client_data) {
    auto* data = static_cast<BodyVisitorData*>(client_data);
    CXCursorKind kind = clang_getCursorKind(cursor);

    // Phase 11: detect parameter mutation for PG002 precision.
    if (data->tu && !data->param_names.empty()) {
        if (kind == CXCursor_BinaryOperator ||
            kind == CXCursor_CompoundAssignOperator ||
            kind == CXCursor_UnaryOperator) {
            mark_assignment(data->tu, cursor, kind, data->param_names, data->mutated);
        } else if (kind == CXCursor_CallExpr) {
            mark_call(data->tu, cursor, data->param_names, data->mutated);
        }
    }

    // Manage loop depth: recurse manually so depth is correct for children
    bool is_loop = (kind == CXCursor_ForStmt      ||
                    kind == CXCursor_WhileStmt     ||
                    kind == CXCursor_DoStmt        ||
                    kind == CXCursor_CXXForRangeStmt);
    if (is_loop) {
        ++data->loop_depth;
        clang_visitChildren(cursor, visit_body, data);
        --data->loop_depth;
        return CXChildVisit_Continue;
    }

    // Capture call expressions
    if (kind == CXCursor_CallExpr) {
        std::string callee = cx_to_string(clang_getCursorSpelling(cursor));
        if (!callee.empty()) {
            CallSite cs;
            cs.callee      = std::move(callee);
            cs.inside_loop = (data->loop_depth > 0);
            cs.loop_depth  = data->loop_depth;
            if (data->tu) cs.lookup_target = lookup_signature(data->tu, cursor);
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file; unsigned line;
            clang_getFileLocation(loc, &file, &line, nullptr, nullptr);
            cs.file = file ? cx_to_string(clang_getFileName(file)) : "";
            cs.line = static_cast<int>(line);
            data->fn->call_sites.push_back(std::move(cs));
        }
        return CXChildVisit_Recurse;
    }

    // Capture local variable declarations (ParmDecl is excluded by cursor kind)
    if (kind == CXCursor_VarDecl) {
        // Semantic parent should be a function/method, not a class/struct
        CXCursorKind pk = clang_getCursorKind(
            clang_getCursorSemanticParent(cursor));
        if (pk == CXCursor_FunctionDecl || pk == CXCursor_CXXMethod ||
            pk == CXCursor_Constructor  || pk == CXCursor_FunctionTemplate) {
            LocalVar lv;
            lv.name          = cx_to_string(clang_getCursorSpelling(cursor));
            CXType cxtype    = clang_getCursorType(cursor);
            lv.type_spelling = cx_to_string(clang_getTypeSpelling(cxtype));
            lv.bare_type_spelling = bare_type_name(cxtype);
            CXTypeKind tk    = cxtype.kind;
            lv.is_reference  = (tk == CXType_LValueReference);
            lv.is_pointer    = (tk == CXType_Pointer);
            CXType canonical = clang_getCanonicalType(cxtype);
            long long sz     = clang_Type_getSizeOf(canonical);
            lv.type_size_bytes = (sz >= 0) ? sz : -1;

            CopyInitProbe probe;
            clang_visitChildren(cursor, probe_copy_init, &probe);
            lv.is_copy_initialized = probe.found;
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file; unsigned line;
            clang_getFileLocation(loc, &file, &line, nullptr, nullptr);
            lv.file = file ? cx_to_string(clang_getFileName(file)) : "";
            lv.line = static_cast<int>(line);
            data->fn->local_vars.push_back(std::move(lv));
        }
        return CXChildVisit_Recurse;
    }

    return CXChildVisit_Recurse;
}

// AST visitor

struct VisitorData {
    ParseResult*       result;
    CXTranslationUnit  tu;
    const std::string* main_file;
    bool               parse_bodies;
};

static bool is_in_main_file(CXCursor cursor, const std::string& main_file) {
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    CXFile file;
    clang_getFileLocation(loc, &file, nullptr, nullptr, nullptr);
    if (!file) return false;
    std::string fname = cx_to_string(clang_getFileName(file));
    for (auto& c : fname) if (c == '\\') c = '/';
    std::string norm_main = main_file;
    for (auto& c : norm_main) if (c == '\\') c = '/';
    return fname == norm_main;
}

static CXChildVisitResult visit_ast(CXCursor cursor, CXCursor /*parent*/,
                                     CXClientData client_data) {
    auto* data = static_cast<VisitorData*>(client_data);

    if (!is_in_main_file(cursor, *data->main_file)) {
        return CXChildVisit_Continue;
    }

    CXCursorKind kind = clang_getCursorKind(cursor);

    // Record function and method declarations
    if (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod ||
        kind == CXCursor_Constructor   || kind == CXCursor_FunctionTemplate) {

        if (!clang_isCursorDefinition(cursor) &&
            clang_equalCursors(cursor, clang_getCanonicalCursor(cursor)) == 0) {
            return CXChildVisit_Recurse;
        }

        FunctionDecl fn;
        fn.qualified_name = cx_to_string(clang_getCursorDisplayName(cursor));
        fn.display_name   = fn.qualified_name;

        CXSourceLocation loc = clang_getCursorLocation(cursor);
        CXFile file;
        unsigned line, col;
        clang_getFileLocation(loc, &file, &line, &col, nullptr);
        fn.file   = file ? cx_to_string(clang_getFileName(file)) : "";
        fn.line   = static_cast<int>(line);
        fn.column = static_cast<int>(col);

        int num_args = clang_Cursor_getNumArguments(cursor);
        for (int i = 0; i < num_args; ++i) {
            CXCursor param = clang_Cursor_getArgument(cursor, i);
            fn.params.push_back(make_param_info(param));
        }

        // Body analysis: visit children for call sites + local vars + mutation
        if (data->parse_bodies && clang_isCursorDefinition(cursor)) {
            BodyVisitorData bd;
            bd.fn = &fn;
            bd.tu = data->tu;
            for (const auto& p : fn.params) {
                if (!p.name.empty()) bd.param_names.insert(p.name);
            }
            clang_visitChildren(cursor, visit_body, &bd);
            for (auto& p : fn.params) {
                if (bd.mutated.count(p.name)) p.is_mutated = true;
            }
        }

        data->result->functions.push_back(std::move(fn));
        return CXChildVisit_Continue; // body already visited above
    }

    // Record struct/class declarations
    if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
        if (clang_isCursorDefinition(cursor)) {
            TypeDecl td;
            td.qualified_name = cx_to_string(clang_getCursorDisplayName(cursor));

            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file;
            unsigned line;
            clang_getFileLocation(loc, &file, &line, nullptr, nullptr);
            td.file = file ? cx_to_string(clang_getFileName(file)) : "";
            td.line = static_cast<int>(line);

            CXType cxtype = clang_getCursorType(cursor);
            long long sz  = clang_Type_getSizeOf(cxtype);
            td.size_bytes = (sz >= 0) ? sz : -1;
            {
                CXType canonical = clang_getCanonicalType(cxtype);
                CXTypeKind k = canonical.kind;
                td.trivially_copyable =
                    (k >= CXType_FirstBuiltin && k <= CXType_LastBuiltin) ||
                    k == CXType_Enum;
            }

            data->result->types.push_back(std::move(td));
        }
        return CXChildVisit_Recurse; // recurse to find methods inside classes
    }

    return CXChildVisit_Recurse;
}

// public API

ParseResult parse_file(const std::string& filepath,
                       const std::vector<std::string>& compile_args,
                       const ClangParserOptions& opts) {
    ParseResult result;
    result.source_file = filepath;

    // Build argv for libclang
    std::vector<const char*> argv;
    for (const auto& a : compile_args) {
        argv.push_back(a.c_str());
    }
    for (const auto& a : opts.extra_args) {
        argv.push_back(a.c_str());
    }

    CXIndex index = clang_createIndex(0, opts.verbose ? 1 : 0);
    if (!index) {
        result.errors.push_back("Failed to create CXIndex");
        result.ok = false;
        return result;
    }

    unsigned parse_flags = CXTranslationUnit_DetailedPreprocessingRecord;
    if (!opts.parse_bodies) {
        parse_flags |= CXTranslationUnit_SkipFunctionBodies;
    }

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index,
        filepath.c_str(),
        argv.data(),
        static_cast<int>(argv.size()),
        nullptr, 0,
        parse_flags
    );

    if (!tu) {
        result.errors.push_back("Failed to parse: " + filepath);
        result.ok = false;
        clang_disposeIndex(index);
        return result;
    }

    // Collect parse diagnostics
    unsigned num_diags = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < num_diags; ++i) {
        CXDiagnostic diag = clang_getDiagnostic(tu, i);
        CXDiagnosticSeverity sev = clang_getDiagnosticSeverity(diag);
        if (sev >= CXDiagnostic_Error) {
            result.errors.push_back(cx_to_string(
                clang_formatDiagnostic(diag, CXDiagnostic_DisplaySourceLocation)));
            result.ok = false;
        }
        clang_disposeDiagnostic(diag);
    }

    // Visit the AST even when there were errors. libclang still produces a
    // usable AST (IDEs depend on this), and the visitor only records cursors
    // from the main source file — so we recover the user's own functions even
    // when a third-party header fails to compile under Clang. result.ok still
    // reflects whether errors occurred, for reporting.
    {
        VisitorData data{&result, tu, &filepath, opts.parse_bodies};
        CXCursor root = clang_getTranslationUnitCursor(tu);
        clang_visitChildren(root, visit_ast, &data);
    }

    spdlog::debug("Parsed {}: {} functions, {} types, {} errors",
                  filepath,
                  result.functions.size(),
                  result.types.size(),
                  result.errors.size());

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return result;
}

}  // namespace perfguardian
