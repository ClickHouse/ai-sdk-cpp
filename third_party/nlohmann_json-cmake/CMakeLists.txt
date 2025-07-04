# Configure nlohmann/json build options
set(JSON_BuildTests OFF CACHE INTERNAL "Disable JSON tests")
set(JSON_Install    ON  CACHE INTERNAL "Enable JSON installation")

# Add nlohmann/json as subdirectory
add_subdirectory(
    ${AI_SDK_THIRD_PARTY_DIR}/nlohmann_json 
    ${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json
)