# OpenAI-only component example

add_executable(openai_component_demo main.cpp)
target_link_libraries(openai_component_demo PRIVATE 
    ai::core 
    ai::openai 
    spdlog::spdlog
)

set_target_properties(openai_component_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/components/openai
)

message(STATUS "OpenAI component example will be built in: ${CMAKE_BINARY_DIR}/examples/components/openai/")