#pragma once

#include <optional>
#include <string>

namespace ai {

/// Configuration for a specific AI model
struct Model {
  std::string name;  ///< Model identifier (e.g., "gpt-4o", "claude-3-5-sonnet")
  std::string provider;  ///< Provider name (e.g., "openai", "anthropic")
  std::optional<std::string> version;  ///< Optional version specifier

  /// Constructor
  Model(std::string model_name,
        std::string provider_name,
        std::optional<std::string> model_version = std::nullopt)
      : name(std::move(model_name)),
        provider(std::move(provider_name)),
        version(std::move(model_version)) {}

  /// Get full model identifier for API calls
  std::string full_name() const {
    if (version.has_value()) {
      return name + ":" + version.value();
    }
    return name;
  }

  /// Check if model configuration is valid
  bool is_valid() const { return !name.empty() && !provider.empty(); }

  /// Equality comparison
  bool operator==(const Model& other) const {
    return name == other.name && provider == other.provider &&
           version == other.version;
  }

  bool operator!=(const Model& other) const { return !(*this == other); }
};

}  // namespace ai