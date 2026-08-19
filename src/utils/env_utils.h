#pragma once

#include <cstdlib>
#include <optional>
#include <string>

namespace ai {
namespace utils {

/// Read an environment variable, treating unset and empty values as absent.
inline std::optional<std::string> non_empty_env(const char* name) {
  const char* value = std::getenv(name);
  if (!value || *value == '\0') {
    return std::nullopt;
  }
  return std::string(value);
}

}  // namespace utils
}  // namespace ai
