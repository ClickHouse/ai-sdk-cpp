# Bedrock-only component example

if(AI_SDK_ENABLE_BEDROCK)
    add_executable(bedrock_component_demo main.cpp)
    target_link_libraries(bedrock_component_demo PRIVATE 
        ai::core 
        ai::bedrock
    )

    set_target_properties(bedrock_component_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/components/bedrock
    )

    message(STATUS "Bedrock component example will be built in: ${CMAKE_BINARY_DIR}/examples/components/bedrock/")
else()
    message(STATUS "Bedrock component example skipped (AI_SDK_ENABLE_BEDROCK=OFF)")
endif()
