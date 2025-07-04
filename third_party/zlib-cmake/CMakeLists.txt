# Configure zlib build options
set(ZLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "Disable zlib examples")

# Add zlib as subdirectory
add_subdirectory(
    ${AI_SDK_THIRD_PARTY_DIR}/zlib 
    ${CMAKE_CURRENT_BINARY_DIR}/zlib
)

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