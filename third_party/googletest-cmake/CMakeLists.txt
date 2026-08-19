# Fetch GoogleTest, pinned to the revision this repository previously tracked
# as a git submodule.
include(FetchContent)

# Configure GoogleTest build options
set(INSTALL_GTEST OFF CACHE INTERNAL "Disable GoogleTest installation")
set(gtest_force_shared_crt ON CACHE INTERNAL "Use shared CRT on Windows")

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/3983f67e32fb3e9294487b9d4f9586efa6e5d088.tar.gz
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
