add_executable(perfguardian main.cpp)

target_link_libraries(perfguardian PRIVATE
    perfguardian_core
    CLI11::CLI11
)

# Place binary at build root so CI smoke tests have a stable path
set_target_properties(perfguardian PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# On Windows, copy libclang.dll next to the binary so it is found at runtime
if(WIN32 AND PERFGUARDIAN_ENABLE_CLANG AND EXISTS "${PERFGUARDIAN_LIBCLANG_DLL}")
    add_custom_command(TARGET perfguardian POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PERFGUARDIAN_LIBCLANG_DLL}"
            "${CMAKE_BINARY_DIR}/bin"
        COMMENT "Copying libclang.dll to bin/"
    )
endif()
