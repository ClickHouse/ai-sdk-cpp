# Configure brotli build options
set(BROTLI_BUILD_TOOLS OFF CACHE INTERNAL "Disable brotli tools")
set(BROTLI_BUNDLED_MODE ON CACHE INTERNAL "Enable bundled mode")

# Add brotli as subdirectory
add_subdirectory(
    ${AI_SDK_THIRD_PARTY_DIR}/brotli 
    ${CMAKE_CURRENT_BINARY_DIR}/brotli
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