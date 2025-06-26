#include "openai_response_parser.h"

#include "../../utils/response_utils.h"

#include <spdlog/spdlog.h>

namespace ai {
namespace openai {

GenerateResult OpenAIResponseParser::parse_success_response(
    const nlohmann::json& response) {
  spdlog::debug("Parsing OpenAI chat completion response");

  GenerateResult result;

  // Extract basic fields
  result.id = response.value("id", "");
  result.model = response.value("model", "");
  result.created = response.value("created", 0);
  result.system_fingerprint = response.value("system_fingerprint", "");

  spdlog::debug("Response ID: {}, Model: {}", result.id.value_or("none"),
                result.model.value_or("unknown"));

  // Extract choices
  if (response.contains("choices") && !response["choices"].empty()) {
    auto& choice = response["choices"][0];

    // Extract message content
    if (choice.contains("message")) {
      auto& message = choice["message"];
      // Handle null content (happens when model makes tool calls)
      if (message.contains("content") && !message["content"].is_null()) {
        result.text = message["content"].get<std::string>();
      } else {
        result.text = "";
      }
      spdlog::debug("Extracted message content - length: {}",
                    result.text.length());

      // Parse tool calls if present
      if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
        spdlog::debug("Found {} tool calls in response",
                      message["tool_calls"].size());

        for (const auto& tool_call_json : message["tool_calls"]) {
          if (tool_call_json.contains("id") &&
              tool_call_json.contains("function") &&
              tool_call_json["function"].contains("name") &&
              tool_call_json["function"].contains("arguments")) {
            std::string call_id = tool_call_json["id"].get<std::string>();
            std::string function_name =
                tool_call_json["function"]["name"].get<std::string>();

            // Handle arguments - they might be null, string, or object
            std::string arguments_str;
            if (tool_call_json["function"]["arguments"].is_null()) {
              arguments_str = "{}";
            } else if (tool_call_json["function"]["arguments"].is_string()) {
              arguments_str =
                  tool_call_json["function"]["arguments"].get<std::string>();
            } else {
              arguments_str = tool_call_json["function"]["arguments"].dump();
            }

            try {
              JsonValue arguments;
              if (arguments_str.empty() || arguments_str == "null") {
                arguments = JsonValue::object();
              } else {
                arguments = JsonValue::parse(arguments_str);
              }
              ToolCall tool_call(call_id, function_name, arguments);
              result.tool_calls.push_back(tool_call);

              spdlog::debug("Parsed tool call: {} with args: {}", function_name,
                            arguments_str);
            } catch (const std::exception& e) {
              spdlog::error("Failed to parse tool call arguments: {}",
                            e.what());
            }
          }
        }
      }

      // Add assistant response to messages
      if (!result.text.empty()) {
        result.response_messages.push_back(
            {kMessageRoleAssistant, result.text});
      }
    }

    // Extract finish reason
    if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
      auto finish_reason_str = choice["finish_reason"].get<std::string>();
      result.finish_reason = parse_finish_reason(finish_reason_str);
      spdlog::debug("Finish reason: {}", finish_reason_str);
    } else {
      result.finish_reason = kFinishReasonStop;  // Default to stop if null or missing
      spdlog::debug("Finish reason was null or missing, defaulting to stop");
    }
  }

  // Extract usage
  if (response.contains("usage")) {
    auto& usage = response["usage"];
    result.usage.prompt_tokens = usage.value("prompt_tokens", 0);
    result.usage.completion_tokens = usage.value("completion_tokens", 0);
    result.usage.total_tokens = usage.value("total_tokens", 0);
    spdlog::debug("Token usage - prompt: {}, completion: {}, total: {}",
                  result.usage.prompt_tokens, result.usage.completion_tokens,
                  result.usage.total_tokens);
  }

  // Store full metadata
  result.provider_metadata = response.dump();

  return result;
}

GenerateResult OpenAIResponseParser::parse_error_response(
    int status_code,
    const std::string& body) {
  return utils::parse_standard_error_response("OpenAI", status_code, body);
}

FinishReason OpenAIResponseParser::parse_finish_reason(
    const std::string& reason) {
  if (reason == "stop") {
    return kFinishReasonStop;
  }
  if (reason == "length") {
    return kFinishReasonLength;
  }
  if (reason == "content_filter") {
    return kFinishReasonContentFilter;
  }
  if (reason == "tool_calls") {
    return kFinishReasonToolCalls;
  }
  return kFinishReasonError;
}

}  // namespace openai
}  // namespace ai