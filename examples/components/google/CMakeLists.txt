# Google-only component example

add_executable(google_component_demo main.cpp)
target_link_libraries(google_component_demo PRIVATE 
    ai::core 
    ai::google
)

set_target_properties(google_component_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/components/google
)

message(STATUS "Google component example will be built in: ${CMAKE_BINARY_DIR}/examples/components/google/")