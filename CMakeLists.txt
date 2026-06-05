cmake_minimum_required(VERSION 3.20...4.3)

project(PerfGuardian
    VERSION 0.1.0
    DESCRIPTION "C++ performance analyzer and automated code reviewer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE tooling and future Clang integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PERFGUARDIAN_BUILD_TESTS "Build test suite" ON)
option(PERFGUARDIAN_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# AddressSanitizer (debug only, Linux/macOS)
if(PERFGUARDIAN_ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

include(cmake/deps.cmake)

add_subdirectory(core)
add_subdirectory(cli)

if(PERFGUARDIAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
