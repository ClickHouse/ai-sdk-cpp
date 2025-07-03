#include "anthropic_response_parser.h"

#include "../../utils/response_utils.h"
#include "ai/logger.h"

namespace ai {
namespace anthropic {

GenerateResult AnthropicResponseParser::parse_success_response(
    const nlohmann::json& response) {
  ai::logger::log_debug("Parsing Anthropic messages response");

  GenerateResult result;

  // Extract basic fields
  result.id = response.value("id", "");
  result.model = response.value("model", "");

  ai::logger::log_debug("Response ID: {}, Model: {}",
                        result.id.value_or("none"),
                        result.model.value_or("unknown"));

  // Extract content from the response
  if (response.contains("content") && response["content"].is_array()) {
    std::string full_text;

    for (const auto& content_block : response["content"]) {
      if (content_block.contains("type")) {
        std::string type = content_block["type"];

        if (type == "text" && content_block.contains("text")) {
          full_text += content_block["text"].get<std::string>();
        } else if (type == "tool_use") {
          // Parse Anthropic tool use
          if (content_block.contains("id") && content_block.contains("name") &&
              content_block.contains("input")) {
            std::string call_id = content_block["id"];
            std::string function_name = content_block["name"];
            const JsonValue& arguments = content_block["input"];

            ToolCall tool_call(call_id, function_name, arguments);
            result.tool_calls.push_back(tool_call);

            ai::logger::log_debug(
                "Parsed Anthropic tool call: {} with args: {}", function_name,
                arguments.dump());
          }
        }
      }
    }

    result.text = full_text;
    ai::logger::log_debug(
        "Extracted message content - length: {}, tool calls: {}",
        result.text.length(), result.tool_calls.size());

    // Add assistant response to messages
    if (!result.text.empty()) {
      result.response_messages.push_back({kMessageRoleAssistant, result.text});
    }
  }

  // Extract stop reason
  if (response.contains("stop_reason")) {
    std::string stop_reason = response["stop_reason"];
    result.finish_reason = parse_stop_reason(stop_reason);
    ai::logger::log_debug("Stop reason: {}", stop_reason);
  }

  // Extract usage
  if (response.contains("usage")) {
    auto& usage = response["usage"];
    result.usage.prompt_tokens = usage.value("input_tokens", 0);
    result.usage.completion_tokens = usage.value("output_tokens", 0);
    result.usage.total_tokens =
        result.usage.prompt_tokens + result.usage.completion_tokens;
    ai::logger::log_debug("Token usage - input: {}, output: {}, total: {}",
                          result.usage.prompt_tokens,
                          result.usage.completion_tokens,
                          result.usage.total_tokens);
  }

  // Store full metadata
  result.provider_metadata = response.dump();

  return result;
}

GenerateResult AnthropicResponseParser::parse_error_response(
    int status_code,
    const std::string& body) {
  return utils::parse_standard_error_response("Anthropic", status_code, body);
}

FinishReason AnthropicResponseParser::parse_stop_reason(
    const std::string& reason) {
  if (reason == "end_turn") {
    return kFinishReasonStop;
  }
  if (reason == "max_tokens") {
    return kFinishReasonLength;
  }
  if (reason == "stop_sequence") {
    return kFinishReasonStop;
  }
  if (reason == "tool_use") {
    return kFinishReasonToolCalls;
  }
  return kFinishReasonStop;
}

}  // namespace anthropic
}  // namespace ai