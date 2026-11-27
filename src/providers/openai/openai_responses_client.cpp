#include "openai_responses_client.h"

#include "ai/logger.h"
#include "ai/openai.h"
#include "openai_responses_request_builder.h"
#include "openai_responses_response_parser.h"
#include "openai_responses_stream.h"

#include <memory>

namespace ai {
namespace openai {

namespace {
constexpr const char* kResponsesEndpointPath = "/v1/responses";
}  // namespace

OpenAIResponsesClient::OpenAIResponsesClient(const std::string& api_key,
                                             const std::string& base_url)
    : OpenAIClient(api_key,
                   base_url,
                   kResponsesEndpointPath,
                   std::nullopt,
                   std::make_unique<OpenAIResponsesRequestBuilder>(),
                   std::make_unique<OpenAIResponsesResponseParser>()) {}

OpenAIResponsesClient::OpenAIResponsesClient(
    const std::string& api_key,
    const std::string& base_url,
    const retry::RetryConfig& retry_config)
    : OpenAIClient(api_key,
                   base_url,
                   kResponsesEndpointPath,
                   retry_config,
                   std::make_unique<OpenAIResponsesRequestBuilder>(),
                   std::make_unique<OpenAIResponsesResponseParser>()) {}

StreamResult OpenAIResponsesClient::stream_text(const StreamOptions& options) {
  ai::logger::log_debug(
      "Starting Responses API streaming - model: {}, prompt length: {}",
      options.model, options.prompt.length());

  auto request_json = request_builder_->build_request_json(options);
  // `response.completed` carries the usage, no stream options are needed.
  request_json["stream"] = true;

  auto headers = request_builder_->build_headers(config_);
  headers.emplace("Accept", "text/event-stream");

  auto impl = std::make_unique<OpenAIResponsesStreamImpl>();
  impl->start_stream(config_.base_url + config_.completions_endpoint_path,
                     headers, request_json);

  ai::logger::log_info("Responses API streaming started - model: {}",
                       options.model);
  return StreamResult(std::move(impl));
}

std::string OpenAIResponsesClient::config_info() const {
  return "OpenAI Responses API (base_url: " + config_.base_url + ")";
}

std::string OpenAIResponsesClient::default_model() const {
  return models::kDefaultResponsesModel;
}

}  // namespace openai
}  // namespace ai
