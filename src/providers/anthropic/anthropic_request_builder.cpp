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
      message["role"] = utils::message_role_to_string(msg.role);
      message["content"] = msg.content;
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

httplib::Headers AnthropicRequestBuilder::build_headers(
    const providers::ProviderConfig& config) {
  httplib::Headers headers = {
      {config.auth_header_name, config.auth_header_prefix + config.api_key},
      {"Content-Type", "application/json"}};

  // Add any extra headers
  for (const auto& [key, value] : config.extra_headers) {
    headers.emplace(key, value);
  }

  return headers;
}

}  // namespace anthropic
}  // namespace ai