cmake_minimum_required(VERSION 3.16)

# Wrapper project for concurrentqueue library
project(concurrentqueue_wrapper)

# Add concurrentqueue as subdirectory
add_subdirectory(
    ${AI_SDK_THIRD_PARTY_DIR}/concurrentqueue 
    ${CMAKE_CURRENT_BINARY_DIR}/concurrentqueue
)