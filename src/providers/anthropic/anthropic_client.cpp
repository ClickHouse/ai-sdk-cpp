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
  // Both the friendly aliases (e.g. "claude-sonnet-4-5") and their dated
  // snapshot variants (e.g. "claude-sonnet-4-5-20250929") are accepted by the
  // Anthropic API; list both so identifiers in `ai::anthropic::models::*`
  // resolve via `supports_model()`.
  return {"claude-opus-4-7", "claude-sonnet-4-6", "claude-opus-4-6",
          "claude-haiku-4-5", "claude-haiku-4-5-20251001", "claude-opus-4-5",
          "claude-opus-4-5-20251101", "claude-sonnet-4-5",
          "claude-sonnet-4-5-20250929", "claude-opus-4-1",
          "claude-opus-4-1-20250805",
          // Deprecated, retire 2026-06-15:
          "claude-sonnet-4-0", "claude-sonnet-4-20250514", "claude-opus-4-0",
          "claude-opus-4-20250514"};
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
