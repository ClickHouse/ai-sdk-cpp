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

# Add third party directory with submodules
add_subdirectory(third_party)

# Find system OpenSSL
find_package(OpenSSL REQUIRED)

# Helper function to configure component libraries
function(configure_ai_component target_name)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endfunction()

# Common compile definitions for HTTP support
set(HTTPLIB_COMPILE_DEFS
    CPPHTTPLIB_OPENSSL_SUPPORT=1
    CPPHTTPLIB_BROTLI_SUPPORT=1
    CPPHTTPLIB_THREAD_POOL_COUNT=8
)

# Create libraries
add_library(ai-sdk-cpp-core)
add_library(ai-sdk-cpp-openai)
add_library(ai-sdk-cpp-anthropic)
add_library(ai-sdk-cpp INTERFACE)

# Set library aliases
add_library(ai::sdk ALIAS ai-sdk-cpp)
add_library(ai::core ALIAS ai-sdk-cpp-core)
add_library(ai::openai ALIAS ai-sdk-cpp-openai)
add_library(ai::anthropic ALIAS ai-sdk-cpp-anthropic)

# Configure all component libraries
configure_ai_component(ai-sdk-cpp-core)
configure_ai_component(ai-sdk-cpp-openai)
configure_ai_component(ai-sdk-cpp-anthropic)

# Configure main interface library
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

# Provider-specific sources
set(OPENAI_SOURCES
    src/providers/openai/openai_client.cpp
    src/providers/openai/openai_request_builder.cpp
    src/providers/openai/openai_response_parser.cpp
    src/providers/openai/openai_stream.cpp
    src/providers/openai/openai_factory.cpp
)

set(ANTHROPIC_SOURCES
    src/providers/anthropic/anthropic_client.cpp
    src/providers/anthropic/anthropic_request_builder.cpp
    src/providers/anthropic/anthropic_response_parser.cpp
    src/providers/anthropic/anthropic_stream.cpp
    src/providers/anthropic/anthropic_factory.cpp
)

target_sources(ai-sdk-cpp-openai PRIVATE ${OPENAI_SOURCES})
target_sources(ai-sdk-cpp-anthropic PRIVATE ${ANTHROPIC_SOURCES})

set(GEMINI_SOURCES
    src/providers/gemini/gemini_client.cpp
    src/providers/gemini/gemini_request_builder.cpp
    src/providers/gemini/gemini_response_parser.cpp
    src/providers/gemini/gemini_factory.cpp
)

add_library(ai-sdk-cpp-gemini)
add_library(ai::gemini ALIAS ai-sdk-cpp-gemini)
target_sources(ai-sdk-cpp-gemini PRIVATE ${GEMINI_SOURCES})
configure_ai_component(ai-sdk-cpp-gemini)

# Link gemini into main interface
target_link_libraries(ai-sdk-cpp INTERFACE ai::gemini)
target_compile_definitions(ai-sdk-cpp INTERFACE AI_SDK_HAS_GEMINI=1)

# Link dependencies
target_link_libraries(ai-sdk-cpp-core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        $<BUILD_INTERFACE:httplib::httplib>
        $<BUILD_INTERFACE:OpenSSL::SSL>
        $<BUILD_INTERFACE:OpenSSL::Crypto>
        $<BUILD_INTERFACE:concurrentqueue>
)

# Provider components link to core and share common dependencies
foreach(provider openai anthropic)
    target_link_libraries(ai-sdk-cpp-${provider}
        PUBLIC
            ai::core
            nlohmann_json::nlohmann_json
        PRIVATE
            $<BUILD_INTERFACE:httplib::httplib>
            $<BUILD_INTERFACE:concurrentqueue>
    )
    
    # Set component availability and HTTP definitions
    string(TOUPPER ${provider} PROVIDER_UPPER)
    target_compile_definitions(ai-sdk-cpp-${provider}
        PUBLIC
            AI_SDK_HAS_${PROVIDER_UPPER}=1
        PRIVATE
            ${HTTPLIB_COMPILE_DEFS}
    )
endforeach()

# Configure gemini component
target_link_libraries(ai-sdk-cpp-gemini
    PUBLIC
        ai::core
        nlohmann_json::nlohmann_json
    PRIVATE
        $<BUILD_INTERFACE:httplib::httplib>
        $<BUILD_INTERFACE:concurrentqueue>
)
target_compile_definitions(ai-sdk-cpp-gemini
    PUBLIC
        AI_SDK_HAS_GEMINI=1
    PRIVATE
        ${HTTPLIB_COMPILE_DEFS}
)

# Core needs HTTP definitions too
target_compile_definitions(ai-sdk-cpp-core
    PRIVATE
        ${HTTPLIB_COMPILE_DEFS}
)

# Main library links all components
target_link_libraries(ai-sdk-cpp
    INTERFACE
        ai::core
        ai::openai
        ai::anthropic
)

# Define all component availability for main library
target_compile_definitions(ai-sdk-cpp
    INTERFACE
        AI_SDK_HAS_OPENAI=1
        AI_SDK_HAS_ANTHROPIC=1
)

# List of all concrete component targets
set(COMPONENT_TARGETS ai-sdk-cpp-core ai-sdk-cpp-openai ai-sdk-cpp-anthropic ai-sdk-cpp-gemini)

# Common compile options
if(MSVC)
    set(COMMON_WARNINGS /W4)
    set(COMMON_PLATFORM_DEFS WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
    set(DEBUG_FLAGS)
    set(RELEASE_FLAGS)
else()
    set(COMMON_WARNINGS -Wall -Wextra -Wpedantic)
    set(DEBUG_FLAGS -g -O0)
    set(RELEASE_FLAGS -O3)
    if(APPLE)
        list(APPEND COMMON_WARNINGS -stdlib=libc++)
    endif()
endif()

# Apply common options to all components
foreach(target ${COMPONENT_TARGETS})
    target_compile_options(${target} PRIVATE 
        ${COMMON_WARNINGS}
        $<$<CONFIG:Debug>:${DEBUG_FLAGS}>
        $<$<CONFIG:Release>:${RELEASE_FLAGS}>
    )
    
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:AI_SDK_DEBUG=1>
        $<$<CONFIG:Release>:AI_SDK_RELEASE=1;NDEBUG>
        $<$<BOOL:${MSVC}>:${COMMON_PLATFORM_DEFS}>
    )
endforeach()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)

# Install all targets (httplib is internal only, not installed)
set(ALL_TARGETS ${COMPONENT_TARGETS} ai-sdk-cpp nlohmann_json)
install(TARGETS ${ALL_TARGETS}
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