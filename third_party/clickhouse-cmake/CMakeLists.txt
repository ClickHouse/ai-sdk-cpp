# ClickHouse C++ Client CMake wrapper
# This wrapper provides a consistent interface for the ClickHouse C++ client library

# Only build ClickHouse client for tests
if(NOT BUILD_TESTS)
    return()
endif()

# Add ClickHouse client as subdirectory
add_subdirectory(../clickhouse-cpp clickhouse-cpp EXCLUDE_FROM_ALL)

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