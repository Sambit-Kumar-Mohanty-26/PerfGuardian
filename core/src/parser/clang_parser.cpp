#include "perfguardian/clang_parser.hpp"
#include <clang-c/Index.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
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

static ParamInfo make_param_info(CXCursor param_cursor) {
    ParamInfo p;
    p.name = cx_to_string(clang_getCursorSpelling(param_cursor));

    CXType cxtype = clang_getCursorType(param_cursor);
    p.type_spelling = cx_to_string(clang_getTypeSpelling(cxtype));

    CXTypeKind kind = cxtype.kind;
    p.is_reference   = (kind == CXType_LValueReference);
    p.is_rvalue_ref  = (kind == CXType_RValueReference);
    p.is_pointer     = (kind == CXType_Pointer);
    p.is_const       = clang_isConstQualifiedType(cxtype) != 0;

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
    FunctionDecl* fn;
    int           loop_depth = 0;
    bool          in_main_file = true; // already filtered by caller
};

// Forward-declare so visit_body can call itself recursively for loops
static CXChildVisitResult visit_body(CXCursor cursor, CXCursor parent,
                                      CXClientData client_data);

static CXChildVisitResult visit_body(CXCursor cursor, CXCursor /*parent*/,
                                      CXClientData client_data) {
    auto* data = static_cast<BodyVisitorData*>(client_data);
    CXCursorKind kind = clang_getCursorKind(cursor);

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
            CXTypeKind tk    = cxtype.kind;
            lv.is_reference  = (tk == CXType_LValueReference);
            lv.is_pointer    = (tk == CXType_Pointer);
            CXType canonical = clang_getCanonicalType(cxtype);
            long long sz     = clang_Type_getSizeOf(canonical);
            lv.type_size_bytes = (sz >= 0) ? sz : -1;
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

        // Body analysis: visit children for call sites + local vars
        if (data->parse_bodies && clang_isCursorDefinition(cursor)) {
            BodyVisitorData bd{&fn, 0};
            clang_visitChildren(cursor, visit_body, &bd);
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

    if (result.ok) {
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
