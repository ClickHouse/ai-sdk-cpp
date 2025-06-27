# All components example

add_executable(all_components_demo main.cpp)
target_link_libraries(all_components_demo PRIVATE 
    ai::sdk  # This includes all components
    spdlog::spdlog
)

set_target_properties(all_components_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/components/all
)

message(STATUS "All components example will be built in: ${CMAKE_BINARY_DIR}/examples/components/all/")