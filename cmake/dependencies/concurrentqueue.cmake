# Fetch concurrentqueue
CPMAddPackage(
    NAME concurrentqueue
    GITHUB_REPOSITORY cameron314/concurrentqueue
    VERSION 1.0.4
)

# Create an interface library for concurrentqueue if it doesn't exist
if(TARGET concurrentqueue AND NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
elseif(NOT TARGET concurrentqueue)
    add_library(concurrentqueue INTERFACE)
    target_include_directories(concurrentqueue INTERFACE ${concurrentqueue_SOURCE_DIR})
endif()