include(FetchContent)

# CMake 4.x removed compatibility with cmake_minimum_required < 3.5.
# This suppresses the error for vendored dependencies (yaml-cpp 0.8.0, etc.)
# that haven't updated their minimum version yet.
# CMake 4.x: tell all FetchContent subprojects to inherit the parent's policy version
# so compiler detection (CMP0025, feature detection) works correctly.
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD CACHE STRING "" FORCE)
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER CACHE STRING "" FORCE)

# ─── CLI11 — argument parsing ──────────────────────────────────────────────────
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLI11)

# ─── nlohmann/json — JSON I/O ──────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ─── spdlog — structured logging ──────────────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ─── yaml-cpp — config file parsing ───────────────────────────────────────────
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_yaml_cpp.cmake"
)
set(YAML_CPP_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS  OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL      OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# ─── GoogleTest — unit testing ─────────────────────────────────────────────────
if(PERFGUARDIAN_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    # Prevent GoogleTest from overriding our compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK            OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
