#include "mock_openai_client.h"

#include "ai/types/enums.h"
#include "ai/types/stream_result.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace test {

// ControllableOpenAIClient implementation
ControllableOpenAIClient::ControllableOpenAIClient(const std::string& api_key,
                                                   const std::string& base_url)
    : predefined_status_code_(200),
      should_fail_(false),
      should_timeout_(false),
      call_count_(0) {
  predefined_response_ = ResponseBuilder::buildSuccessResponse();
}

void ControllableOpenAIClient::setPredefinedResponse(
    const std::string& response,
    int status_code) {
  predefined_response_ = response;
  predefined_status_code_ = status_code;
}

void ControllableOpenAIClient::setShouldFail(bool fail) {
  should_fail_ = fail;
}

void ControllableOpenAIClient::setShouldTimeout(bool timeout) {
  should_timeout_ = timeout;
}

void ControllableOpenAIClient::reset() {
  predefined_response_ = ResponseBuilder::buildSuccessResponse();
  predefined_status_code_ = 200;
  should_fail_ = false;
  should_timeout_ = false;
  call_count_ = 0;
}

GenerateResult ControllableOpenAIClient::generate_text(
    const GenerateOptions& options) {
  last_generate_options_ = options;
  call_count_++;

  if (should_timeout_) {
    return GenerateResult("Network timeout");
  }

  if (should_fail_) {
    return NetworkFailureSimulator::createNetworkErrorResult(
        NetworkFailureSimulator::FailureType::kConnectionRefused);
  }

  if (predefined_status_code_ != 200) {
    return GenerateResult("HTTP " + std::to_string(predefined_status_code_) +
                          " error: " + predefined_response_);
  }

  // Parse predefined response and create result
  try {
    auto json_response = nlohmann::json::parse(predefined_response_);

    GenerateResult result;
    result.text = json_response["choices"][0]["message"]["content"];
    result.finish_reason = kFinishReasonStop;

    if (json_response.contains("usage")) {
      result.usage.prompt_tokens = json_response["usage"]["prompt_tokens"];
      result.usage.completion_tokens =
          json_response["usage"]["completion_tokens"];
      result.usage.total_tokens = json_response["usage"]["total_tokens"];
    }

    if (json_response.contains("id")) {
      result.id = json_response["id"];
    }

    if (json_response.contains("model")) {
      result.model = json_response["model"];
    }

    result.provider_metadata = predefined_response_;

    return result;

  } catch (const std::exception& e) {
    return GenerateResult("Failed to parse response: " + std::string(e.what()));
  }
}

StreamResult ControllableOpenAIClient::stream_text(
    const StreamOptions& options) {
  // Copy just the GenerateOptions part to avoid const function issues
  last_stream_options_ = static_cast<const GenerateOptions&>(options);
  call_count_++;

  // For now, return a simple stream result
  // In a full implementation, this would return a controllable stream
  return StreamResult(nullptr);  // Simplified for testing
}

bool ControllableOpenAIClient::is_valid() const {
  return !should_fail_;
}

std::string ControllableOpenAIClient::provider_name() const {
  return "openai-test";
}

std::vector<std::string> ControllableOpenAIClient::supported_models() const {
  return {"gpt-4o", "gpt-4o-mini", "gpt-4", "gpt-3.5-turbo"};
}

bool ControllableOpenAIClient::supports_model(
    const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string ControllableOpenAIClient::config_info() const {
  return "Test OpenAI Client";
}

GenerateOptions ControllableOpenAIClient::getLastGenerateOptions() const {
  return last_generate_options_;
}

GenerateOptions ControllableOpenAIClient::getLastStreamOptions() const {
  return last_stream_options_;
}

int ControllableOpenAIClient::getCallCount() const {
  return call_count_;
}

// NetworkFailureSimulator implementation
std::string NetworkFailureSimulator::simulateFailure(FailureType type) {
  switch (type) {
    case FailureType::kConnectionTimeout:
      return "Connection timeout after 30 seconds";
    case FailureType::kReadTimeout:
      return "Read timeout after 120 seconds";
    case FailureType::kConnectionRefused:
      return "Connection refused by server";
    case FailureType::kDnsFailure:
      return "DNS resolution failed";
    case FailureType::kSslError:
      return "SSL handshake failed";
    case FailureType::kUnexpectedDisconnect:
      return "Server disconnected unexpectedly";
    default:
      return "Unknown network error";
  }
}

GenerateResult NetworkFailureSimulator::createNetworkErrorResult(
    FailureType type) {
  return GenerateResult("Network error: " + simulateFailure(type));
}

// ResponseBuilder implementation
std::string ResponseBuilder::buildSuccessResponse(const std::string& content,
                                                  const std::string& model,
                                                  int prompt_tokens,
                                                  int completion_tokens) {
  nlohmann::json response = {
      {"id", "chatcmpl-test123"},
      {"object", "chat.completion"},
      {"created", 1234567890},
      {"model", model},
      {"system_fingerprint", "fp_test123"},
      {"choices",
       nlohmann::json::array(
           {{{"index", 0},
             {"message", {{"role", "assistant"}, {"content", content}}},
             {"finish_reason", "stop"},
             {"logprobs", nullptr}}})},
      {"usage",
       {{"prompt_tokens", prompt_tokens},
        {"completion_tokens", completion_tokens},
        {"total_tokens", prompt_tokens + completion_tokens}}}};

  return response.dump();
}

std::string ResponseBuilder::buildErrorResponse(int status_code,
                                                const std::string& error_type,
                                                const std::string& message) {
  nlohmann::json error_response = {{"error",
                                    {{"message", message},
                                     {"type", error_type},
                                     {"param", nullptr},
                                     {"code", nullptr}}}};

  return error_response.dump();
}

std::string ResponseBuilder::buildPartialResponse() {
  return R"({"id":"incomplete","choices":[{"message":{"content":"partial)";  // Deliberately incomplete
}

std::vector<std::string> ResponseBuilder::buildStreamingResponse(
    const std::string& content) {
  std::vector<std::string> events;

  // Split content into words for streaming
  std::istringstream iss(content);
  std::string word;
  while (std::getline(iss, word, ' ')) {
    nlohmann::json chunk = {
        {"id", "chatcmpl-stream123"},
        {"object", "chat.completion.chunk"},
        {"created", 1234567890},
        {"model", "gpt-4o"},
        {"choices",
         nlohmann::json::array({{{"index", 0},
                                 {"delta", {{"content", word + " "}}},
                                 {"finish_reason", nullptr}}})}};
    events.push_back("data: " + chunk.dump() + "\n\n");
  }

  // Final chunk with finish_reason
  nlohmann::json final_chunk = {
      {"id", "chatcmpl-stream123"},
      {"object", "chat.completion.chunk"},
      {"created", 1234567890},
      {"model", "gpt-4o"},
      {"choices",
       nlohmann::json::array(
           {{{"index", 0}, {"delta", {}}, {"finish_reason", "stop"}}})}};
  events.push_back("data: " + final_chunk.dump() + "\n\n");
  events.push_back("data: [DONE]\n\n");

  return events;
}

std::string ResponseBuilder::buildRateLimitResponse() {
  return buildErrorResponse(
      429, "rate_limit_exceeded",
      "Rate limit reached for requests. Please try again later.");
}

std::string ResponseBuilder::buildAuthErrorResponse() {
  return buildErrorResponse(401, "invalid_request_error",
                            "Incorrect API key provided");
}

std::string ResponseBuilder::buildModelNotFoundResponse() {
  return buildErrorResponse(404, "model_not_found",
                            "The model 'invalid-model' does not exist");
}

}  // namespace test
}  // namespace ai