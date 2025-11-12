# PostgreSQL AI SQL Generator
# 
# A production-ready AI-powered SQL generation system for PostgreSQL
# Built with ai-sdk-cpp: https://github.com/ClickHouse/ai-sdk-cpp

cmake_minimum_required(VERSION 3.16)
project(pg_gen_ai VERSION 1.0.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PostgreSQL)
if(NOT PostgreSQL_FOUND)
    # Try to find it manually on macOS
    if(APPLE)
        set(PostgreSQL_INCLUDE_DIRS "/opt/homebrew/opt/postgresql@14/include" "/opt/homebrew/opt/postgresql/include")
        set(PostgreSQL_LIBRARIES "/opt/homebrew/opt/postgresql@14/lib/libpq.dylib" "/opt/homebrew/opt/postgresql/lib/libpq.dylib")
    endif()
endif()

# Build the AI Worker executable
add_executable(ai_worker
    src/ai_worker.cpp
    src/pg_cache.cpp
)

# Link against ai-sdk-cpp and dependencies
target_link_libraries(ai_worker PRIVATE 
    ai::sdk 
    httplib::httplib 
    ${PostgreSQL_LIBRARIES}
)

# Include directories
target_include_directories(ai_worker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PostgreSQL_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(ai_worker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pg_gen_ai
)

# Set output name based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(ai_worker PROPERTIES OUTPUT_NAME "ai_worker_debug")
else()
    set_target_properties(ai_worker PROPERTIES OUTPUT_NAME "ai_worker")
endif()

# Installation rules
install(TARGETS ai_worker
    RUNTIME DESTINATION bin
)

install(FILES 
    sql/cache_schema.sql
    sql/ai_gen_via_worker.sql
    DESTINATION share/pg_gen_ai/sql
)

install(FILES
    systemd/ai_worker.service
    DESTINATION share/pg_gen_ai/systemd
)

install(FILES
    README.md
    docs/QUICKSTART.md
    docs/CACHING.md
    DESTINATION share/pg_gen_ai/docs
)

# Print build information
message(STATUS "")
message(STATUS "PostgreSQL AI SQL Generator Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/pg_gen_ai/ai_worker")
message(STATUS "  PostgreSQL: ${PostgreSQL_LIBRARIES}")
message(STATUS "")
