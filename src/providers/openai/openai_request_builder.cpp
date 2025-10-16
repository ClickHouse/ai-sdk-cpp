#include "openai_request_builder.h"

#include "ai/logger.h"
#include "utils/message_utils.h"

namespace ai {
namespace openai {

nlohmann::json OpenAIRequestBuilder::build_request_json(
    const GenerateOptions& options) {
  nlohmann::json request{{"model", options.model},
                         {"messages", nlohmann::json::array()}};

  if (options.response_format)
    request["response_format"] = options.response_format.value();
  // Build messages array
  if (!options.messages.empty()) {
    // Use provided messages
    for (const auto& msg : options.messages) {
      nlohmann::json message;

      // Handle different content types
      if (msg.has_tool_results()) {
        // OpenAI expects each tool result as a separate message with role
        // "tool"
        for (const auto& result : msg.get_tool_results()) {
          nlohmann::json tool_message;
          tool_message["role"] = "tool";
          tool_message["tool_call_id"] = result.tool_call_id;

          if (!result.is_error) {
            tool_message["content"] = result.result.dump();
          } else {
            tool_message["content"] = "Error: " + result.result.dump();
          }

          request["messages"].push_back(tool_message);
        }
        continue;  // Skip adding the main message
      }

      // Handle messages with text and/or tool calls
      message["role"] = utils::message_role_to_string(msg.role);

      // Get text content (accumulate all text parts)
      std::string text_content = msg.get_text();

      // Get tool calls
      auto tool_calls = msg.get_tool_calls();

      // Set content - OpenAI expects both text and tool calls in the same
      // message
      if (!text_content.empty()) {
        message["content"] = text_content;
      }

      if (!tool_calls.empty()) {
        nlohmann::json tool_calls_array = nlohmann::json::array();
        for (const auto& tool_call : tool_calls) {
          tool_calls_array.push_back(
              {{"id", tool_call.id},
               {"type", "function"},
               {"function",
                {{"name", tool_call.tool_name},
                 {"arguments", tool_call.arguments.dump()}}}});
        }
        message["tool_calls"] = tool_calls_array;
      }

      // Skip empty messages
      if (text_content.empty() && tool_calls.empty()) {
        continue;
      }

      request["messages"].push_back(message);
    }
  } else {
    // Build from system + prompt
    if (!options.system.empty()) {
      request["messages"].push_back(
          {{"role", "system"}, {"content", options.system}});
    }

    if (!options.prompt.empty()) {
      request["messages"].push_back(
          {{"role", "user"}, {"content", options.prompt}});
    }
  }

  // Add optional parameters
  if (options.temperature) {
    request["temperature"] = *options.temperature;
  }

  if (options.max_tokens) {
    request["max_completion_tokens"] = *options.max_tokens;
  }

  if (options.top_p) {
    request["top_p"] = *options.top_p;
  }

  if (options.frequency_penalty) {
    request["frequency_penalty"] = *options.frequency_penalty;
  }

  if (options.presence_penalty) {
    request["presence_penalty"] = *options.presence_penalty;
  }

  if (options.seed) {
    request["seed"] = *options.seed;
  }

  // Add tools if specified
  if (options.has_tools()) {
    ai::logger::log_debug("Adding {} tools to request", options.tools.size());

    nlohmann::json tools_array = nlohmann::json::array();
    auto active_tool_names = options.get_active_tool_names();

    for (const auto& tool_name : active_tool_names) {
      auto it = options.tools.find(tool_name);
      if (it != options.tools.end()) {
        const auto& tool = it->second;

        nlohmann::json tool_def = {{"type", "function"},
                                   {"function",
                                    {{"name", tool_name},
                                     {"description", tool.description},
                                     {"parameters", tool.parameters_schema}}}};

        tools_array.push_back(tool_def);
      }
    }

    if (!tools_array.empty()) {
      request["tools"] = tools_array;

      // Add tool choice if specified
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
            request["tool_choice"] = {
                {"type", "function"},
                {"function", {{"name", *options.tool_choice.tool_name}}}};
          }
          break;
      }

      ai::logger::log_debug("Added {} tools with choice: {}",
                            tools_array.size(),
                            options.tool_choice.to_string());
    }
  }

  return request;
}

nlohmann::json OpenAIRequestBuilder::build_request_json(
    const EmbeddingOptions& options) {
  nlohmann::json request{{"model", options.model},
                         {"input", options.input}};

  // Set encoding format (default to float for compatibility)
  if (options.encoding_format) {
    request["encoding_format"] = options.encoding_format.value();
  } else {
    request["encoding_format"] = "float";
  }

  // Add dimensions if specified
  if (options.dimensions && options.dimensions.value() > 0) {
    request["dimensions"] = options.dimensions.value();
  }

  // Add user identifier if specified
  if (options.user) {
    request["user"] = options.user.value();
  }

  return request;
}

httplib::Headers OpenAIRequestBuilder::build_headers(
    const providers::ProviderConfig& config) {
  httplib::Headers headers = {
      {config.auth_header_name, config.auth_header_prefix + config.api_key}};

  // Add any extra headers
  for (const auto& [key, value] : config.extra_headers) {
    headers.emplace(key, value);
  }

  // Note: Content-Type is passed separately to httplib::Post() as content_type parameter
  return headers;
}

}  // namespace openai
}  // namespace ai