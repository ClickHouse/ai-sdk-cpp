#include "anthropic_client.h"

#include "ai/anthropic.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_result.h"
#include "anthropic_stream.h"

#include <algorithm>
#include <httplib.h>
#include <memory>

#include <spdlog/spdlog.h>

namespace ai {
namespace anthropic {

AnthropicClient::AnthropicClient(const std::string& api_key,
                                 const std::string& base_url)
    : api_key_(api_key), base_url_(base_url) {
  spdlog::debug("Initializing Anthropic client with base_url: {}", base_url);

  // Extract host from base_url
  if (base_url.starts_with("https://")) {
    host_ = base_url.substr(8);
    use_ssl_ = true;
  } else if (base_url.starts_with("http://")) {
    host_ = base_url.substr(7);
    use_ssl_ = false;
  } else {
    host_ = base_url;
    use_ssl_ = true;
  }

  // Remove trailing slash and path if any
  if (auto pos = host_.find('/'); pos != std::string::npos) {
    host_ = host_.substr(0, pos);
  }

  spdlog::debug("Anthropic client configured - host: {}, use_ssl: {}", host_,
                use_ssl_);
}

GenerateResult AnthropicClient::generate_text(const GenerateOptions& options) {
  spdlog::debug(
      "Starting text generation - model: {}, prompt length: {}, tools: {}, "
      "max_steps: {}",
      options.model, options.prompt.length(), options.tools.size(),
      options.max_steps);

  // Check if multi-step tool calling is enabled
  if (options.has_tools() && options.is_multi_step()) {
    spdlog::debug("Using multi-step tool calling with {} tools",
                  options.tools.size());

    // Use MultiStepCoordinator for complex workflows
    return MultiStepCoordinator::execute_multi_step(
        options, [this](const GenerateOptions& step_options) {
          return this->generate_text_single_step(step_options);
        });
  } else {
    // Single step generation
    return generate_text_single_step(options);
  }
}

GenerateResult AnthropicClient::generate_text_single_step(
    const GenerateOptions& options) {
  try {
    // Build request JSON
    auto request_json = build_request_json(options);
    std::string json_body = request_json.dump();
    spdlog::debug("Request JSON built: {}", json_body);

    // Make the request
    auto result = make_request(json_body);

    if (!result.is_success()) {
      return result;
    }

    // Parse the response JSON from result.text
    auto json_response = nlohmann::json::parse(result.text);
    spdlog::info("Text generation successful - model: {}, response_id: {}",
                 options.model, json_response.value("id", "unknown"));

    auto parsed_result = parse_messages_response(json_response);

    // Execute tools if the model made tool calls
    if (parsed_result.has_tool_calls() && options.has_tools()) {
      spdlog::debug("Model made {} tool calls, executing them",
                    parsed_result.tool_calls.size());

      try {
        auto tool_results = ToolExecutor::execute_tools(
            parsed_result.tool_calls, options.tools, options.messages);

        parsed_result.tool_results = tool_results;
        spdlog::debug("Successfully executed {} tools", tool_results.size());

      } catch (const ToolError& e) {
        spdlog::error("Tool execution failed: {}", e.what());
        parsed_result.error = "Tool execution failed: " + std::string(e.what());
        parsed_result.finish_reason = kFinishReasonError;
      }
    }

    return parsed_result;

  } catch (const std::exception& e) {
    spdlog::error("Exception during text generation: {}", e.what());
    return GenerateResult(std::string("Exception: ") + e.what());
  }
}

StreamResult AnthropicClient::stream_text(const StreamOptions& options) {
  spdlog::debug("Starting text streaming - model: {}, prompt length: {}",
                options.model, options.prompt.length());

  // Build request with stream: true
  auto request_json = build_request_json(options);
  request_json["stream"] = true;
  spdlog::debug("Stream request JSON built with stream=true");

  // Create headers
  httplib::Headers headers = {{"x-api-key", api_key_},
                              {"Content-Type", "application/json"},
                              {"Accept", "text/event-stream"},
                              {"anthropic-version", "2023-06-01"}};

  // Add beta header if needed for newer models (remove outdated beta header)
  // Note: Most Claude 3.5 models are now stable and don't need beta headers

  // Create stream implementation
  auto impl = std::make_unique<AnthropicStreamImpl>();
  impl->start_stream(base_url_ + "/v1/messages", headers, request_json);

  spdlog::info("Text streaming started - model: {}", options.model);

  // Return StreamResult with implementation
  return StreamResult(std::move(impl));
}

bool AnthropicClient::is_valid() const {
  return !api_key_.empty();
}

std::string AnthropicClient::provider_name() const {
  return "anthropic";
}

std::vector<std::string> AnthropicClient::supported_models() const {
  return {"claude-3-5-sonnet-20241022", "claude-3-5-haiku-20241022",
          "claude-3-opus-20240229", "claude-3-sonnet-20240229",
          "claude-3-haiku-20240307"};
}

bool AnthropicClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string AnthropicClient::config_info() const {
  return "Anthropic API (base_url: " + base_url_ + ")";
}

nlohmann::json AnthropicClient::build_request_json(
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
      message["role"] = message_role_to_string(msg.role);
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
    spdlog::debug("Adding {} tools to Anthropic request", options.tools.size());

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

      spdlog::debug("Added {} tools with choice: {}", tools_array.size(),
                    options.tool_choice.to_string());
    }
  }

  return request;
}

std::string AnthropicClient::message_role_to_string(MessageRole role) {
  switch (role) {
    case kMessageRoleSystem:
      return "system";
    case kMessageRoleUser:
      return "user";
    case kMessageRoleAssistant:
      return "assistant";
    default:
      return "user";
  }
}

GenerateResult AnthropicClient::parse_messages_response(
    const nlohmann::json& response) {
  spdlog::debug("Parsing Anthropic messages response");

  GenerateResult result;

  // Extract basic fields
  result.id = response.value("id", "");
  result.model = response.value("model", "");

  spdlog::debug("Response ID: {}, Model: {}", result.id.value_or("none"),
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
            JsonValue arguments = content_block["input"];

            ToolCall tool_call(call_id, function_name, arguments);
            result.tool_calls.push_back(tool_call);

            spdlog::debug("Parsed Anthropic tool call: {} with args: {}",
                          function_name, arguments.dump());
          }
        }
      }
    }

    result.text = full_text;
    spdlog::debug("Extracted message content - length: {}, tool calls: {}",
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
    spdlog::debug("Stop reason: {}", stop_reason);
  }

  // Extract usage
  if (response.contains("usage")) {
    auto& usage = response["usage"];
    result.usage.prompt_tokens = usage.value("input_tokens", 0);
    result.usage.completion_tokens = usage.value("output_tokens", 0);
    result.usage.total_tokens =
        result.usage.prompt_tokens + result.usage.completion_tokens;
    spdlog::debug("Token usage - input: {}, output: {}, total: {}",
                  result.usage.prompt_tokens, result.usage.completion_tokens,
                  result.usage.total_tokens);
  }

  // Store full metadata
  result.provider_metadata = response.dump();

  return result;
}

FinishReason AnthropicClient::parse_stop_reason(const std::string& reason) {
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

GenerateResult AnthropicClient::make_request(const std::string& json_body) {
  try {
    spdlog::debug("Making request with body size: {}", json_body.size());

    // Common headers for all requests
    httplib::Headers headers = {{"x-api-key", api_key_},
                                {"Content-Type", "application/json"},
                                {"anthropic-version", "2023-06-01"}};

    // Common request handler
    auto handle_response = [this](
                               const httplib::Result& res,
                               const std::string& protocol) -> GenerateResult {
      if (!res) {
        spdlog::error("{} request failed - no response", protocol);
        return GenerateResult(
            "Network error: Failed to connect to Anthropic API");
      }

      spdlog::debug("Got response: status={}, body_size={}", res->status,
                    res->body.size());

      if (res->status == 200) {
        GenerateResult result;
        result.text = res->body;
        return result;
      }

      return parse_error_response(res->status, res->body);
    };

    // Make request based on SSL setting
    if (use_ssl_) {
      spdlog::debug("Making HTTPS request to host: {}", host_);
      httplib::SSLClient cli(host_);
      cli.set_connection_timeout(30, 0);
      cli.set_read_timeout(120, 0);
      cli.enable_server_certificate_verification(false);

      auto res =
          cli.Post("/v1/messages", headers, json_body, "application/json");
      return handle_response(res, "HTTPS");
    } else {
      spdlog::debug("Making HTTP request to host: {}", host_);
      httplib::Client cli(host_);
      cli.set_connection_timeout(30, 0);
      cli.set_read_timeout(120, 0);

      auto res =
          cli.Post("/v1/messages", headers, json_body, "application/json");
      return handle_response(res, "HTTP");
    }

  } catch (const std::exception& e) {
    spdlog::error("Exception in make_request: {}", e.what());
    return GenerateResult(std::string("Request failed: ") + e.what());
  } catch (...) {
    spdlog::error("Unknown exception in make_request");
    return GenerateResult("Request failed: Unknown error");
  }
}

GenerateResult AnthropicClient::parse_error_response(int status_code,
                                                     const std::string& body) {
  spdlog::debug("Parsing error response - status: {}, body: {}", status_code,
                body);

  try {
    auto json = nlohmann::json::parse(body);
    if (json.contains("error")) {
      auto& error = json["error"];
      std::string message = error.value("message", "Unknown error");
      std::string type = error.value("type", "");

      std::string full_error = "Anthropic API error (" +
                               std::to_string(status_code) + "): " + message +
                               (type.empty() ? "" : " [" + type + "]");
      spdlog::error("Anthropic API error parsed: {}", full_error);

      return GenerateResult(full_error);
    }
  } catch (...) {
    // If JSON parsing fails, return raw error
    spdlog::debug("Failed to parse error response as JSON");
  }

  std::string raw_error =
      "HTTP " + std::to_string(status_code) + " error: " + body;
  spdlog::error("Anthropic API raw error: {}", raw_error);

  return GenerateResult(raw_error);
}

}  // namespace anthropic
}  // namespace ai