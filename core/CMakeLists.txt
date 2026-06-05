# ── libclang import (Phase 1) ─────────────────────────────────────────────────
# We link against libclang via a MinGW-compatible import library generated from
# the DLL exports. The DLL itself ships with LLVM.
#
# To regenerate cmake/libclang.dll.a (only needed when updating LLVM):
#   objdump -p "$LLVM_BIN/libclang.dll" | grep clang_ -> cmake/libclang.def
#   dlltool -D libclang.dll -d cmake/libclang.def -l cmake/libclang.dll.a

option(PERFGUARDIAN_ENABLE_CLANG "Enable Clang/LibClang AST parsing" ON)

if(PERFGUARDIAN_ENABLE_CLANG)
    # Locate LLVM headers and DLL
    if(WIN32)
        set(LLVM_INCLUDE_HINT "C:/Program Files/LLVM/include")
        set(LIBCLANG_IMPORT   "${CMAKE_SOURCE_DIR}/cmake/libclang.dll.a")
        set(LIBCLANG_DLL      "C:/Program Files/LLVM/bin/libclang.dll")
    else()
        # Linux/macOS: use system LLVM via find_package or pkg-config
        find_package(LLVM REQUIRED CONFIG)
        find_package(Clang REQUIRED CONFIG)
        set(LLVM_INCLUDE_HINT "${LLVM_INCLUDE_DIRS}")
        set(LIBCLANG_IMPORT   "clang")
    endif()

    add_library(libclang_import SHARED IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(libclang_import PROPERTIES
            IMPORTED_LOCATION   "${LIBCLANG_DLL}"
            IMPORTED_IMPLIB     "${LIBCLANG_IMPORT}"
            INTERFACE_INCLUDE_DIRECTORIES "${LLVM_INCLUDE_HINT}"
        )
    else()
        set_target_properties(libclang_import PROPERTIES
            IMPORTED_LOCATION "${LIBCLANG_IMPORT}"
            INTERFACE_INCLUDE_DIRECTORIES "${LLVM_INCLUDE_HINT}"
        )
    endif()
endif()

# ── Core compiled library ─────────────────────────────────────────────────────
set(CORE_SOURCES
    src/project_loader/project_loader.cpp
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
