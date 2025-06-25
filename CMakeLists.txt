cmake_minimum_required(VERSION 3.16)

# Project definition
project(ai-sdk-cpp 
    VERSION 0.1.0
    DESCRIPTION "A unified C++ SDK for multiple AI/LLM providers"
    LANGUAGES CXX
)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)

# Find required packages through vcpkg
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-concurrentqueue CONFIG REQUIRED)

# Main library target
add_library(ai-sdk-cpp)

# Set library alias for consistent naming
add_library(ai::sdk ALIAS ai-sdk-cpp)

# Public headers (what users will include)
target_include_directories(ai-sdk-cpp 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files will be added here
target_sources(ai-sdk-cpp
    PRIVATE
        # Type implementations
        src/types/generate_options.cpp
        src/types/stream_options.cpp
        src/types/stream_result.cpp
        src/types/message.cpp
        
        # Provider implementations
        src/providers/openai/openai_client.cpp
        src/providers/openai/openai_stream.cpp
        src/providers/openai/openai_factory.cpp
        src/providers/anthropic_client.cpp
)

# Link dependencies (all private - not exposed to users)
target_link_libraries(ai-sdk-cpp
    PRIVATE
        fmt::fmt
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        httplib::httplib
        OpenSSL::SSL
        OpenSSL::Crypto
        unofficial::concurrentqueue::concurrentqueue
)

# Define httplib configuration
target_compile_definitions(ai-sdk-cpp
    PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT=1
        CPPHTTPLIB_BROTLI_SUPPORT=1
        CPPHTTPLIB_THREAD_POOL_COUNT=8
)

# Compiler-specific options
target_compile_features(ai-sdk-cpp PRIVATE cxx_std_20)

# Set appropriate compile options
if(MSVC)
    target_compile_options(ai-sdk-cpp PRIVATE /W4)
else()
    target_compile_options(ai-sdk-cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set build-specific options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ai-sdk-cpp PRIVATE AI_SDK_DEBUG=1)
    if(NOT MSVC)
        target_compile_options(ai-sdk-cpp PRIVATE -g -O0)
    endif()
else()
    target_compile_definitions(ai-sdk-cpp PRIVATE AI_SDK_RELEASE=1)
    if(NOT MSVC)
        target_compile_options(ai-sdk-cpp PRIVATE -O3 -DNDEBUG)
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)

# Install the library
install(TARGETS ai-sdk-cpp
    EXPORT ai-sdk-cpp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT ai-sdk-cpp-targets
    FILE ai-sdk-cpp-targets.cmake
    NAMESPACE ai::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ai-sdk-cpp
)

# Create and install config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ai-sdk-cpp-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ai-sdk-cpp-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ai-sdk-cpp
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ai-sdk-cpp-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ai-sdk-cpp-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ai-sdk-cpp-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ai-sdk-cpp
)

# Print configuration summary
message(STATUS "")
message(STATUS "ai-sdk-cpp ${PROJECT_VERSION} Configuration Summary")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")