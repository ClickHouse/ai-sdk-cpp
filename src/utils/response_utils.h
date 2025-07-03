#pragma once

#include "ai/logger.h"
#include "ai/types/generate_options.h"

#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace utils {

// Parse a generic error response that follows the common pattern of
// { "error": { "message": "...", "type": "..." } }
inline GenerateResult parse_standard_error_response(
    const std::string& provider_name,
    int status_code,
    const std::string& body) {
  ai::logger::log_debug("Parsing error response - status: {}, body: {}",
                        status_code, body);

  try {
    auto json = nlohmann::json::parse(body);
    if (json.contains("error")) {
      auto& error = json["error"];
      std::string message = error.value("message", "Unknown error");
      std::string type = error.value("type", "");

      std::string full_error = provider_name + " API error (" +
                               std::to_string(status_code) + "): " + message +
                               (type.empty() ? "" : " [" + type + "]");
      ai::logger::log_error("{} API error parsed: {}", provider_name,
                            full_error);

      return GenerateResult(full_error);
    }
  } catch (...) {
    // If JSON parsing fails, return raw error
    ai::logger::log_debug("Failed to parse error response as JSON");
  }

  std::string raw_error =
      "HTTP " + std::to_string(status_code) + " error: " + body;
  ai::logger::log_error("{} API raw error: {}", provider_name, raw_error);

  return GenerateResult(raw_error);
}

}  // namespace utils
}  // namespace ai