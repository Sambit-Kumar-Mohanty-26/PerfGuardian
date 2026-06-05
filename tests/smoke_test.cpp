#include <gtest/gtest.h>
#include <string>
#include "perfguardian/version.hpp"
#include "perfguardian/parse_result.hpp"
#include "perfguardian/symbol_db.hpp"
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/rules.hpp"
#include "perfguardian/pg001.hpp"

// ── Version header ─────────────────────────────────────────────────────────────

TEST(Version, StringNotEmpty) {
    EXPECT_FALSE(perfguardian::version_string().empty());
}

TEST(Version, StringContainsTag) {
    auto v = perfguardian::version_string();
    EXPECT_NE(v.find("0.1.0"), std::string::npos);
}

TEST(Version, StringContainsBinaryName) {
    auto v = perfguardian::version_string();
    EXPECT_NE(v.find("PerfGuardian"), std::string::npos);
}

TEST(Version, MajorMinorPatch) {
    EXPECT_EQ(perfguardian::version_major, 0);
    EXPECT_EQ(perfguardian::version_minor, 1);
    EXPECT_EQ(perfguardian::version_patch, 0);
}

TEST(Version, ConstexprStr) {
    // version_str must be a literal-compatible constexpr string
    static_assert(perfguardian::version_major == 0);
    static_assert(perfguardian::version_minor == 1);
    static_assert(perfguardian::version_patch == 0);
    SUCCEED();
}

// ── SymbolDB (Phase 2) ────────────────────────────────────────────────────────

namespace {

perfguardian::ParseResult make_result_with_type(const std::string& name,
                                                long long size_bytes) {
    perfguardian::ParseResult r;
    r.source_file = "test.cpp";
    perfguardian::TypeDecl td;
    td.qualified_name = name;
    td.size_bytes     = size_bytes;
    td.file           = "test.cpp";
    td.line           = 1;
    r.types.push_back(td);
    return r;
}

perfguardian::ParseResult make_result_with_function(
    const std::string& fn_name,
    std::vector<perfguardian::ParamInfo> params) {
    perfguardian::ParseResult r;
    r.source_file = "test.cpp";
    perfguardian::FunctionDecl fn;
    fn.qualified_name = fn_name;
    fn.display_name   = fn_name;
    fn.file           = "test.cpp";
    fn.line           = 10;
    fn.params         = std::move(params);
    r.functions.push_back(fn);
    return r;
}

perfguardian::ParamInfo make_param(const std::string& name,
                                   const std::string& type,
                                   long long size,
                                   bool is_ref   = false,
                                   bool is_ptr   = false,
                                   bool is_rref  = false) {
    perfguardian::ParamInfo p;
    p.name            = name;
    p.type_spelling   = type;
    p.type_size_bytes = size;
    p.is_reference    = is_ref;
    p.is_pointer      = is_ptr;
    p.is_rvalue_ref   = is_rref;
    p.is_const        = is_ref;
    return p;
}

}  // namespace

TEST(SymbolDB, StartsEmpty) {
    perfguardian::SymbolDB db;
    EXPECT_TRUE(db.empty());
    EXPECT_EQ(db.function_count(), 0u);
    EXPECT_EQ(db.type_count(),     0u);
}

TEST(SymbolDB, AddTypeAndFind) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_type("Player", 800));

    EXPECT_EQ(db.type_count(), 1u);
    const auto* td = db.find_type("Player");
    ASSERT_NE(td, nullptr);
    EXPECT_EQ(td->size_bytes, 800);
    EXPECT_EQ(td->qualified_name, "Player");
}

TEST(SymbolDB, FindMissingTypeReturnsNull) {
    perfguardian::SymbolDB db;
    EXPECT_EQ(db.find_type("NoSuchType"), nullptr);
}

TEST(SymbolDB, TypeSizeLookup) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_type("Vec3", 12));
    EXPECT_EQ(db.type_size("Vec3"),    12);
    EXPECT_EQ(db.type_size("Missing"), -1);
}

TEST(SymbolDB, DeduplicatesTypesByName) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_type("Player", 800));
    db.add(make_result_with_type("Player", 800));  // duplicate
    EXPECT_EQ(db.type_count(), 1u);
}

TEST(SymbolDB, AccumulatesFunctions) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function("foo", {}));
    db.add(make_result_with_function("bar", {}));
    EXPECT_EQ(db.function_count(), 2u);
}

// ── PG001 (Phase 3) ───────────────────────────────────────────────────────────

TEST(PG001, FlagsLargeByValueParam) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "updatePlayer",
        {make_param("p", "Player", 800)}));  // 800 bytes by value

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id,  "PG001");
    EXPECT_EQ(sink.all()[0].severity, perfguardian::Severity::High);
    EXPECT_NE(sink.all()[0].message.find("Player"), std::string::npos);
    EXPECT_EQ(sink.all()[0].metrics.type_size_bytes, 800);
}

TEST(PG001, SkipsConstRef) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "printPlayer",
        {make_param("p", "Player", 800, /*ref=*/true)}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG001, SkipsPointer) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "resetPlayer",
        {make_param("p", "Player *", 8, /*ref=*/false, /*ptr=*/true)}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG001, SkipsRValueRef) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "consumePlayer",
        {make_param("p", "Player &&", 800, false, false, /*rref=*/true)}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG001, SkipsSmallByValueParam) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "setX",
        {make_param("x", "int", 4)}));  // 4 bytes — well under threshold

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG001, RespectsCustomThreshold) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "fn",
        {make_param("v", "Vec3", 12)}));  // 12 bytes by value

    perfguardian::RuleConfig cfg;
    perfguardian::DiagnosticSink sink_default;
    perfguardian::RulePG001 rule;

    // Default threshold is 16 — 12 bytes should NOT fire
    rule.run(db, sink_default, cfg);
    EXPECT_TRUE(sink_default.empty());

    // Lower threshold to 8 — 12 bytes SHOULD fire
    cfg.pg001_size_threshold_bytes = 8;
    perfguardian::DiagnosticSink sink_low;
    rule.run(db, sink_low, cfg);
    EXPECT_EQ(sink_low.count(), 1u);
}

TEST(PG001, SuggestedFixContainsConstRef) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "updatePlayer",
        {make_param("p", "Player", 800)}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});

    ASSERT_FALSE(sink.empty());
    EXPECT_NE(sink.all()[0].suggested_fix.find("const"), std::string::npos);
    EXPECT_NE(sink.all()[0].suggested_fix.find("&"),     std::string::npos);
}

TEST(PG001, MultipleParamsOnlyFlagsLargeByValue) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function("fn", {
        make_param("big",  "Player",  800),               // BAD
        make_param("ref",  "Player",  800, /*ref=*/true), // ok
        make_param("small","int",     4),                 // ok
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG001 rule;
    rule.run(db, sink, {});
    EXPECT_EQ(sink.count(), 1u);
}

TEST(PG001, MakeDefaultRulesContainsPG001) {
    auto rules = perfguardian::make_default_rules();
    EXPECT_FALSE(rules.empty());
    bool found = false;
    for (const auto& r : rules) {
        if (r->rule_id() == "PG001") { found = true; break; }
    }
    EXPECT_TRUE(found);
}
