#include "anthropic_request_builder.h"

#include "ai/logger.h"
#include "utils/message_utils.h"

namespace ai {
namespace anthropic {

nlohmann::json AnthropicRequestBuilder::build_request_json(
    const GenerateOptions& options) {
  nlohmann::json request;
  request["model"] = options.model;
  request["max_tokens"] = options.max_tokens.value_or(4096);
  request["messages"] = nlohmann::json::array();

  // Handle system message
  if (!options.system.empty()) {
    request["system"] = options.system;
  }

  // Build messages array
  if (!options.messages.empty()) {
    // Use provided messages
    for (const auto& msg : options.messages) {
      nlohmann::json message;

      // Handle different content types
      if (msg.has_tool_results()) {
        // Anthropic expects tool results as content arrays in user messages
        message["role"] = "user";
        message["content"] = nlohmann::json::array();

        for (const auto& result : msg.get_tool_results()) {
          nlohmann::json tool_result_content;
          tool_result_content["type"] = "tool_result";
          tool_result_content["tool_use_id"] = result.tool_call_id;

          if (!result.is_error) {
            tool_result_content["content"] = result.result.dump();
          } else {
            tool_result_content["content"] = result.result.dump();
            tool_result_content["is_error"] = true;
          }

          message["content"].push_back(tool_result_content);
        }
      } else {
        // Handle messages with text and/or tool calls
        message["role"] = utils::message_role_to_string(msg.role);

        // Get text content and tool calls
        std::string text_content = msg.get_text();
        auto tool_calls = msg.get_tool_calls();

        // Anthropic expects content as array for mixed content or tool calls
        if (!tool_calls.empty() ||
            (msg.role == kMessageRoleAssistant && !text_content.empty())) {
          message["content"] = nlohmann::json::array();

          // Add text content if present
          if (!text_content.empty()) {
            message["content"].push_back(
                {{"type", "text"}, {"text", text_content}});
          }

          // Add tool use content
          for (const auto& tool_call : tool_calls) {
            message["content"].push_back({{"type", "tool_use"},
                                          {"id", tool_call.id},
                                          {"name", tool_call.tool_name},
                                          {"input", tool_call.arguments}});
          }
        } else if (!text_content.empty()) {
          // Simple text message (non-assistant or assistant with text only)
          message["content"] = text_content;
        } else {
          // Empty message, skip
          continue;
        }
      }

      request["messages"].push_back(message);
    }
  } else {
    // Build from prompt
    if (!options.prompt.empty()) {
      nlohmann::json message;
      message["role"] = "user";
      message["content"] = options.prompt;
      request["messages"].push_back(message);
    }
  }

  // Add optional parameters
  if (options.temperature) {
    request["temperature"] = *options.temperature;
  }

  if (options.top_p) {
    request["top_p"] = *options.top_p;
  }

  // Anthropic uses top_k instead of top_p for some control
  if (options.seed) {
    // Anthropic doesn't support seed directly, but we can add it as metadata
  }

  // Add tools if specified
  if (options.has_tools()) {
    ai::logger::log_debug("Adding {} tools to Anthropic request",
                          options.tools.size());

    nlohmann::json tools_array = nlohmann::json::array();
    auto active_tool_names = options.get_active_tool_names();

    for (const auto& tool_name : active_tool_names) {
      auto it = options.tools.find(tool_name);
      if (it != options.tools.end()) {
        const auto& tool = it->second;

        nlohmann::json tool_def = {{"name", tool_name},
                                   {"description", tool.description},
                                   {"input_schema", tool.parameters_schema}};

        tools_array.push_back(tool_def);
      }
    }

    if (!tools_array.empty()) {
      request["tools"] = tools_array;

      // Add tool choice if specified
      switch (options.tool_choice.type) {
        case ToolChoiceType::kAuto:
          request["tool_choice"] = {{"type", "auto"}};
          break;
        case ToolChoiceType::kRequired:
          request["tool_choice"] = {{"type", "any"}};
          break;
        case ToolChoiceType::kSpecific:
          if (options.tool_choice.tool_name) {
            request["tool_choice"] = {{"type", "tool"},
                                      {"name", *options.tool_choice.tool_name}};
          }
          break;
        case ToolChoiceType::kNone:
          // Anthropic doesn't have explicit "none" - just don't send tools
          break;
      }

      ai::logger::log_debug("Added {} tools with choice: {}",
                            tools_array.size(),
                            options.tool_choice.to_string());
    }
  }

  return request;
}

nlohmann::json AnthropicRequestBuilder::build_request_json(const EmbeddingOptions& options) {
  // Note: Anthropic does not currently offer embeddings API
  // This is a placeholder for future compatibility or custom endpoints
  nlohmann::json request{{"model", options.model},
                          {"input", options.input}};
  return request;
}

httplib::Headers AnthropicRequestBuilder::build_headers(
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

}  // namespace anthropic
}  // namespace ai
