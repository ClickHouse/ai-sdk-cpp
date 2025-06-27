# Examples for AI SDK C++

# Helper function to create examples
function(add_ai_example name source_file)
    add_executable(${name} ${source_file})
    target_link_libraries(${name} PRIVATE ai::sdk spdlog::spdlog)
    
    # Set output directory
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
    
    # Copy to examples directory for easy running
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(EXAMPLE_SUFFIX "_debug")
    else()
        set(EXAMPLE_SUFFIX "")
    endif()
    
    set_target_properties(${name} PROPERTIES
        OUTPUT_NAME "${name}${EXAMPLE_SUFFIX}"
    )
endfunction()

# Basic chat example
add_ai_example(basic_chat basic_chat.cpp)

# Streaming chat example  
add_ai_example(streaming_chat streaming_chat.cpp)

# Multi-provider comparison
add_ai_example(multi_provider multi_provider.cpp)

# Error handling demonstration
add_ai_example(error_handling error_handling.cpp)

# Simple test examples
add_ai_example(test_openai test_openai.cpp)
add_ai_example(test_anthropic test_anthropic.cpp)

# Tool calling examples
add_ai_example(tool_calling_basic tool_calling_basic.cpp)
add_ai_example(tool_calling_multistep tool_calling_multistep.cpp) 
add_ai_example(tool_calling_async tool_calling_async.cpp)
add_ai_example(test_tool_integration test_tool_integration.cpp)

# Component-specific examples
add_subdirectory(components/openai)
add_subdirectory(components/anthropic) 
add_subdirectory(components/all)

# Message to show how to run examples
message(STATUS "Examples will be built in: ${CMAKE_BINARY_DIR}/examples/")
message(STATUS "To run examples:")
message(STATUS "  Set environment variables: OPENAI_API_KEY, ANTHROPIC_API_KEY")
message(STATUS "  Run: ./examples/basic_chat")
message(STATUS "       ./examples/streaming_chat")
message(STATUS "       ./examples/multi_provider")
message(STATUS "       ./examples/error_handling")
message(STATUS "       ./examples/test_openai")
message(STATUS "       ./examples/test_anthropic")
message(STATUS "")
message(STATUS "Component-specific examples:")
message(STATUS "  ./examples/components/openai/openai_component_demo     (OpenAI + Core only)")
message(STATUS "  ./examples/components/anthropic/anthropic_component_demo (Anthropic + Core only)")
message(STATUS "  ./examples/components/all/all_components_demo          (All components)")