#pragma once

#include <optional>
#include <string>

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

}  // namespace ai