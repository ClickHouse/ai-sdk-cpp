# Anthropic-only component example

add_executable(anthropic_component_demo main.cpp)
target_link_libraries(anthropic_component_demo PRIVATE 
    ai::core 
    ai::anthropic 
    spdlog::spdlog
)

set_target_properties(anthropic_component_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/components/anthropic
)

message(STATUS "Anthropic component example will be built in: ${CMAKE_BINARY_DIR}/examples/components/anthropic/")