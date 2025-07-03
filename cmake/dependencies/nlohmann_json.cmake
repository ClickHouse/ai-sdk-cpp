# Fetch nlohmann/json
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
    OPTIONS 
        "JSON_BuildTests OFF"
        "JSON_Install ON"
)