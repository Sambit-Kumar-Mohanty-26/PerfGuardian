# CMake 4.3 rejects CMAKE_POLICY_VERSION_MINIMUM="3" (major-only).
# Force it to a valid major.minor before GoogleTest.cmake reads it.
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "" FORCE)
include(GoogleTest)

# ── Unit tests ────────────────────────────────────────────────────────────────
add_executable(perfguardian_tests
    smoke_test.cpp
)

target_link_libraries(perfguardian_tests PRIVATE
    perfguardian_core
    GTest::gtest_main
)

gtest_discover_tests(perfguardian_tests)

# ── Binary smoke tests via CTest ──────────────────────────────────────────────
add_test(NAME smoke_version
    COMMAND $<TARGET_FILE:perfguardian> --version
)
add_test(NAME smoke_help
    COMMAND $<TARGET_FILE:perfguardian> --help
)
add_test(NAME smoke_list_rules
    COMMAND $<TARGET_FILE:perfguardian> list-rules
)
add_test(NAME smoke_analyze_stub
    COMMAND $<TARGET_FILE:perfguardian> analyze .
)

# --version must contain the version string
set_tests_properties(smoke_version PROPERTIES
    PASS_REGULAR_EXPRESSION "PerfGuardian 0\\.2\\.1"
)
