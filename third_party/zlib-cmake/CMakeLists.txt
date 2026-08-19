# Fetch zlib (needed by httplib compression), pinned to the revision this
# repository previously tracked as a git submodule.
include(FetchContent)

# Configure zlib build options
set(ZLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "Disable zlib examples")

FetchContent_Declare(zlib
    URL https://github.com/madler/zlib/archive/5a82f71ed1dfc0bec044d9702463dbdf84ea3b71.tar.gz
)
FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
    add_subdirectory(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Create ZLIB::ZLIB alias if it doesn't exist
if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB found")
    set(ZLIB_LIBRARIES zlibstatic CACHE STRING "ZLIB libraries")
    get_target_property(ZLIB_INCLUDE_DIRS zlibstatic INTERFACE_INCLUDE_DIRECTORIES)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} CACHE STRING "ZLIB include directories")
elseif(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
    set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB found")
    set(ZLIB_LIBRARIES zlib CACHE STRING "ZLIB libraries")
    get_target_property(ZLIB_INCLUDE_DIRS zlib INTERFACE_INCLUDE_DIRECTORIES)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} CACHE STRING "ZLIB include directories")
endif()
