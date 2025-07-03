# Fetch zlib (needed by httplib)
CPMAddPackage(
    NAME zlib
    GITHUB_REPOSITORY madler/zlib
    VERSION 1.3.1
    OPTIONS
        "ZLIB_BUILD_EXAMPLES OFF"
)

# Create ZLIB::ZLIB alias if it doesn't exist
if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_LIBRARIES zlibstatic)
    get_target_property(ZLIB_INCLUDE_DIRS zlibstatic INTERFACE_INCLUDE_DIRECTORIES)
elseif(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_LIBRARIES zlib)
    get_target_property(ZLIB_INCLUDE_DIRS zlib INTERFACE_INCLUDE_DIRECTORIES)
endif()