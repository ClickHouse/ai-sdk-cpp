# Common httplib configuration definitions
set(HTTPLIB_DEFINITIONS
    CPPHTTPLIB_OPENSSL_SUPPORT=1
    CPPHTTPLIB_BROTLI_SUPPORT=1
    CPPHTTPLIB_THREAD_POOL_COUNT=8
)

# Dependencies for httplib
include(${CMAKE_CURRENT_LIST_DIR}/zlib.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/brotli.cmake)

# Fetch cpp-httplib
CPMAddPackage(
    NAME httplib
    GITHUB_REPOSITORY yhirose/cpp-httplib
    VERSION 0.15.3
    OPTIONS
        "HTTPLIB_REQUIRE_OPENSSL ON"
        "HTTPLIB_REQUIRE_ZLIB ON"
        "HTTPLIB_REQUIRE_BROTLI ON"
        "HTTPLIB_COMPILE ON"
)