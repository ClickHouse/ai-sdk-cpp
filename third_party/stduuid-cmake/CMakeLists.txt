cmake_minimum_required(VERSION 3.16)
project(stduuid_wrapper)

# stduuid is a header-only library; expose it via an INTERFACE target.
add_library(stduuid INTERFACE)
add_library(stduuid::stduuid ALIAS stduuid)

set(STDUUID_BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${STDUUID_BUILD_INCLUDE_DIR})

# Copy uuid.h from the vendored header-only directory.
configure_file(
    ${AI_SDK_THIRD_PARTY_DIR}/stduuid-header-only/uuid.h
    ${STDUUID_BUILD_INCLUDE_DIR}/uuid.h
    COPYONLY
)

target_include_directories(stduuid INTERFACE
    $<BUILD_INTERFACE:${STDUUID_BUILD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# stduuid requires C++17 minimum (uses std::optional, std::string_view).
target_compile_features(stduuid INTERFACE cxx_std_17)
