cmake_minimum_required(VERSION 3.16)

project(ai_sdk_third_party)

# Set the third party directory variable for all subdirectories
set(AI_SDK_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Third party directory root")

# Add zlib (needed by httplib)
add_subdirectory(zlib-cmake)

# Add brotli (needed by httplib)
add_subdirectory(brotli-cmake)

# Add nlohmann_json
add_subdirectory(nlohmann_json-cmake)

# Add concurrentqueue
add_subdirectory(concurrentqueue-cmake)