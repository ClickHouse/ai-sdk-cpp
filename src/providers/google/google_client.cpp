#include "google_client.h"

#include "ai/google.h"
#include "ai/logger.h"
#include "google_request_builder.h"
#include "google_response_parser.h"
#include "google_stream.h"

#include <cstdio>
#include <exception>
#include <memory>
#include <set>
#include <string_view>

namespace ai {
namespace google {

namespace {
constexpr const char* kApiVersionPath = "/v1beta";
}  // namespace

GoogleClient::GoogleClient(const std::string& api_key,
                           const std::string& base_url)
    : GoogleClient(api_key, base_url, std::optional<retry::RetryConfig>{}) {}

GoogleClient::GoogleClient(const std::string& api_key,
                           const std::string& base_url,
                           const retry::RetryConfig& retry_config)
    : GoogleClient(api_key,
                   base_url,
                   std::optional<retry::RetryConfig>(retry_config)) {}

GoogleClient::GoogleClient(const std::string& api_key,
                           const std::string& base_url,
                           std::optional<retry::RetryConfig> retry_config)
    : BaseProviderClient(
          providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              // The model is part of the path; see completions_path().
              .completions_endpoint_path = "",
              .embeddings_endpoint_path = "",
              .auth_header_name = "x-goog-api-key",
              .auth_header_prefix = "",
              .extra_headers = {},
              .retry_config = retry_config},
          std::make_unique<GoogleRequestBuilder>(),
          std::make_unique<GoogleResponseParser>()) {
  ai::logger::log_debug("Google client initialized with base_url: {}{}",
                        base_url,
                        config_.retry_config ? " and custom retry config" : "");
}

std::string GoogleClient::model_path(const std::string& model,
                                     const std::string& method) {
  return std::string(kApiVersionPath) + "/" + model_resource_name(model) + ":" +
         method;
}

std::string GoogleClient::completions_path(
    const GenerateOptions& options) const {
  return model_path(options.model, "generateContent");
}

StreamResult GoogleClient::stream_text(const StreamOptions& options) {
  ai::logger::log_debug(
      "Starting text streaming - model: {}, prompt length: {}", options.model,
      options.prompt.length());

  auto impl = std::make_unique<GoogleStreamImpl>();

  // Report requests that cannot be built (unsupported `response_format`, a
  // tool result without its call) as stream errors, like generate_text does.
  nlohmann::json request_json;
  try {
    request_json = request_builder_->build_request_json(options);
  } catch (const std::exception& e) {
    ai::logger::log_error("Failed to build Gemini stream request: {}",
                          e.what());
    impl->fail(std::string("Exception: ") + e.what());
    return StreamResult(std::move(impl));
  }

  include_thought_summaries(request_json);

  auto headers = request_builder_->build_headers(config_);
  headers.emplace("Accept", "text/event-stream");

  impl->start_stream(config_.base_url +
                         model_path(options.model, "streamGenerateContent") +
                         "?alt=sse",
                     headers, request_json);

  ai::logger::log_info("Text streaming started - model: {}", options.model);
  return StreamResult(std::move(impl));
}

EmbeddingResult GoogleClient::embeddings(const EmbeddingOptions& options) {
  (void)options;
  return EmbeddingResult(
      std::string("Embeddings are not supported by the Google Gemini client"));
}

std::string GoogleClient::provider_name() const {
  return "google";
}

bool GoogleClient::supports_model(const std::string& model_name) const {
  return !model_name.empty();
}

namespace {

std::string percent_encode(const std::string& value) {
  std::string encoded;
  for (const unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded.push_back(static_cast<char>(c));
    } else {
      char buffer[4];
      std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
      encoded += buffer;
    }
  }
  return encoded;
}

}  // namespace

ModelListResult GoogleClient::list_models() {
  // `GET /v1beta/models` returns `{"models": [{"name": "models/<id>", ...}],
  // "nextPageToken": ...}`; the IDs are reported without the `models/`
  // prefix, the way requests take them.
  constexpr std::string_view kNamePrefix = "models/";
  const auto headers = request_builder_->build_headers(config_);
  ModelListResult listed;
  std::set<std::string> seen_tokens;
  std::string page_token;

  while (true) {
    std::string path = "/v1beta/models?pageSize=1000";
    if (!page_token.empty()) {
      path += "&pageToken=" + percent_encode(page_token);
    }

    nlohmann::json json_response;
    if (auto error = get_json(path, headers, json_response)) {
      return ModelListResult(std::move(*error));
    }
    if (!json_response.is_object()) {
      return ModelListResult("Unexpected models response: not an object");
    }

    // An empty last page may omit `models`.
    if (const auto models = json_response.find("models");
        models != json_response.end()) {
      if (!models->is_array()) {
        return ModelListResult(
            "Unexpected models response: `models` is not an array");
      }
      for (const auto& entry : *models) {
        if (!entry.is_object() || !entry.contains("name") ||
            !entry["name"].is_string()) {
          continue;
        }
        std::string name = entry["name"].get<std::string>();
        if (name.starts_with(kNamePrefix)) {
          name.erase(0, kNamePrefix.size());
        }
        listed.models.push_back(std::move(name));
      }
    }

    const auto next = json_response.find("nextPageToken");
    if (next == json_response.end() || !next->is_string() ||
        next->get<std::string>().empty()) {
      break;
    }
    page_token = next->get<std::string>();
    if (!seen_tokens.insert(page_token).second) {
      return ModelListResult("Unexpected models response: page token repeated");
    }
  }

  ai::logger::log_debug("Listed {} Gemini models", listed.models.size());
  return listed;
}

std::string GoogleClient::config_info() const {
  return "Google Gemini API (base_url: " + config_.base_url + ")";
}

std::string GoogleClient::default_model() const {
  return models::kDefaultModel;
}

}  // namespace google
}  // namespace ai
