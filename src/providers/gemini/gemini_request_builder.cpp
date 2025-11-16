#include "gemini_request_builder.h"

#include "ai/logger.h"
#include "utils/message_utils.h"

namespace ai {
namespace gemini {

GeminiRequestBuilder::GeminiRequestBuilder(const providers::ProviderConfig& config)
    : config_(config) {}

nlohmann::json GeminiRequestBuilder::build_request_json(
    const GenerateOptions& options) {
  // If base_url points to Google GL, build the GL payload
  if (config_.base_url.find("generativelanguage.googleapis.com") != std::string::npos) {
    nlohmann::json body_json;
    body_json["contents"] = nlohmann::json::array();
    nlohmann::json content_obj;
    content_obj["parts"] = nlohmann::json::array();
    content_obj["parts"].push_back({{"text", options.prompt.empty() ? options.system : options.prompt}});
    body_json["contents"].push_back(content_obj);
    return body_json;
  }

  // Otherwise, build an OpenAI-compatible request
  nlohmann::json request{{"model", options.model}, {"messages", nlohmann::json::array()}};

  if (!options.messages.empty()) {
    for (const auto& msg : options.messages) {
      nlohmann::json message;
      message["role"] = utils::message_role_to_string(msg.role);
      std::string text_content = msg.get_text();
      if (!text_content.empty()) {
        message["content"] = text_content;
      }
      request["messages"].push_back(message);
    }
  } else {
    if (!options.system.empty()) {
      request["messages"].push_back({{"role", "system"}, {"content", options.system}});
    }
    if (!options.prompt.empty()) {
      request["messages"].push_back({{"role", "user"}, {"content", options.prompt}});
    }
  }

  if (options.temperature) request["temperature"] = *options.temperature;
  if (options.max_tokens) request["max_completion_tokens"] = *options.max_tokens;

  return request;
}

nlohmann::json GeminiRequestBuilder::build_request_json(const EmbeddingOptions& options) {
  nlohmann::json request{{"model", options.model}, {"input", options.input}};
  return request;
}

httplib::Headers GeminiRequestBuilder::build_headers(const providers::ProviderConfig& config) {
  httplib::Headers headers;
  // If configured for Google GL, use X-goog-api-key
  if (config.base_url.find("generativelanguage.googleapis.com") != std::string::npos) {
    headers.emplace("X-goog-api-key", config.api_key);
  } else {
    headers.emplace(config.auth_header_name, config.auth_header_prefix + config.api_key);
  }

  for (const auto& [k, v] : config.extra_headers) headers.emplace(k, v);
  return headers;
}

}  // namespace gemini
}  // namespace ai
