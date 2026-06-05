#include <gtest/gtest.h>
#include <string>
#include "perfguardian/version.hpp"

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
