#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <string>
#include "perfguardian/version.hpp"
#include "perfguardian/parse_result.hpp"
#include "perfguardian/symbol_db.hpp"
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/rules.hpp"
#include "perfguardian/pg001.hpp"
#include "perfguardian/pg002.hpp"
#include "perfguardian/pg003.hpp"
#include "perfguardian/pg004.hpp"
#include "perfguardian/pg005.hpp"
#include "perfguardian/pg006.hpp"
#include "perfguardian/hotspot.hpp"
#include "perfguardian/json_report.hpp"
#include "perfguardian/html_report.hpp"
#include <nlohmann/json.hpp>

// Version header

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

// ── Phase 4 helpers ───────────────────────────────────────────────────────────

namespace {

perfguardian::CallSite make_call(const std::string& callee,
                                  bool inside_loop = false,
                                  int  loop_depth  = 0,
                                  int  line        = 1) {
    perfguardian::CallSite cs;
    cs.callee      = callee;
    cs.inside_loop = inside_loop;
    cs.loop_depth  = loop_depth;
    cs.line        = line;
    return cs;
}

perfguardian::LocalVar make_local(const std::string& name,
                                   const std::string& type,
                                   long long size,
                                   bool is_ref = false,
                                   bool is_ptr = false) {
    perfguardian::LocalVar lv;
    lv.name            = name;
    lv.type_spelling   = type;
    lv.type_size_bytes = size;
    lv.is_reference    = is_ref;
    lv.is_pointer      = is_ptr;
    lv.line            = 5;
    return lv;
}

perfguardian::ParseResult make_result_with_calls(
    const std::string& fn_name,
    std::vector<perfguardian::CallSite> calls) {
    perfguardian::ParseResult r;
    r.source_file = "test.cpp";
    perfguardian::FunctionDecl fn;
    fn.qualified_name = fn_name;
    fn.file           = "test.cpp";
    fn.line           = 1;
    fn.call_sites     = std::move(calls);
    r.functions.push_back(fn);
    return r;
}

perfguardian::ParseResult make_result_with_locals(
    const std::string& fn_name,
    std::vector<perfguardian::LocalVar> locals) {
    perfguardian::ParseResult r;
    r.source_file = "test.cpp";
    perfguardian::FunctionDecl fn;
    fn.qualified_name = fn_name;
    fn.file           = "test.cpp";
    fn.line           = 1;
    fn.local_vars     = std::move(locals);
    r.functions.push_back(fn);
    return r;
}

}  // namespace

// ── PG002 — missing-const-ref ─────────────────────────────────────────────────

TEST(PG002, FlagsNonConstRefLargeParam) {
    perfguardian::SymbolDB db;
    perfguardian::ParamInfo p;
    p.name            = "player";
    p.type_spelling   = "Player";
    p.type_size_bytes = 800;
    p.is_reference    = true;
    p.is_const        = false;
    p.is_pointer      = false;
    p.is_rvalue_ref   = false;
    db.add(make_result_with_function("updatePlayer", {p}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG002 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id, "PG002");
    EXPECT_EQ(sink.all()[0].severity, perfguardian::Severity::Medium);
    EXPECT_NE(sink.all()[0].suggested_fix.find("const"), std::string::npos);
}

TEST(PG002, SkipsConstRef) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "printPlayer",
        {make_param("p", "Player", 800, /*ref=*/true)}));  // is_const=true via make_param

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG002 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG002, SkipsByValue) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_function(
        "fn", {make_param("p", "Player", 800)}));  // by value, PG001 handles this

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG002 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG002, SkipsSmallNonConstRef) {
    perfguardian::SymbolDB db;
    perfguardian::ParamInfo p;
    p.name            = "x";
    p.type_spelling   = "int";
    p.type_size_bytes = 4;
    p.is_reference    = true;
    p.is_const        = false;
    p.is_pointer      = false;
    p.is_rvalue_ref   = false;
    db.add(make_result_with_function("fn", {p}));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG002 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

// ── PG003 — reserve-before-loop ───────────────────────────────────────────────

TEST(PG003, FlagsPushBackInLoopWithoutReserve) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("buildList", {
        make_call("push_back", /*inside_loop=*/true, 1, 10),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG003 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id, "PG003");
    EXPECT_NE(sink.all()[0].suggested_fix.find("reserve"), std::string::npos);
}

TEST(PG003, OkWhenReservePresent) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("buildList", {
        make_call("reserve",   false),
        make_call("push_back", true, 1),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG003 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG003, OkWhenPushBackOutsideLoop) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("appendOne", {
        make_call("push_back", false),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG003 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG003, FlagsEmplaceBackToo) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("build", {
        make_call("emplace_back", true, 1),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG003 rule;
    rule.run(db, sink, {});
    EXPECT_EQ(sink.count(), 1u);
}

// ── PG004 — find-in-loop ──────────────────────────────────────────────────────

TEST(PG004, FlagsFindInsideLoop) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("lookupAll", {
        make_call("find", true, 1, 20),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG004 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id, "PG004");
    EXPECT_EQ(sink.all()[0].severity, perfguardian::Severity::High);
    EXPECT_EQ(sink.all()[0].metrics.loop_depth, 1);
}

TEST(PG004, SkipsFindOutsideLoop) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("lookup", {
        make_call("find", false),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG004 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG004, FlagsCountInsideLoop) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("fn", {
        make_call("count", true, 2),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG004 rule;
    rule.run(db, sink, {});
    EXPECT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].metrics.loop_depth, 2);
}

// ── PG005 — large-local-copy ──────────────────────────────────────────────────

TEST(PG005, FlagsLargeLocalByValue) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_locals("process", {
        make_local("state", "GameState", 1200),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG005 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id, "PG005");
    EXPECT_EQ(sink.all()[0].severity, perfguardian::Severity::Low);
    EXPECT_EQ(sink.all()[0].metrics.type_size_bytes, 1200);
}

TEST(PG005, SkipsReference) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_locals("process", {
        make_local("state", "GameState", 1200, /*ref=*/true),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG005 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG005, SkipsSmallLocal) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_locals("fn", {
        make_local("x", "int", 4),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG005 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG005, RespectsCustomThreshold) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_locals("fn", {
        make_local("v", "Vec3", 12),
    }));

    perfguardian::RuleConfig cfg;
    perfguardian::DiagnosticSink sink_default;
    perfguardian::RulePG005 rule;

    // Default pg005 threshold is 32 — 12 bytes should NOT fire
    rule.run(db, sink_default, cfg);
    EXPECT_TRUE(sink_default.empty());

    // Lower threshold — 12 bytes SHOULD fire
    cfg.pg005_copy_size_threshold = 8;
    perfguardian::DiagnosticSink sink_low;
    rule.run(db, sink_low, cfg);
    EXPECT_EQ(sink_low.count(), 1u);
}

// ── PG006 — repeated-map-lookup ───────────────────────────────────────────────

TEST(PG006, FlagsRepeatedFind) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("doWork", {
        make_call("find", false, 0, 5),
        make_call("find", false, 0, 8),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG006 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].rule_id, "PG006");
    EXPECT_EQ(sink.all()[0].severity, perfguardian::Severity::Medium);
    EXPECT_EQ(sink.all()[0].metrics.lookup_count, 2);
}

TEST(PG006, SkipsSingleFind) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("doWork", {
        make_call("find", false, 0, 5),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG006 rule;
    rule.run(db, sink, {});
    EXPECT_TRUE(sink.empty());
}

TEST(PG006, FlagsRepeatedOperatorBracket) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("fn", {
        make_call("operator[]", false, 0, 3),
        make_call("operator[]", false, 0, 7),
        make_call("operator[]", false, 0, 11),
    }));

    perfguardian::DiagnosticSink sink;
    perfguardian::RulePG006 rule;
    rule.run(db, sink, {});

    ASSERT_EQ(sink.count(), 1u);
    EXPECT_EQ(sink.all()[0].metrics.lookup_count, 3);
}

TEST(PG006, RespectsMinRepeatConfig) {
    perfguardian::SymbolDB db;
    db.add(make_result_with_calls("fn", {
        make_call("find", false, 0, 5),
        make_call("find", false, 0, 9),
    }));

    // Default min_repeat=2 — should fire
    perfguardian::DiagnosticSink sink2;
    perfguardian::RulePG006 rule;
    rule.run(db, sink2, {});
    EXPECT_EQ(sink2.count(), 1u);

    // Raise threshold to 3 — should NOT fire
    perfguardian::RuleConfig cfg;
    cfg.pg006_min_repeat_count = 3;
    perfguardian::DiagnosticSink sink3;
    rule.run(db, sink3, cfg);
    EXPECT_TRUE(sink3.empty());
}

TEST(PG006, AllRulesRegistered) {
    auto rules = perfguardian::make_default_rules();
    std::vector<std::string> ids;
    for (const auto& r : rules) ids.push_back(std::string(r->rule_id()));
    for (const auto& expected : {"PG001","PG002","PG003","PG004","PG005","PG006"}) {
        EXPECT_NE(std::find(ids.begin(), ids.end(), expected), ids.end())
            << expected << " not registered";
    }
}

// ── Phase 5 — HotspotRanker ───────────────────────────────────────────────────

namespace {

// Build a Diagnostic manually for ranker tests
perfguardian::Diagnostic make_diag(const std::string& rule_id,
                                    const std::string& fn_name,
                                    const std::string& file,
                                    int line,
                                    perfguardian::Severity sev) {
    perfguardian::Diagnostic d;
    d.rule_id       = rule_id;
    d.rule_name     = rule_id + "-name";
    d.severity      = sev;
    d.function_name = fn_name;
    d.location      = {file, line, 0};
    d.message       = "test";
    return d;
}

}  // namespace

TEST(HotspotRanker, EmptySinkProducesEmptyReport) {
    perfguardian::DiagnosticSink sink;
    auto report = perfguardian::rank_hotspots(sink);
    EXPECT_EQ(report.total_issues,        0);
    EXPECT_EQ(report.total_severity_score, 0);
    EXPECT_TRUE(report.top_functions.empty());
    EXPECT_TRUE(report.top_files.empty());
    EXPECT_TRUE(report.rule_summary.empty());
}

TEST(HotspotRanker, TotalCountsAreCorrect) {
    perfguardian::DiagnosticSink sink;
    sink.emit(make_diag("PG001","fn_a","a.cpp",1, perfguardian::Severity::High));
    sink.emit(make_diag("PG002","fn_b","b.cpp",2, perfguardian::Severity::Medium));
    sink.emit(make_diag("PG001","fn_a","a.cpp",3, perfguardian::Severity::High));

    auto report = perfguardian::rank_hotspots(sink);
    EXPECT_EQ(report.total_issues, 3);
    // High=4, Medium=3, High=4 → 11
    EXPECT_EQ(report.total_severity_score, 4 + 3 + 4);
}

TEST(HotspotRanker, FunctionsRankedByScore) {
    perfguardian::DiagnosticSink sink;
    // fn_low: 1 Low issue (score=2)
    sink.emit(make_diag("PG005","fn_low","x.cpp",1, perfguardian::Severity::Low));
    // fn_high: 1 High issue (score=4)
    sink.emit(make_diag("PG001","fn_high","x.cpp",5, perfguardian::Severity::High));
    // fn_med: 1 Medium issue (score=3)
    sink.emit(make_diag("PG002","fn_med","x.cpp",3, perfguardian::Severity::Medium));

    auto report = perfguardian::rank_hotspots(sink);
    ASSERT_GE(report.top_functions.size(), 3u);
    EXPECT_EQ(report.top_functions[0].function_name, "fn_high");
    EXPECT_EQ(report.top_functions[1].function_name, "fn_med");
    EXPECT_EQ(report.top_functions[2].function_name, "fn_low");
}

TEST(HotspotRanker, FunctionAggregatesMultipleIssues) {
    perfguardian::DiagnosticSink sink;
    sink.emit(make_diag("PG001","worker","w.cpp",10, perfguardian::Severity::High));
    sink.emit(make_diag("PG003","worker","w.cpp",20, perfguardian::Severity::Medium));
    sink.emit(make_diag("PG004","worker","w.cpp",30, perfguardian::Severity::High));

    auto report = perfguardian::rank_hotspots(sink);
    ASSERT_EQ(report.top_functions.size(), 1u);
    EXPECT_EQ(report.top_functions[0].issue_count, 3);
    EXPECT_EQ(report.top_functions[0].score, 4 + 3 + 4);  // High+Med+High
    EXPECT_EQ(report.top_functions[0].rule_ids.size(), 3u);
}

TEST(HotspotRanker, FilesAggregatedCorrectly) {
    perfguardian::DiagnosticSink sink;
    sink.emit(make_diag("PG001","fn1","player.cpp",1, perfguardian::Severity::High));
    sink.emit(make_diag("PG002","fn2","player.cpp",5, perfguardian::Severity::Medium));
    sink.emit(make_diag("PG003","fn3","enemy.cpp", 1, perfguardian::Severity::Low));

    auto report = perfguardian::rank_hotspots(sink);
    ASSERT_EQ(report.top_files.size(), 2u);
    // player.cpp has higher score
    EXPECT_NE(report.top_files[0].file.find("player.cpp"), std::string::npos);
    EXPECT_EQ(report.top_files[0].issue_count, 2);
}

TEST(HotspotRanker, RuleSummarySortedByCount) {
    perfguardian::DiagnosticSink sink;
    sink.emit(make_diag("PG001","f","f.cpp",1, perfguardian::Severity::High));
    sink.emit(make_diag("PG001","g","f.cpp",2, perfguardian::Severity::High));
    sink.emit(make_diag("PG001","h","f.cpp",3, perfguardian::Severity::High));
    sink.emit(make_diag("PG004","i","f.cpp",4, perfguardian::Severity::High));

    auto report = perfguardian::rank_hotspots(sink);
    ASSERT_GE(report.rule_summary.size(), 2u);
    EXPECT_EQ(report.rule_summary[0].rule_id, "PG001");
    EXPECT_EQ(report.rule_summary[0].count,   3);
    EXPECT_EQ(report.rule_summary[1].count,   1);
}

TEST(HotspotRanker, TopNLimitsResults) {
    perfguardian::DiagnosticSink sink;
    for (int i = 0; i < 8; ++i) {
        sink.emit(make_diag("PG001",
                             "fn_" + std::to_string(i),
                             "f.cpp", i + 1,
                             perfguardian::Severity::High));
    }

    auto report = perfguardian::rank_hotspots(sink, /*top_n=*/3);
    EXPECT_LE(report.top_functions.size(), 3u);
    EXPECT_LE(report.top_files.size(),     3u);
    EXPECT_EQ(report.total_issues, 8);  // total is unaffected by top_n
}

TEST(HotspotRanker, RuleIdsDeduplicatedPerFunction) {
    perfguardian::DiagnosticSink sink;
    // Same rule fires twice on same function
    sink.emit(make_diag("PG001","fn","f.cpp",1, perfguardian::Severity::High));
    sink.emit(make_diag("PG001","fn","f.cpp",2, perfguardian::Severity::High));

    auto report = perfguardian::rank_hotspots(sink);
    ASSERT_EQ(report.top_functions.size(), 1u);
    EXPECT_EQ(report.top_functions[0].issue_count,   2);
    EXPECT_EQ(report.top_functions[0].rule_ids.size(), 1u);  // deduplicated
}

// ── Phase 6 — JSON report ─────────────────────────────────────────────────────

namespace {

// Build a sink + report for JSON tests
struct JsonTestFixture {
    perfguardian::DiagnosticSink sink;
    perfguardian::AnalysisReport report;

    JsonTestFixture() {
        sink.emit(make_diag("PG001","ns::Player::update","player.cpp",45,
                             perfguardian::Severity::High));
        sink.emit(make_diag("PG003","ns::World::tick",  "world.cpp", 10,
                             perfguardian::Severity::Medium));
        sink.emit(make_diag("PG001","ns::Player::update","player.cpp",60,
                             perfguardian::Severity::High));
        report = perfguardian::rank_hotspots(sink);
    }
};

}  // namespace

TEST(JsonReport, TopLevelKeysPresent) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    EXPECT_TRUE(js.contains("version"));
    EXPECT_TRUE(js.contains("timestamp"));
    EXPECT_TRUE(js.contains("summary"));
    EXPECT_TRUE(js.contains("hotspots"));
    EXPECT_TRUE(js.contains("diagnostics"));
}

TEST(JsonReport, VersionMatchesBinary) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    std::string ver = js["version"].get<std::string>();
    EXPECT_NE(ver.find("0.1.0"), std::string::npos);
}

TEST(JsonReport, DiagnosticsArrayHasCorrectCount) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    EXPECT_EQ(js["diagnostics"].size(), 3u);
}

TEST(JsonReport, SeveritySerializedAsString) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    // All PG001 entries should have "high"
    for (const auto& d : js["diagnostics"]) {
        if (d["rule_id"] == "PG001") {
            EXPECT_EQ(d["severity"].get<std::string>(), "high");
        }
    }
}

TEST(JsonReport, LocationFieldsPresent) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    const auto& first = js["diagnostics"][0];
    EXPECT_TRUE(first.contains("location"));
    EXPECT_TRUE(first["location"].contains("file"));
    EXPECT_TRUE(first["location"].contains("line"));
    EXPECT_TRUE(first["location"].contains("column"));
}

TEST(JsonReport, MetricsFieldPresent) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    const auto& first = js["diagnostics"][0];
    EXPECT_TRUE(first.contains("metrics"));
    EXPECT_TRUE(first["metrics"].contains("type_size_bytes"));
    EXPECT_TRUE(first["metrics"].contains("loop_depth"));
}

TEST(JsonReport, SummaryTotalIssuesCorrect) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    EXPECT_EQ(js["summary"]["total_issues"].get<int>(), 3);
}

TEST(JsonReport, RuleSummaryPopulated) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    const auto& by_rule = js["summary"]["by_rule"];
    EXPECT_GE(by_rule.size(), 2u);  // PG001 + PG003
    bool found_pg001 = false;
    for (const auto& r : by_rule) {
        if (r["rule_id"] == "PG001") {
            found_pg001 = true;
            EXPECT_EQ(r["count"].get<int>(), 2);
        }
    }
    EXPECT_TRUE(found_pg001);
}

TEST(JsonReport, HotspotFunctionsPopulated) {
    JsonTestFixture f;
    auto js = nlohmann::json::parse(perfguardian::to_json_string(f.report, f.sink));
    const auto& fns = js["hotspots"]["functions"];
    EXPECT_GE(fns.size(), 1u);
    EXPECT_TRUE(fns[0].contains("function_name"));
    EXPECT_TRUE(fns[0].contains("score"));
    EXPECT_TRUE(fns[0].contains("rule_ids"));
}

TEST(JsonReport, EmptySinkProducesValidJson) {
    perfguardian::DiagnosticSink empty_sink;
    auto empty_report = perfguardian::rank_hotspots(empty_sink);
    auto js = nlohmann::json::parse(
        perfguardian::to_json_string(empty_report, empty_sink));
    EXPECT_EQ(js["summary"]["total_issues"].get<int>(), 0);
    EXPECT_TRUE(js["diagnostics"].empty());
}

TEST(JsonReport, WriteAndReadFile) {
    JsonTestFixture f;
    const std::string tmp = "perfguardian_test_report.json";
    ASSERT_NO_THROW(perfguardian::write_json_report(tmp, f.report, f.sink));

    std::ifstream in(tmp);
    ASSERT_TRUE(in.is_open());
    auto js = nlohmann::json::parse(in);
    EXPECT_EQ(js["summary"]["total_issues"].get<int>(), 3);

    std::remove(tmp.c_str());
}

// ── Phase 7 — HTML dashboard ──────────────────────────────────────────────────

TEST(HtmlReport, ContainsDoctype) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
}

TEST(HtmlReport, ContainsVersionString) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("0.1.0"), std::string::npos);
}

TEST(HtmlReport, ContainsTotalIssueCount) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    // The summary card shows "3" and the section heading "All Issues (3)"
    EXPECT_NE(html.find("All Issues (3)"), std::string::npos);
}

TEST(HtmlReport, ContainsSeverityClasses) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("sev-high"),   std::string::npos);
    EXPECT_NE(html.find("sev-medium"), std::string::npos);
}

TEST(HtmlReport, ContainsRuleIds) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("PG001"), std::string::npos);
    EXPECT_NE(html.find("PG003"), std::string::npos);
}

TEST(HtmlReport, ContainsFunctionNames) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("ns::Player::update"), std::string::npos);
    EXPECT_NE(html.find("ns::World::tick"),    std::string::npos);
}

TEST(HtmlReport, HtmlEscapesSpecialChars) {
    // Function name with angle brackets should be escaped
    perfguardian::DiagnosticSink sink;
    sink.emit(make_diag("PG001", "foo<Bar>", "x.cpp", 1,
                         perfguardian::Severity::High));
    auto report = perfguardian::rank_hotspots(sink);
    auto html   = perfguardian::to_html_string(report, sink);
    // Raw < and > must not appear inside tag content
    EXPECT_EQ(html.find("foo<Bar>"), std::string::npos);
    EXPECT_NE(html.find("foo&lt;Bar&gt;"), std::string::npos);
}

TEST(HtmlReport, EmptySinkProducesValidHtml) {
    perfguardian::DiagnosticSink empty_sink;
    auto empty_report = perfguardian::rank_hotspots(empty_sink);
    auto html = perfguardian::to_html_string(empty_report, empty_sink);
    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html.find("All Issues (0)"), std::string::npos);
}

TEST(HtmlReport, ContainsEmbeddedStyle) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("<style>"), std::string::npos);
    EXPECT_NE(html.find("</style>"), std::string::npos);
}

TEST(HtmlReport, ContainsEmbeddedScript) {
    JsonTestFixture f;
    auto html = perfguardian::to_html_string(f.report, f.sink);
    EXPECT_NE(html.find("<script>"), std::string::npos);
    EXPECT_NE(html.find("</script>"), std::string::npos);
}

TEST(HtmlReport, WriteAndReadFile) {
    JsonTestFixture f;
    const std::string tmp = "perfguardian_test_report.html";
    ASSERT_NO_THROW(perfguardian::write_html_report(tmp, f.report, f.sink));

    std::ifstream in(tmp);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(content.find("All Issues (3)"),  std::string::npos);

    std::remove(tmp.c_str());
}
