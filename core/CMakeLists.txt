# libclang import
# We link against libclang via a MinGW-compatible import library on Windows
# and via find_library on Linux/macOS. Clang integration is silently disabled
# if libclang cannot be found rather than failing the configure step.
#
# To regenerate cmake/libclang.dll.a (Windows, only needed when updating LLVM):
#   objdump -p "$LLVM_BIN/libclang.dll" | grep clang_ > cmake/libclang.def
#   dlltool -D libclang.dll -d cmake/libclang.def -l cmake/libclang.dll.a

option(PERFGUARDIAN_ENABLE_CLANG "Enable Clang/LibClang AST parsing" ON)

if(PERFGUARDIAN_ENABLE_CLANG)
    if(WIN32)
        set(_pg_clang_include "C:/Program Files/LLVM/include")
        set(_pg_clang_implib  "${CMAKE_SOURCE_DIR}/cmake/libclang.dll.a")
        set(_pg_clang_dll     "C:/Program Files/LLVM/bin/libclang.dll")
        # Expose DLL path for post-build copy in cli/
        set(PERFGUARDIAN_LIBCLANG_DLL "${_pg_clang_dll}" CACHE INTERNAL "")
        if(NOT EXISTS "${_pg_clang_dll}")
            message(STATUS "PerfGuardian: libclang.dll not found — CLANG integration disabled")
            set(PERFGUARDIAN_ENABLE_CLANG OFF CACHE BOOL "" FORCE)
        endif()
    else()
        # Linux/macOS: locate headers and shared library without requiring
        # CMake config files (not always installed with the dev packages).
        find_path(_pg_clang_include clang-c/Index.h
            HINTS
                ${LLVM_INCLUDE_HINTS}        # caller may set via -DLLVM_INCLUDE_HINTS=...
                /usr/include
                /usr/local/include
                /usr/lib/llvm-20/include
                /usr/lib/llvm-19/include
                /usr/lib/llvm-18/include
                /usr/lib/llvm-17/include
                /usr/lib/llvm-16/include
                /usr/lib/llvm-15/include
                /usr/lib/llvm-14/include
                /opt/homebrew/opt/llvm/include
                /usr/local/opt/llvm/include
        )
        find_library(_pg_clang_lib NAMES clang
            HINTS
                ${LLVM_LIBRARY_HINTS}        # caller may set via -DLLVM_LIBRARY_HINTS=...
                /usr/lib/x86_64-linux-gnu
                /usr/lib/aarch64-linux-gnu
                /usr/lib
                /usr/local/lib
                /usr/lib/llvm-20/lib
                /usr/lib/llvm-19/lib
                /usr/lib/llvm-18/lib
                /usr/lib/llvm-17/lib
                /usr/lib/llvm-16/lib
                /usr/lib/llvm-15/lib
                /usr/lib/llvm-14/lib
                /opt/homebrew/opt/llvm/lib
                /usr/local/opt/llvm/lib
        )
        if(NOT _pg_clang_include OR NOT _pg_clang_lib)
            message(STATUS "PerfGuardian: libclang not found — CLANG integration disabled "
                           "(install llvm-dev / libclang-dev to enable)")
            set(PERFGUARDIAN_ENABLE_CLANG OFF CACHE BOOL "" FORCE)
        else()
            message(STATUS "PerfGuardian: found libclang at ${_pg_clang_lib}")
        endif()
    endif()
endif()

if(PERFGUARDIAN_ENABLE_CLANG)
    add_library(libclang_import SHARED IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(libclang_import PROPERTIES
            IMPORTED_LOCATION             "${_pg_clang_dll}"
            IMPORTED_IMPLIB               "${_pg_clang_implib}"
            INTERFACE_INCLUDE_DIRECTORIES "${_pg_clang_include}"
        )
    else()
        set_target_properties(libclang_import PROPERTIES
            IMPORTED_LOCATION             "${_pg_clang_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${_pg_clang_include}"
        )
    endif()
endif()

# Core compiled library
set(CORE_SOURCES
    src/project_loader/project_loader.cpp
    src/symbol_db/symbol_db.cpp        # Phase 2
    src/rules/rules.cpp                # Phase 3+4
    src/rules/pg001.cpp                # Phase 3
    src/rules/pg002.cpp                # Phase 4
    src/rules/pg003.cpp                # Phase 4
    src/rules/pg004.cpp                # Phase 4
    src/rules/pg005.cpp                # Phase 4
    src/rules/pg006.cpp                # Phase 4
    src/analysis/hotspot.cpp           # Phase 5
    src/analysis/json_report.cpp       # Phase 6
    src/analysis/html_report.cpp       # Phase 7
)

if(PERFGUARDIAN_ENABLE_CLANG)
    list(APPEND CORE_SOURCES src/parser/clang_parser.cpp)
endif()

add_library(perfguardian_core ${CORE_SOURCES})

target_include_directories(perfguardian_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(perfguardian_core PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
)

if(PERFGUARDIAN_ENABLE_CLANG)
    target_link_libraries(perfguardian_core PUBLIC libclang_import)
    target_compile_definitions(perfguardian_core PUBLIC PERFGUARDIAN_CLANG_ENABLED)
endif()
