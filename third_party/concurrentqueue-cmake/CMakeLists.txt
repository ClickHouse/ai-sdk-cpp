cmake_minimum_required(VERSION 3.16)
project(concurrentqueue_wrapper)

# concurrentqueue is a header-only library, so we create an INTERFACE target
add_library(concurrentqueue INTERFACE)
add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)

# Set the header-only directory path
set(CONCURRENTQUEUE_HEADER_DIR ${AI_SDK_THIRD_PARTY_DIR}/concurrentqueue-header-only)

# Add include directory
target_include_directories(concurrentqueue INTERFACE
    $<BUILD_INTERFACE:${CONCURRENTQUEUE_HEADER_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Require C++11 minimum (concurrentqueue's requirement)
target_compile_features(concurrentqueue INTERFACE cxx_std_11)

# Find and link required dependencies
find_package(Threads REQUIRED)
target_link_libraries(concurrentqueue INTERFACE Threads::Threads)