cmake_minimum_required(VERSION 3.16)
project(pg_gen_query LANGUAGES C CXX)

find_package(PostgreSQL REQUIRED)

## The ai-sdk-cpp repo may provide an in-tree CMake alias `ai::sdk`.
## When the SDK is installed it exports targets with the `ai::` prefix
## such as `ai::ai-sdk-cpp`. Check both and prefer the installed name.
## If ai-sdk-cpp was installed to a prefix, try to find the package so the
## exported targets (e.g. `ai::ai-sdk-cpp`) become available to this project.
## Ensure `find_dependency` macro is available for the package config file.
include(CMakeFindDependencyMacro)
find_package(ai-sdk-cpp QUIET)

# Find common system libraries required by the SDK
find_package(OpenSSL QUIET)
find_package(ZLIB QUIET)
find_library(BROTLI_DEC_LIB brotlidec HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib)
find_library(BROTLI_COMMON_LIB brotlicommon HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib)

## The ai-sdk-cpp repo may also provide an in-tree CMake alias `ai::sdk`.
## Prefer the installed target names when available.
if(TARGET ai::ai-sdk-cpp)
  set(AI_SDK_ALIAS_TARGET ai::ai-sdk-cpp)
elseif(TARGET ai::sdk)
  set(AI_SDK_ALIAS_TARGET ai::sdk)
else()
  message(STATUS "ai::sdk or ai::ai-sdk-cpp target not found. Please set AI_SDK_TARGET to your SDK target name.")
endif()

set(SOURCE_FILES pg_gen_query.cpp)

add_library(pg_gen_query SHARED ${SOURCE_FILES})
target_include_directories(pg_gen_query PRIVATE ${PostgreSQL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${CMAKE_SOURCE_DIR}/src)
target_compile_features(pg_gen_query PRIVATE cxx_std_17)

if(TARGET ${AI_SDK_ALIAS_TARGET})
  target_link_libraries(pg_gen_query PRIVATE ${PostgreSQL_LIBRARIES} ${AI_SDK_ALIAS_TARGET})
  # Link common transitive dependencies used by ai-sdk-cpp so symbols
  # from OpenSSL, brotli, zlib and httplib are available at link time.
  if(TARGET OpenSSL::SSL)
    target_link_libraries(pg_gen_query PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  endif()
  if(TARGET httplib::httplib)
    target_link_libraries(pg_gen_query PRIVATE httplib::httplib)
  endif()
  if(TARGET concurrentqueue)
    target_link_libraries(pg_gen_query PRIVATE concurrentqueue)
  endif()
  if(ZLIB_FOUND)
    target_link_libraries(pg_gen_query PRIVATE ZLIB::ZLIB)
  endif()
  if(BROTLI_DEC_LIB)
    target_link_libraries(pg_gen_query PRIVATE ${BROTLI_DEC_LIB})
  endif()
  if(BROTLI_COMMON_LIB)
    target_link_libraries(pg_gen_query PRIVATE ${BROTLI_COMMON_LIB})
  endif()
else()
  # User must provide AI_SDK_TARGET when configuring
  if(DEFINED AI_SDK_TARGET)
    target_link_libraries(pg_gen_query PRIVATE ${PostgreSQL_LIBRARIES} ${AI_SDK_TARGET})
  else()
    message(FATAL_ERROR "AI SDK CMake target not found. Define AI_SDK_TARGET when running cmake.")
  endif()
endif()

set_target_properties(pg_gen_query PROPERTIES
  PREFIX ""
  SUFFIX ".so"
)

# On macOS allow undefined symbols (Postgres provides them at runtime)
if(APPLE)
  set_target_properties(pg_gen_query PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )
endif()
