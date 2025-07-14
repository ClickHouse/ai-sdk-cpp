# Test executable for AI SDK C++
add_executable(ai_tests
    # Unit tests
    unit/openai_client_test.cpp
    unit/anthropic_client_test.cpp
    unit/types_test.cpp
    unit/openai_stream_test.cpp
    unit/anthropic_stream_test.cpp
    
    # Integration tests
    integration/openai_integration_test.cpp
    integration/anthropic_integration_test.cpp
    integration/tool_calling_integration_test.cpp
    integration/clickhouse_integration_test.cpp
    
    # Utility classes
    utils/mock_openai_client.cpp
    utils/mock_anthropic_client.cpp
    utils/test_fixtures.cpp
)

# Link against the main library and test dependencies
target_link_libraries(ai_tests
    PRIVATE
        ai-sdk-cpp
        httplib::httplib  # Need direct access to httplib for internal headers
        concurrentqueue   # Need direct access to concurrentqueue
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        ClickHouse::Client  # ClickHouse C++ client for integration tests
)

# Include directories for tests
target_include_directories(ai_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/src  # Access to private headers
)

# Set test properties
set_target_properties(ai_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Compiler options for tests (less strict than main library)
if(MSVC)
    target_compile_options(ai_tests PRIVATE /W3)
else()
    target_compile_options(ai_tests PRIVATE -Wall -Wextra)
endif()

# Define preprocessor macros for tests
target_compile_definitions(ai_tests PRIVATE
    AI_SDK_TESTING=1
    $<$<CONFIG:Debug>:AI_SDK_DEBUG_TESTS=1>
    # httplib configuration (must match main library)
    CPPHTTPLIB_OPENSSL_SUPPORT=1
    CPPHTTPLIB_BROTLI_SUPPORT=1
    CPPHTTPLIB_THREAD_POOL_COUNT=8
)

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(ai_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES 
        TIMEOUT 30
)
