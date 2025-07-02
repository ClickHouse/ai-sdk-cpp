#include "anthropic_client.h"

#include "ai/anthropic.h"
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
              .endpoint_path = "/v1/messages",
              .auth_header_name = "x-api-key",
              .auth_header_prefix = "",
              .extra_headers = {{"anthropic-version", "2023-06-01"}}},
          std::make_unique<AnthropicRequestBuilder>(),
          std::make_unique<AnthropicResponseParser>()) {}

StreamResult AnthropicClient::stream_text(const StreamOptions& options) {
  // Build request with stream: true
  auto request_json = request_builder_->build_request_json(options);
  request_json["stream"] = true;

  // Create headers
  auto headers = request_builder_->build_headers(config_);
  headers.emplace("Accept", "text/event-stream");

  // Create stream implementation
  auto impl = std::make_unique<AnthropicStreamImpl>();
  impl->start_stream(config_.base_url + config_.endpoint_path, headers,
                     request_json);

  // Return StreamResult with implementation
  return StreamResult(std::move(impl));
}

std::string AnthropicClient::provider_name() const {
  return "anthropic";
}

std::vector<std::string> AnthropicClient::supported_models() const {
  return {"claude-3-5-sonnet-20241022", "claude-3-5-haiku-20241022",
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

}  // namespace anthropic
}  // namespace ai