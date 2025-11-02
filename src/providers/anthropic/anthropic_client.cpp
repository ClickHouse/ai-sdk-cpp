#include "anthropic_client.h"

#include "ai/anthropic.h"
#include "ai/logger.h"
#include "anthropic_request_builder.h"
#include "anthropic_response_parser.h"
#include "anthropic_stream.h"

#include <algorithm>
#include <memory>

namespace ai {
namespace anthropic {

AnthropicClient::AnthropicClient(const std::string& api_key,
                                 const std::string& base_url)
    : BaseProviderClient(
          providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              .completions_endpoint_path = "/v1/messages",
              .embeddings_endpoint_path = "/v1/embeddings",
              .auth_header_name = "x-api-key",
              .auth_header_prefix = "",
              .extra_headers = {{"anthropic-version", "2023-06-01"}}},
          std::make_unique<AnthropicRequestBuilder>(),
          std::make_unique<AnthropicResponseParser>()) {
  ai::logger::log_debug("Anthropic client initialized with base_url: {}",
                        base_url);
}

StreamResult AnthropicClient::stream_text(const StreamOptions& options) {
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
  auto impl = std::make_unique<AnthropicStreamImpl>();
  impl->start_stream(config_.base_url + config_.completions_endpoint_path,
                     headers, request_json);

  ai::logger::log_info("Text streaming started - model: {}", options.model);

  // Return StreamResult with implementation
  return StreamResult(std::move(impl));
}

std::string AnthropicClient::provider_name() const {
  return "anthropic";
}

std::vector<std::string> AnthropicClient::supported_models() const {
  return {"claude-sonnet-4-5-20250929",   // Latest Sonnet 4.5
          "claude-haiku-4-5-20251001",    // Latest Haiku 4.5
          "claude-opus-4-1-20250805",     // Latest Opus 4.1
          "claude-sonnet-4-20250514",     // Sonnet 4.0
          "claude-3-5-sonnet-20241022",   // Legacy 3.5 Sonnet
          "claude-3-5-haiku-20241022",    // Legacy 3.5 Haiku
          "claude-3-opus-20240229", "claude-3-sonnet-20240229",
          "claude-3-haiku-20240307"};
}

bool AnthropicClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string AnthropicClient::config_info() const {
  return "Anthropic API (base_url: " + config_.base_url + ")";
}

std::string AnthropicClient::default_model() const {
  return models::kDefaultModel;
}

}  // namespace anthropic
}  // namespace ai
