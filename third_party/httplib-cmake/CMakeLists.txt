cmake_minimum_required(VERSION 3.16)
project(httplib_wrapper)

# httplib is a header-only library, so we create an INTERFACE target
add_library(httplib INTERFACE)
add_library(httplib::httplib ALIAS httplib)

# Create a build interface include directory
set(HTTPLIB_BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${HTTPLIB_BUILD_INCLUDE_DIR})

# Copy httplib.h to the build include directory
configure_file(
    ${AI_SDK_THIRD_PARTY_DIR}/httplib/httplib.h
    ${HTTPLIB_BUILD_INCLUDE_DIR}/httplib.h
    COPYONLY
)

# Add include directory (only the header file is exposed)
target_include_directories(httplib INTERFACE
    $<BUILD_INTERFACE:${HTTPLIB_BUILD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Find and link required dependencies
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Link dependencies
target_link_libraries(httplib INTERFACE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Brotli::common
    Brotli::decoder  
    Brotli::encoder
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
)

# Set compile definitions for httplib features
target_compile_definitions(httplib INTERFACE
    CPPHTTPLIB_OPENSSL_SUPPORT
    CPPHTTPLIB_ZLIB_SUPPORT
    CPPHTTPLIB_BROTLI_SUPPORT
    CPPHTTPLIB_THREAD_POOL_COUNT=8
)

# Require C++11 minimum (httplib's requirement)
target_compile_features(httplib INTERFACE cxx_std_11)