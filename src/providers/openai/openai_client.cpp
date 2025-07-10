#include "openai_client.h"

#include "ai/logger.h"
#include "ai/openai.h"
#include "openai_request_builder.h"
#include "openai_response_parser.h"
#include "openai_stream.h"

#include <algorithm>
#include <memory>

namespace ai {
namespace openai {

OpenAIClient::OpenAIClient(const std::string& api_key,
                           const std::string& base_url)
    : BaseProviderClient(
          providers::ProviderConfig{.api_key = api_key,
                                    .base_url = base_url,
                                    .endpoint_path = "/v1/chat/completions",
                                    .auth_header_name = "Authorization",
                                    .auth_header_prefix = "Bearer ",
                                    .extra_headers = {}},
          std::make_unique<OpenAIRequestBuilder>(),
          std::make_unique<OpenAIResponseParser>()) {
  ai::logger::log_debug("OpenAI client initialized with base_url: {}",
                        base_url);
}

OpenAIClient::OpenAIClient(const std::string& api_key,
                           const std::string& base_url,
                           const retry::RetryConfig& retry_config)
    : BaseProviderClient(
          providers::ProviderConfig{.api_key = api_key,
                                    .base_url = base_url,
                                    .endpoint_path = "/v1/chat/completions",
                                    .auth_header_name = "Authorization",
                                    .auth_header_prefix = "Bearer ",
                                    .extra_headers = {},
                                    .retry_config = retry_config},
          std::make_unique<OpenAIRequestBuilder>(),
          std::make_unique<OpenAIResponseParser>()) {
  ai::logger::log_debug(
      "OpenAI client initialized with base_url: {} and custom retry config",
      base_url);
}

StreamResult OpenAIClient::stream_text(const StreamOptions& options) {
  ai::logger::log_debug(
      "Starting text streaming - model: {}, prompt length: {}", options.model,
      options.prompt.length());

  // Build request with stream: true
  auto request_json = request_builder_->build_request_json(options);
  request_json["stream"] = true;
  ai::logger::log_debug("Stream request JSON built with stream=true");

  // Create headers
  auto headers = request_builder_->build_headers(config_);
  headers.emplace("Accept", "text/event-stream");

  // Create stream implementation
  auto impl = std::make_unique<OpenAIStreamImpl>();
  impl->start_stream(config_.base_url + config_.endpoint_path, headers,
                     request_json);

  ai::logger::log_info("Text streaming started - model: {}", options.model);

  // Return StreamResult with implementation
  return StreamResult(std::move(impl));
}

std::string OpenAIClient::provider_name() const {
  return "openai";
}

std::vector<std::string> OpenAIClient::supported_models() const {
  return {models::kGpt4o,       models::kGpt4oMini,   models::kGpt4Turbo,
          models::kGpt35Turbo,  models::kGpt4,        "gpt-4-0125-preview",
          "gpt-4-1106-preview", "gpt-3.5-turbo-0125", "gpt-3.5-turbo-1106"};
}

bool OpenAIClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string OpenAIClient::config_info() const {
  return "OpenAI API (base_url: " + config_.base_url + ")";
}

}  // namespace openai
}  // namespace ai