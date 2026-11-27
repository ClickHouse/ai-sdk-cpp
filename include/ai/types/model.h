#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ai {

struct Model {
  std::string name;
  std::string provider;
  std::optional<std::string> version;

  Model(std::string model_name,
        std::string provider_name,
        std::optional<std::string> model_version = std::nullopt)
      : name(std::move(model_name)),
        provider(std::move(provider_name)),
        version(std::move(model_version)) {}

  std::string full_name() const {
    if (version.has_value()) {
      return name + ":" + version.value();
    }
    return name;
  }

  bool is_valid() const { return !name.empty() && !provider.empty(); }

  bool operator==(const Model& other) const {
    return name == other.name && provider == other.provider &&
           version == other.version;
  }

  bool operator!=(const Model& other) const { return !(*this == other); }
};

/// The model IDs a provider serves to the client's API key, as returned by
/// `Client::list_models`.
struct ModelListResult {
  std::vector<std::string> models;
  std::optional<std::string> error;

  ModelListResult() = default;

  explicit ModelListResult(std::string error_message)
      : error(std::move(error_message)) {}

  bool is_success() const { return !error.has_value(); }

  explicit operator bool() const { return is_success(); }

  std::string error_message() const { return error.value_or(""); }
};

}  // namespace ai