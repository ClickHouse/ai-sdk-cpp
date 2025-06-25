#include "openai_client.h"

#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_result.h"
#include "openai_stream.h"

#include <algorithm>
#include <httplib.h>
#include <memory>

#include <spdlog/spdlog.h>
namespace ai {
namespace openai {

OpenAIClient::OpenAIClient(const std::string& api_key,
                           const std::string& base_url)
    : api_key_(api_key), base_url_(base_url) {
  spdlog::debug("Initializing OpenAI client with base_url: {}", base_url);

  // Extract host from base_url
  if (base_url.starts_with("https://")) {
    host_ = base_url.substr(8);  // Remove "https://"
  } else if (base_url.starts_with("http://")) {
    host_ = base_url.substr(7);  // Remove "http://"
    use_ssl_ = false;
  } else {
    host_ = base_url;
  }

  // Remove trailing slash and path if any
  if (auto pos = host_.find('/'); pos != std::string::npos) {
    host_ = host_.substr(0, pos);
  }

  spdlog::debug("OpenAI client configured - host: {}, use_ssl: {}", host_,
                use_ssl_);
}

GenerateResult OpenAIClient::generate_text(const GenerateOptions& options) {
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

GenerateResult OpenAIClient::generate_text_single_step(
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

    // Parse the response JSON from result.text (which contains the raw
    // response)
    auto json_response = nlohmann::json::parse(result.text);
    spdlog::info("Text generation successful - model: {}, response_id: {}",
                 options.model, json_response.value("id", "unknown"));

    // Debug: print the response for tool calls
    if (options.has_tools()) {
      spdlog::debug("OpenAI response with tools: {}", json_response.dump(2));
    }

    auto parsed_result = parse_chat_completion_response(json_response);

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

StreamResult OpenAIClient::stream_text(const StreamOptions& options) {
  spdlog::debug("Starting text streaming - model: {}, prompt length: {}",
                options.model, options.prompt.length());

  // Build request with stream: true
  auto request_json = build_request_json(options);
  request_json["stream"] = true;
  spdlog::debug("Stream request JSON built with stream=true");

  // Create headers
  httplib::Headers headers = {{"Authorization", "Bearer " + api_key_},
                              {"Content-Type", "application/json"},
                              {"Accept", "text/event-stream"}};

  // Create stream implementation
  auto impl = std::make_unique<OpenAIStreamImpl>();
  impl->start_stream(base_url_ + "/v1/chat/completions", headers, request_json);

  spdlog::info("Text streaming started - model: {}", options.model);

  // Return StreamResult with implementation
  return StreamResult(std::move(impl));
}

bool OpenAIClient::is_valid() const {
  return !api_key_.empty();
}

std::string OpenAIClient::provider_name() const {
  return "openai";
}

std::vector<std::string> OpenAIClient::supported_models() const {
  return {models::kGpt4o,       models::kGpt4oMini,   models::kGpt4Turbo,
          models::kGpt35Turbo,  models::kGpt4,        "gpt-4-0125-preview",
          "gpt-4-1106-preview", "gpt-3.5-turbo-0125", "gpt-3.5-turbo-1106"};
}

bool OpenAIClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string OpenAIClient::config_info() const {
  return "OpenAI API (base_url: " + base_url_ + ")";
}

nlohmann::json OpenAIClient::build_request_json(
    const GenerateOptions& options) {
  nlohmann::json request{{"model", options.model},
                         {"messages", nlohmann::json::array()}};

  // Build messages array
  if (!options.messages.empty()) {
    // Use provided messages
    for (const auto& msg : options.messages) {
      request["messages"].push_back({{"role", message_role_to_string(msg.role)},
                                     {"content", msg.content}});
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
    spdlog::debug("Adding {} tools to request", options.tools.size());

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

      spdlog::debug("Added {} tools with choice: {}", tools_array.size(),
                    options.tool_choice.to_string());
    }
  }

  return request;
}

GenerateResult OpenAIClient::parse_chat_completion_response(
    const nlohmann::json& response) {
  spdlog::debug("Parsing chat completion response");

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
    if (choice.contains("finish_reason")) {
      auto finish_reason_str = choice["finish_reason"].get<std::string>();
      result.finish_reason = parse_finish_reason(finish_reason_str);
      spdlog::debug("Finish reason: {}", finish_reason_str);
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

GenerateResult OpenAIClient::parse_error_response(int status_code,
                                                  const std::string& body) {
  spdlog::debug("Parsing error response - status: {}, body: {}", status_code,
                body);

  try {
    auto json = nlohmann::json::parse(body);
    if (json.contains("error")) {
      auto& error = json["error"];
      std::string message = error.value("message", "Unknown error");
      std::string type = error.value("type", "");

      std::string full_error = "OpenAI API error (" +
                               std::to_string(status_code) + "): " + message +
                               (type.empty() ? "" : " [" + type + "]");
      spdlog::error("OpenAI API error parsed: {}", full_error);

      return GenerateResult(full_error);
    }
  } catch (...) {
    // If JSON parsing fails, return raw error
    spdlog::debug("Failed to parse error response as JSON");
  }

  std::string raw_error =
      "HTTP " + std::to_string(status_code) + " error: " + body;
  spdlog::error("OpenAI API raw error: {}", raw_error);

  return GenerateResult(raw_error);
}

std::string OpenAIClient::message_role_to_string(MessageRole role) {
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

FinishReason OpenAIClient::parse_finish_reason(const std::string& reason) {
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

GenerateResult OpenAIClient::make_request(const std::string& json_body) {
  try {
    spdlog::debug("Making request with body size: {}", json_body.size());

    // Common headers for all requests
    httplib::Headers headers = {{"Authorization", "Bearer " + api_key_},
                                {"Content-Type", "application/json"}};

    // Common request handler
    auto handle_response = [this](
                               const httplib::Result& res,
                               const std::string& protocol) -> GenerateResult {
      if (!res) {
        spdlog::error("{} request failed - no response", protocol);
        return GenerateResult("Network error: Failed to connect to OpenAI API");
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

      auto res = cli.Post("/v1/chat/completions", headers, json_body,
                          "application/json");
      return handle_response(res, "HTTPS");
    } else {
      spdlog::debug("Making HTTP request to host: {}", host_);
      httplib::Client cli(host_);
      cli.set_connection_timeout(30, 0);
      cli.set_read_timeout(120, 0);

      auto res = cli.Post("/v1/chat/completions", headers, json_body,
                          "application/json");
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

}  // namespace openai
}  // namespace ai