#include "mock_anthropic_client.h"

#include "ai/types/enums.h"
#include "ai/types/stream_result.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace test {

// ControllableAnthropicClient implementation
ControllableAnthropicClient::ControllableAnthropicClient(
    const std::string& /* api_key */,
    const std::string& /* base_url */)
    : predefined_status_code_(200),
      should_fail_(false),
      should_timeout_(false),
      call_count_(0) {
  predefined_response_ = AnthropicResponseBuilder::buildSuccessResponse();
}

void ControllableAnthropicClient::setPredefinedResponse(
    const std::string& response,
    int status_code) {
  predefined_response_ = response;
  predefined_status_code_ = status_code;
}

void ControllableAnthropicClient::setShouldFail(bool fail) {
  should_fail_ = fail;
}

void ControllableAnthropicClient::setShouldTimeout(bool timeout) {
  should_timeout_ = timeout;
}

void ControllableAnthropicClient::reset() {
  predefined_response_ = AnthropicResponseBuilder::buildSuccessResponse();
  predefined_status_code_ = 200;
  should_fail_ = false;
  should_timeout_ = false;
  call_count_ = 0;
}

GenerateResult ControllableAnthropicClient::generate_text(
    const GenerateOptions& options) {
  last_generate_options_ = options;
  call_count_++;

  if (should_timeout_) {
    return GenerateResult("Network timeout");
  }

  if (should_fail_) {
    return AnthropicNetworkFailureSimulator::createNetworkErrorResult(
        AnthropicNetworkFailureSimulator::FailureType::kConnectionRefused);
  }

  if (predefined_status_code_ != 200) {
    return GenerateResult("HTTP " + std::to_string(predefined_status_code_) +
                          " error: " + predefined_response_);
  }

  // Parse predefined response and create result
  try {
    auto json_response = nlohmann::json::parse(predefined_response_);

    GenerateResult result;

    // Anthropic response format
    if (json_response["type"] == "message") {
      result.text = json_response["content"][0]["text"];

      // Map Anthropic stop reasons to our enum
      std::string stop_reason = json_response.value("stop_reason", "");
      if (stop_reason == "end_turn") {
        result.finish_reason = kFinishReasonStop;
      } else if (stop_reason == "max_tokens") {
        result.finish_reason = kFinishReasonLength;
      } else if (stop_reason == "stop_sequence") {
        result.finish_reason = kFinishReasonStop;
      } else if (stop_reason == "tool_use") {
        result.finish_reason = kFinishReasonToolCalls;
      } else {
        result.finish_reason = kFinishReasonError;
      }

      if (json_response.contains("usage")) {
        result.usage.prompt_tokens = json_response["usage"]["input_tokens"];
        result.usage.completion_tokens =
            json_response["usage"]["output_tokens"];
        result.usage.total_tokens =
            result.usage.prompt_tokens + result.usage.completion_tokens;
      }

      if (json_response.contains("id")) {
        result.id = json_response["id"];
      }

      if (json_response.contains("model")) {
        result.model = json_response["model"];
      }

      result.provider_metadata = predefined_response_;
      return result;
    }

    return GenerateResult("Unexpected response format");

  } catch (const std::exception& e) {
    return GenerateResult("Failed to parse response: " + std::string(e.what()));
  }
}

StreamResult ControllableAnthropicClient::stream_text(
    const StreamOptions& options) {
  // Copy the generate options part instead of the whole StreamOptions
  last_generate_options_ = static_cast<const GenerateOptions&>(options);
  call_count_++;

  // For now, return a basic StreamResult
  // In a real implementation, this would return a proper streaming interface
  return StreamResult(nullptr);
}

bool ControllableAnthropicClient::is_valid() const {
  return !should_fail_;
}

std::string ControllableAnthropicClient::provider_name() const {
  return "anthropic";
}

std::vector<std::string> ControllableAnthropicClient::supported_models() const {
  return {"claude-3-5-sonnet-20241022", "claude-3-haiku-20240307",
          "claude-3-opus-20240229", "claude-3-sonnet-20240229"};
}

bool ControllableAnthropicClient::supports_model(
    const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string ControllableAnthropicClient::config_info() const {
  return "Mock Anthropic Client for Testing";
}

GenerateOptions ControllableAnthropicClient::getLastGenerateOptions() const {
  return last_generate_options_;
}

GenerateOptions ControllableAnthropicClient::getLastStreamOptions() const {
  return last_generate_options_;
}

int ControllableAnthropicClient::getCallCount() const {
  return call_count_;
}

// AnthropicNetworkFailureSimulator implementation
std::string AnthropicNetworkFailureSimulator::simulateFailure(
    FailureType type) {
  switch (type) {
    case FailureType::kConnectionTimeout:
      return "Connection timeout";
    case FailureType::kReadTimeout:
      return "Read timeout";
    case FailureType::kConnectionRefused:
      return "Connection refused";
    case FailureType::kDnsFailure:
      return "DNS resolution failed";
    case FailureType::kSslError:
      return "SSL handshake failed";
    case FailureType::kUnexpectedDisconnect:
      return "Unexpected connection disconnect";
    default:
      return "Unknown network error";
  }
}

GenerateResult AnthropicNetworkFailureSimulator::createNetworkErrorResult(
    FailureType type) {
  return GenerateResult(simulateFailure(type));
}

// AnthropicResponseBuilder implementation
std::string AnthropicResponseBuilder::buildSuccessResponse(
    const std::string& content,
    const std::string& model,
    int input_tokens,
    int output_tokens) {
  nlohmann::json response = {
      {"id", "msg_test123"},
      {"type", "message"},
      {"role", "assistant"},
      {"content",
       nlohmann::json::array({{{"type", "text"}, {"text", content}}})},
      {"model", model},
      {"stop_reason", "end_turn"},
      {"stop_sequence", nullptr},
      {"usage",
       {{"input_tokens", input_tokens}, {"output_tokens", output_tokens}}}};
  return response.dump();
}

std::string AnthropicResponseBuilder::buildErrorResponse(
    int /* status_code */,
    const std::string& error_type,
    const std::string& message) {
  nlohmann::json response = {
      {"type", "error"},
      {"error", {{"type", error_type}, {"message", message}}}};
  return response.dump();
}

std::string AnthropicResponseBuilder::buildPartialResponse() {
  return "{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":"
         "\"text\",\"text\":\"Partial";  // Intentionally incomplete
}

std::vector<std::string> AnthropicResponseBuilder::buildStreamingResponse(
    const std::string& content) {
  std::vector<std::string> events;

  // Message start event
  events.push_back(
      "data: "
      "{\"type\":\"message_start\",\"message\":{\"id\":\"msg_stream\",\"type\":"
      "\"message\",\"role\":\"assistant\",\"content\":[]}}\n\n");

  // Content block start
  events.push_back(
      "data: "
      "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{"
      "\"type\":\"text\",\"text\":\"\"}}\n\n");

  // Split content into chunks
  const size_t chunk_size = 5;
  for (size_t i = 0; i < content.length(); i += chunk_size) {
    std::string chunk = content.substr(i, chunk_size);
    nlohmann::json event = {
        {"type", "content_block_delta"},
        {"index", 0},
        {"delta", {{"type", "text_delta"}, {"text", chunk}}}};
    events.push_back("data: " + event.dump() + "\n\n");
  }

  // Content block stop
  events.push_back("data: {\"type\":\"content_block_stop\",\"index\":0}\n\n");

  // Message stop event
  events.push_back("data: {\"type\":\"message_stop\"}\n\n");

  return events;
}

std::string AnthropicResponseBuilder::buildRateLimitResponse() {
  return buildErrorResponse(429, "rate_limit_error", "Rate limit exceeded");
}

std::string AnthropicResponseBuilder::buildAuthErrorResponse() {
  return buildErrorResponse(401, "authentication_error", "Invalid API key");
}

std::string AnthropicResponseBuilder::buildModelNotFoundResponse() {
  return buildErrorResponse(400, "invalid_request_error", "Model not found");
}

std::string AnthropicResponseBuilder::buildMaxTokensRequiredResponse() {
  return buildErrorResponse(400, "invalid_request_error",
                            "max_tokens is required");
}

}  // namespace test
}  // namespace ai