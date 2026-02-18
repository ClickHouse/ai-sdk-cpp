#include "bedrock_response_parser.h"

#include "ai/logger.h"

#include <aws/bedrock-runtime/BedrockRuntimeErrors.h>
#include <aws/bedrock-runtime/model/ContentBlock.h>
#include <aws/bedrock-runtime/model/ConverseResult.h>
#include <aws/bedrock-runtime/model/StopReason.h>
#include <aws/bedrock-runtime/model/TokenUsage.h>

namespace ai {
namespace bedrock {

GenerateResult BedrockResponseParser::parse_converse_response(
    const Aws::BedrockRuntime::Model::ConverseResult& result) {
  ai::logger::log_debug("Parsing Bedrock Converse response");

  GenerateResult gen_result;

  // Extract text from output message content blocks
  if (result.GetOutput().MessageHasBeenSet()) {
    const auto& message = result.GetOutput().GetMessage();
    gen_result.text = extract_text(message.GetContent());
    ai::logger::log_debug("Extracted text length: {}", gen_result.text.length());

    // Add assistant response to messages
    if (!gen_result.text.empty()) {
      gen_result.response_messages.push_back(Message::assistant(gen_result.text));
    }
  }

  // Map stop reason to finish reason
  gen_result.finish_reason = map_stop_reason(result.GetStopReason());
  ai::logger::log_debug("Stop reason mapped to: {}",
                        gen_result.finishReasonToString());

  // Extract usage information
  // Note: Always try to get usage - the SDK initializes with zeros if not set
  const auto& usage = result.GetUsage();
  gen_result.usage.prompt_tokens = usage.GetInputTokens();
  gen_result.usage.completion_tokens = usage.GetOutputTokens();
  gen_result.usage.total_tokens =
      gen_result.usage.prompt_tokens + gen_result.usage.completion_tokens;
  
  if (gen_result.usage.total_tokens > 0) {
    ai::logger::log_debug("Token usage - input: {}, output: {}, total: {}",
                          gen_result.usage.prompt_tokens,
                          gen_result.usage.completion_tokens,
                          gen_result.usage.total_tokens);
  }

  return gen_result;
}

GenerateResult BedrockResponseParser::parse_converse_error(
    const Aws::BedrockRuntime::BedrockRuntimeError& error) {
  ai::logger::log_debug("Parsing Bedrock error: {} - {}",
                        error.GetExceptionName(), error.GetMessage());

  GenerateResult result;
  result.finish_reason = kFinishReasonError;

  // Build descriptive error message with AWS error code
  std::string error_message;
  const auto& exception_name = error.GetExceptionName();
  const auto& message = error.GetMessage();

  if (!exception_name.empty()) {
    error_message = exception_name + ": " + message;
  } else {
    // Fallback to error type name if exception name is empty
    error_message = "BedrockError: " + message;
  }

  result.error = error_message;
  result.is_retryable = is_error_retryable(error);

  ai::logger::log_debug("Error is retryable: {}",
                        result.is_retryable.value_or(false));

  return result;
}

bool BedrockResponseParser::is_error_retryable(
    const Aws::BedrockRuntime::BedrockRuntimeError& error) {
  // Get the error type from AWS SDK
  auto error_type = error.GetErrorType();

  // Check for retryable errors based on error type
  switch (error_type) {
    case Aws::BedrockRuntime::BedrockRuntimeErrors::THROTTLING:
      // ThrottlingException - rate limiting
      return true;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::SERVICE_UNAVAILABLE:
      // ServiceUnavailableException - temporary service issue
      return true;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::INTERNAL_FAILURE:
      // InternalServerException - 5xx errors
      return true;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::NETWORK_CONNECTION:
      // Transient network failure
      return true;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::VALIDATION:
      // ValidationException - invalid request
      return false;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::ACCESS_DENIED:
      // AccessDeniedException - permission denied
      return false;

    case Aws::BedrockRuntime::BedrockRuntimeErrors::RESOURCE_NOT_FOUND:
      // ResourceNotFoundException - invalid model ID
      return false;

    default:
      break;
  }

  // Check exception name for additional error types
  const auto& exception_name = error.GetExceptionName();

  // Retryable errors by exception name
  if (exception_name == "ThrottlingException" ||
      exception_name == "ServiceUnavailableException" ||
      exception_name == "InternalServerException") {
    return true;
  }

  // Non-retryable errors by exception name
  if (exception_name == "ValidationException" ||
      exception_name == "AccessDeniedException" ||
      exception_name == "ResourceNotFoundException" ||
      exception_name == "UnrecognizedClientException") {
    return false;
  }

  // Check HTTP response code for 5xx errors
  auto response_code = error.GetResponseCode();
  if (response_code != Aws::Http::HttpResponseCode::REQUEST_NOT_MADE) {
    int status_code = static_cast<int>(response_code);
    if (status_code >= 500 && status_code < 600) {
      return true;
    }
  }

  // Default to non-retryable for unknown errors
  return false;
}

FinishReason BedrockResponseParser::map_stop_reason(
    Aws::BedrockRuntime::Model::StopReason reason) {
  // Map Bedrock stop reasons to SDK FinishReason (from design doc)
  switch (reason) {
    case Aws::BedrockRuntime::Model::StopReason::end_turn:
      return kFinishReasonStop;

    case Aws::BedrockRuntime::Model::StopReason::max_tokens:
      return kFinishReasonLength;

    case Aws::BedrockRuntime::Model::StopReason::content_filtered:
      return kFinishReasonContentFilter;

    case Aws::BedrockRuntime::Model::StopReason::tool_use:
      return kFinishReasonToolCalls;

    case Aws::BedrockRuntime::Model::StopReason::stop_sequence:
      // stop_sequence is similar to end_turn - normal completion
      return kFinishReasonStop;

    case Aws::BedrockRuntime::Model::StopReason::guardrail_intervened:
      // Guardrail intervention is a form of content filtering
      return kFinishReasonContentFilter;

    default:
      // For unknown stop reasons, default to stop
      return kFinishReasonStop;
  }
}

std::string BedrockResponseParser::extract_text(
    const Aws::Vector<Aws::BedrockRuntime::Model::ContentBlock>& blocks) {
  std::string full_text;

  for (const auto& block : blocks) {
    // Check if this is a text block and extract the text
    if (block.TextHasBeenSet()) {
      full_text += block.GetText();
    }
    // Note: Tool use blocks are not supported in V1
  }

  return full_text;
}

}  // namespace bedrock
}  // namespace ai
