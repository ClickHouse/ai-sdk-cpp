# ClickHouse C++ Client CMake wrapper
# Fetches clickhouse-cpp, pinned to the revision this repository previously
# tracked as a git submodule.

# Only build ClickHouse client for tests
if(NOT BUILD_TESTS)
    return()
endif()

include(FetchContent)

FetchContent_Declare(clickhouse-cpp
    URL https://github.com/ClickHouse/clickhouse-cpp/archive/cae657a672ff09b715d7127b13eb25d63bea01d4.tar.gz
)
FetchContent_GetProperties(clickhouse-cpp)
if(NOT clickhouse-cpp_POPULATED)
    FetchContent_Populate(clickhouse-cpp)
    add_subdirectory(${clickhouse-cpp_SOURCE_DIR} ${clickhouse-cpp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Create an interface target that properly exposes ClickHouse client
add_library(clickhouse-cpp-client INTERFACE)

# Link to the actual ClickHouse library
target_link_libraries(clickhouse-cpp-client
    INTERFACE
        clickhouse-cpp-lib
)

# Set properties for consistency with other dependencies
set_target_properties(clickhouse-cpp-lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Disable warnings for third-party code
if(MSVC)
    target_compile_options(clickhouse-cpp-lib PRIVATE /W0)
else()
    target_compile_options(clickhouse-cpp-lib PRIVATE -w)
endif()

# Create alias for consistent naming
add_library(ClickHouse::Client ALIAS clickhouse-cpp-client)
