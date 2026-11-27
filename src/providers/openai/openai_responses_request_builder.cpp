#include "openai_responses_request_builder.h"

#include "ai/logger.h"
#include "openai_model_traits.h"
#include "utils/message_utils.h"

#include <map>
#include <optional>
#include <string>
#include <variant>

namespace ai {
namespace openai {

namespace {

nlohmann::json make_text_message(MessageRole role, std::string text) {
  return {{"type", "message"},
          {"role", utils::message_role_to_string(role)},
          {"content", std::move(text)}};
}

/// The `id` of the `function_call` output item each tool call came from, by
/// `call_id`. The parser records them as `ProviderContentPart`s; replaying the
/// item IDs keeps the calls paired with the `reasoning` items before them.
std::map<std::string, std::string> function_call_item_ids(
    const Message& message) {
  std::map<std::string, std::string> ids;
  for (const auto& part : message.content) {
    const auto* provider_part = std::get_if<ProviderContentPart>(&part);
    if (provider_part == nullptr ||
        provider_part->provider != kResponsesProviderName) {
      continue;
    }
    const auto& data = provider_part->data;
    if (data.value("type", "") == "function_call" && data.contains("id") &&
        data.contains("call_id")) {
      ids[data["call_id"].get<std::string>()] = data["id"].get<std::string>();
    }
  }
  return ids;
}

/// Append the items of an assistant turn in their original order: text as
/// assistant messages, tool calls as `function_call` items, and the replayed
/// provider items (`reasoning`) verbatim.
void append_assistant_items(const Message& message, nlohmann::json& input) {
  const auto item_ids = function_call_item_ids(message);
  std::string pending_text;
  const auto flush_text = [&]() {
    if (!pending_text.empty()) {
      input.push_back(
          make_text_message(kMessageRoleAssistant, std::move(pending_text)));
      pending_text.clear();
    }
  };

  for (const auto& part : message.content) {
    if (const auto* text_part = std::get_if<TextContentPart>(&part)) {
      pending_text += text_part->text;
    } else if (const auto* tool_call =
                   std::get_if<ToolCallContentPart>(&part)) {
      flush_text();
      nlohmann::json item{{"type", "function_call"},
                          {"call_id", tool_call->id},
                          {"name", tool_call->tool_name},
                          {"arguments", tool_call->arguments.dump()}};
      if (const auto it = item_ids.find(tool_call->id); it != item_ids.end()) {
        item["id"] = it->second;
      }
      input.push_back(std::move(item));
    } else if (const auto* provider_part =
                   std::get_if<ProviderContentPart>(&part)) {
      // Function call items are replayed from their ToolCallContentPart above.
      if (provider_part->provider != kResponsesProviderName ||
          provider_part->data.value("type", "") == "function_call") {
        continue;
      }
      flush_text();
      input.push_back(provider_part->data);
    }
  }
  flush_text();
}

nlohmann::json build_input(const GenerateOptions& options) {
  nlohmann::json input = nlohmann::json::array();

  if (options.messages.empty()) {
    if (!options.prompt.empty()) {
      input.push_back(make_text_message(kMessageRoleUser, options.prompt));
    }
    return input;
  }

  for (const auto& message : options.messages) {
    if (message.has_tool_results()) {
      for (const auto& result : message.get_tool_results()) {
        input.push_back(
            {{"type", "function_call_output"},
             {"call_id", result.tool_call_id},
             {"output", result.is_error ? "Error: " + result.result.dump()
                                        : result.result.dump()}});
      }
      continue;
    }

    if (message.role == kMessageRoleAssistant) {
      append_assistant_items(message, input);
      continue;
    }

    auto text = message.get_text();
    if (!text.empty()) {
      input.push_back(make_text_message(message.role, std::move(text)));
    }
  }
  return input;
}

/// Chat Completions nests a JSON schema under `json_schema`; `text.format`
/// carries its fields at the top level. Formats already in the Responses shape
/// (`text`, `json_object`, or a flat `json_schema`) pass through unchanged.
nlohmann::json to_text_format(const nlohmann::json& response_format) {
  if (response_format.is_object() &&
      response_format.value("type", "") == "json_schema" &&
      response_format.contains("json_schema") &&
      response_format["json_schema"].is_object()) {
    auto format = response_format["json_schema"];
    format["type"] = "json_schema";
    return format;
  }
  return response_format;
}

}  // namespace

nlohmann::json OpenAIResponsesRequestBuilder::build_request_json(
    const GenerateOptions& options) {
  nlohmann::json request{{"model", options.model},
                         {"input", build_input(options)},
                         // Stateless: every request replays the whole
                         // conversation, so nothing has to be stored.
                         {"store", false}};

  if (!options.system.empty()) {
    request["instructions"] = options.system;
  }

  // With `store: false` the API returns the `reasoning` items with their
  // `encrypted_content` by default, which is what lets them be replayed; the
  // legacy `include: ["reasoning.encrypted_content"]` is not needed.
  const bool reasoning_model = is_reasoning_family(options.model);

  if (options.response_format) {
    request["text"] = {{"format", to_text_format(*options.response_format)}};
  }

  // The GPT-5 and GPT-6 families reject sampling controls; see the Chat
  // Completions builder.
  const auto add_sampling_parameter = [&](const char* key,
                                          const std::optional<double>& value) {
    if (!value) {
      return;
    }
    if (reasoning_model) {
      ai::logger::log_warn(
          "Ignoring {} for {} because the model does not support "
          "sampling parameters",
          key, options.model);
    } else {
      request[key] = *value;
    }
  };
  add_sampling_parameter("temperature", options.temperature);
  add_sampling_parameter("top_p", options.top_p);

  // The Responses API has no `frequency_penalty`, `presence_penalty`, or
  // `seed`; sending them fails the request with "Unknown parameter".
  const auto warn_unsupported = [&](const char* key, bool is_set) {
    if (is_set) {
      ai::logger::log_warn(
          "Ignoring {} because the OpenAI Responses API does not support it",
          key);
    }
  };
  warn_unsupported("frequency_penalty", options.frequency_penalty.has_value());
  warn_unsupported("presence_penalty", options.presence_penalty.has_value());
  warn_unsupported("seed", options.seed.has_value());

  if (options.max_tokens) {
    request["max_output_tokens"] = *options.max_tokens;
  }

  // Use the lowest reasoning effort the model accepts, like the Chat
  // Completions builder, so that the provider-neutral options keep their
  // latency and token budget (`max_tokens` covers reasoning tokens too).
  if (const auto effort = lowest_reasoning_effort(options.model)) {
    request["reasoning"] = {{"effort", std::string(*effort)}};
  }

  if (options.has_tools()) {
    nlohmann::json tools = nlohmann::json::array();
    for (const auto& tool_name : options.get_active_tool_names()) {
      const auto it = options.tools.find(tool_name);
      if (it == options.tools.end()) {
        continue;
      }
      // Without `strict` the API attempts strict mode, which requires every
      // property to be listed in `required` and `additionalProperties: false`.
      // Keep the non-strict semantics of Chat Completions instead.
      tools.push_back({{"type", "function"},
                       {"name", tool_name},
                       {"description", it->second.description},
                       {"parameters", it->second.parameters_schema},
                       {"strict", false}});
    }

    if (!tools.empty()) {
      ai::logger::log_debug("Added {} tools with choice: {}", tools.size(),
                            options.tool_choice.to_string());
      request["tools"] = std::move(tools);

      switch (options.tool_choice.type) {
        case ToolChoiceType::kAuto:
          request["tool_choice"] = "auto";
          break;
        case ToolChoiceType::kRequired:
          request["tool_choice"] = "required";
          break;
        case ToolChoiceType::kNone:
          request["tool_choice"] = "none";
          break;
        case ToolChoiceType::kSpecific:
          if (options.tool_choice.tool_name) {
            request["tool_choice"] = {{"type", "function"},
                                      {"name", *options.tool_choice.tool_name}};
          }
          break;
      }
    }
  }

  return request;
}

}  // namespace openai
}  // namespace ai
