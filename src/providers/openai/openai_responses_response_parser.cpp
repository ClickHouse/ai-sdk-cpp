#include "openai_responses_response_parser.h"

#include "ai/logger.h"
#include "openai_responses_request_builder.h"

#include <utility>

namespace ai {
namespace openai {

namespace {

std::string string_field(const nlohmann::json& object, const char* key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return "";
  }
  return it->get<std::string>();
}

}  // namespace

Usage OpenAIResponsesResponseParser::parse_usage(const nlohmann::json& usage) {
  Usage result;
  if (!usage.is_object()) {
    return result;
  }
  result.prompt_tokens = usage.value("input_tokens", 0);
  result.completion_tokens = usage.value("output_tokens", 0);
  result.total_tokens = usage.value("total_tokens", 0);
  return result;
}

std::optional<FinishReason> OpenAIResponsesResponseParser::parse_finish_reason(
    const nlohmann::json& response,
    bool has_function_calls,
    std::string& error) {
  const auto status = string_field(response, "status");

  if (status == "completed") {
    return has_function_calls ? kFinishReasonToolCalls : kFinishReasonStop;
  }

  if (status == "incomplete") {
    std::string reason;
    if (const auto it = response.find("incomplete_details");
        it != response.end() && it->is_object()) {
      reason = string_field(*it, "reason");
    }
    if (reason == "max_output_tokens") {
      return kFinishReasonLength;
    }
    if (reason == "content_filter") {
      return kFinishReasonContentFilter;
    }
    error = "OpenAI response incomplete: " +
            (reason.empty() ? std::string("unknown reason") : reason);
    return std::nullopt;
  }

  if (status == "failed") {
    error = "OpenAI response failed";
    if (const auto it = response.find("error");
        it != response.end() && it->is_object()) {
      const auto code = string_field(*it, "code");
      const auto message = string_field(*it, "message");
      if (!code.empty()) {
        error += " (" + code + ")";
      }
      if (!message.empty()) {
        error += ": " + message;
      }
    }
    return std::nullopt;
  }

  error = "OpenAI response has unexpected status: " +
          (status.empty() ? std::string("<missing>") : status);
  return std::nullopt;
}

GenerateResult OpenAIResponsesResponseParser::parse_success_completion_response(
    const nlohmann::json& response) {
  ai::logger::log_debug("Parsing OpenAI Responses API response");

  GenerateResult result;
  result.id = string_field(response, "id");
  result.model = string_field(response, "model");
  if (const auto it = response.find("created_at");
      it != response.end() && it->is_number()) {
    result.created = it->get<int64_t>();
  }
  if (const auto it = response.find("usage"); it != response.end()) {
    result.usage = parse_usage(*it);
  }
  result.provider_metadata = response.dump();

  // The assistant turn, with the output items in their original order.
  MessageContent turn;

  if (const auto output = response.find("output");
      output != response.end() && output->is_array()) {
    for (const auto& item : *output) {
      const auto type = string_field(item, "type");

      if (type == "message") {
        std::string text;
        if (const auto content = item.find("content");
            content != item.end() && content->is_array()) {
          for (const auto& part : *content) {
            const auto part_type = string_field(part, "type");
            if (part_type == "output_text") {
              text += string_field(part, "text");
            } else if (part_type == "refusal") {
              text += string_field(part, "refusal");
            }
          }
        }
        if (!text.empty()) {
          result.text += text;
          turn.emplace_back(TextContentPart{std::move(text)});
        }
      } else if (type == "function_call") {
        const auto call_id = string_field(item, "call_id");
        const auto name = string_field(item, "name");
        const auto arguments_text = string_field(item, "arguments");
        JsonValue arguments;
        try {
          arguments = arguments_text.empty()
                          ? JsonValue::object()
                          : JsonValue::parse(arguments_text);
        } catch (const nlohmann::json::exception& e) {
          return GenerateResult("Failed to parse arguments of the " + name +
                                " function call: " + e.what());
        }

        if (const auto item_id = string_field(item, "id"); !item_id.empty()) {
          turn.emplace_back(ProviderContentPart{
              kResponsesProviderName, {{"type", "function_call"},
                                       {"id", item_id},
                                       {"call_id", call_id}}});
        }
        turn.emplace_back(ToolCallContentPart{call_id, name, arguments});
        result.tool_calls.emplace_back(call_id, name, std::move(arguments));
        ai::logger::log_debug("Parsed function call: {} with args: {}", name,
                              arguments_text);
      } else if (type == "reasoning") {
        // Reasoning items must accompany the function calls they led to when
        // the outputs are sent back; keep them for replay.
        turn.emplace_back(ProviderContentPart{kResponsesProviderName, item});
      } else {
        ai::logger::log_debug("Ignoring Responses API output item of type: {}",
                              type);
      }
    }
  }

  std::string error;
  const auto finish_reason =
      parse_finish_reason(response, !result.tool_calls.empty(), error);
  if (!finish_reason) {
    ai::logger::log_error("{}", error);
    result.error = std::move(error);
    result.finish_reason = kFinishReasonError;
    return result;
  }
  result.finish_reason = *finish_reason;

  // A turn of reasoning alone (cut off by `max_output_tokens`) is not kept: a
  // replayed `reasoning` item has to be followed by the item it led to.
  Message message(kMessageRoleAssistant, std::move(turn));
  if (message.has_text() || message.has_tool_calls()) {
    result.response_messages.push_back(std::move(message));
  }

  ai::logger::log_debug(
      "Parsed Responses API response - text length: {}, function calls: {}",
      result.text.length(), result.tool_calls.size());
  return result;
}

}  // namespace openai
}  // namespace ai
