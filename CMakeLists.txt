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
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Include CPM package manager
include(cmake/CPM.cmake)

# Include all dependencies
include(cmake/dependencies/nlohmann_json.cmake)
include(cmake/dependencies/openssl.cmake)
include(cmake/dependencies/httplib.cmake)  # This will include zlib and brotli
include(cmake/dependencies/concurrentqueue.cmake)

# Core library target (always built)
add_library(ai-sdk-cpp-core)

# OpenAI component library
add_library(ai-sdk-cpp-openai)

# Anthropic component library  
add_library(ai-sdk-cpp-anthropic)

# Main library target (includes all components) - interface library
add_library(ai-sdk-cpp INTERFACE)

# Set library aliases for consistent naming
add_library(ai::sdk ALIAS ai-sdk-cpp)
add_library(ai::core ALIAS ai-sdk-cpp-core)
add_library(ai::openai ALIAS ai-sdk-cpp-openai)
add_library(ai::anthropic ALIAS ai-sdk-cpp-anthropic)

# Configure core library
target_include_directories(ai-sdk-cpp-core 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Configure OpenAI component library
target_include_directories(ai-sdk-cpp-openai 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Configure Anthropic component library  
target_include_directories(ai-sdk-cpp-anthropic 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Configure main library (includes all components) - interface only
target_include_directories(ai-sdk-cpp 
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Core component sources
target_sources(ai-sdk-cpp-core
    PRIVATE
        # Type implementations
        src/types/generate_options.cpp
        src/types/stream_options.cpp
        src/types/stream_result.cpp
        src/types/message.cpp
        
        # HTTP and base provider infrastructure
        src/http/http_request_handler.cpp
        src/providers/base_provider_client.cpp
        
        # Tool calling implementations
        src/tools/tool_executor.cpp
        src/tools/multi_step_coordinator.cpp
)

# OpenAI component sources
target_sources(ai-sdk-cpp-openai
    PRIVATE
        # OpenAI provider implementations
        src/providers/openai/openai_client.cpp
        src/providers/openai/openai_request_builder.cpp
        src/providers/openai/openai_response_parser.cpp
        src/providers/openai/openai_stream.cpp
        src/providers/openai/openai_factory.cpp
)

# Anthropic component sources  
target_sources(ai-sdk-cpp-anthropic
    PRIVATE
        # Anthropic provider implementations
        src/providers/anthropic/anthropic_client.cpp
        src/providers/anthropic/anthropic_request_builder.cpp
        src/providers/anthropic/anthropic_response_parser.cpp
        src/providers/anthropic/anthropic_stream.cpp
        src/providers/anthropic/anthropic_factory.cpp
)

# Link dependencies for core component
target_link_libraries(ai-sdk-cpp-core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        httplib::httplib
        OpenSSL::SSL
        OpenSSL::Crypto
        concurrentqueue
)

# Link dependencies for OpenAI component
target_link_libraries(ai-sdk-cpp-openai
    PUBLIC
        ai::core
        nlohmann_json::nlohmann_json
    PRIVATE
        httplib::httplib
        concurrentqueue
)

# Define component availability for OpenAI
target_compile_definitions(ai-sdk-cpp-openai
    PUBLIC
        AI_SDK_HAS_OPENAI=1
    PRIVATE
        ${HTTPLIB_DEFINITIONS}
)

# Link dependencies for Anthropic component
target_link_libraries(ai-sdk-cpp-anthropic
    PUBLIC
        ai::core
        nlohmann_json::nlohmann_json
    PRIVATE
        httplib::httplib
        concurrentqueue
)

# Define component availability for Anthropic
target_compile_definitions(ai-sdk-cpp-anthropic
    PUBLIC
        AI_SDK_HAS_ANTHROPIC=1
    PRIVATE
        ${HTTPLIB_DEFINITIONS}
)

# Main library links all components (interface)
target_link_libraries(ai-sdk-cpp
    INTERFACE
        ai::core
        ai::openai
        ai::anthropic
)

# Define all component availability for main library (interface)
target_compile_definitions(ai-sdk-cpp
    INTERFACE
        AI_SDK_HAS_OPENAI=1
        AI_SDK_HAS_ANTHROPIC=1
)

# Define httplib configuration for core (where HTTP is used)
target_compile_definitions(ai-sdk-cpp-core
    PRIVATE
        ${HTTPLIB_DEFINITIONS}
)

# Compiler-specific options for concrete components
target_compile_features(ai-sdk-cpp-core PRIVATE cxx_std_20)
target_compile_features(ai-sdk-cpp-openai PRIVATE cxx_std_20)
target_compile_features(ai-sdk-cpp-anthropic PRIVATE cxx_std_20)

# Set appropriate compile options for concrete components (not interface library)
foreach(target ai-sdk-cpp-core ai-sdk-cpp-openai ai-sdk-cpp-anthropic)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        # Add Windows-specific definitions for better compatibility
        target_compile_definitions(${target} PRIVATE 
            WIN32_LEAN_AND_MEAN 
            NOMINMAX
            _WIN32_WINNT=0x0601  # Windows 7+
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        # Add Unix-specific compile options for better compatibility
        if(APPLE)
            target_compile_options(${target} PRIVATE -stdlib=libc++)
        endif()
    endif()
    
    # Set build-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${target} PRIVATE AI_SDK_DEBUG=1)
        if(NOT MSVC)
            target_compile_options(${target} PRIVATE -g -O0)
        endif()
    else()
        target_compile_definitions(${target} PRIVATE AI_SDK_RELEASE=1)
        if(NOT MSVC)
            target_compile_options(${target} PRIVATE -O3 -DNDEBUG)
        endif()
    endif()
endforeach()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    include(cmake/dependencies/googletest.cmake)
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)

# Install all library components
install(TARGETS ai-sdk-cpp-core ai-sdk-cpp-openai ai-sdk-cpp-anthropic ai-sdk-cpp
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