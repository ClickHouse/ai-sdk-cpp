#include "google_client.h"

#include "ai/google.h"
#include "ai/logger.h"
#include "google_request_builder.h"
#include "google_response_parser.h"
#include "google_stream.h"

#include <exception>
#include <memory>

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

std::string GoogleClient::config_info() const {
  return "Google Gemini API (base_url: " + config_.base_url + ")";
}

std::string GoogleClient::default_model() const {
  return models::kDefaultModel;
}

}  // namespace google
}  // namespace ai
