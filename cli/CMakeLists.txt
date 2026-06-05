add_executable(perfguardian main.cpp)

target_link_libraries(perfguardian PRIVATE
    perfguardian_core
    CLI11::CLI11
)

# Place binary at build root so CI smoke tests have a stable path
set_target_properties(perfguardian PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
