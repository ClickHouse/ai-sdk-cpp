# Fetch brotli (needed by httplib)
CPMAddPackage(
    NAME brotli
    GITHUB_REPOSITORY google/brotli
    VERSION 1.1.0
    OPTIONS
        "BROTLI_BUILD_TOOLS OFF"
        "BROTLI_BUNDLED_MODE ON"
)

# Brotli doesn't create the expected aliases, so we need to create them
if(NOT TARGET Brotli::common)
    if(TARGET brotlicommon-static)
        add_library(Brotli::common ALIAS brotlicommon-static)
        add_library(Brotli::decoder ALIAS brotlidec-static)
        add_library(Brotli::encoder ALIAS brotlienc-static)
    elseif(TARGET brotlicommon)
        add_library(Brotli::common ALIAS brotlicommon)
        add_library(Brotli::decoder ALIAS brotlidec)
        add_library(Brotli::encoder ALIAS brotlienc)
    endif()
endif()