# Configure GoogleTest build options
set(INSTALL_GTEST OFF CACHE INTERNAL "Disable GoogleTest installation")
set(gtest_force_shared_crt ON CACHE INTERNAL "Use shared CRT on Windows")

# Add GoogleTest as subdirectory
add_subdirectory(
    ${AI_SDK_THIRD_PARTY_DIR}/googletest
    ${CMAKE_CURRENT_BINARY_DIR}/googletest
)