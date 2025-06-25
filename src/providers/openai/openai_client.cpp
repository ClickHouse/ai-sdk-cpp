#include "openai_client.h"

#include "ai/openai.h"
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
  spdlog::debug("Starting text generation - model: {}, prompt length: {}",
                options.model, options.prompt.length());

  try {
    // Build request JSON
    auto request_json = build_request_json(options);
    spdlog::debug("Request JSON built: {}", request_json.dump());

    // Make HTTP request
    httplib::Headers headers = {{"Authorization", "Bearer " + api_key_},
                                {"Content-Type", "application/json"}};

    std::unique_ptr<httplib::Response> response;

    if (use_ssl_) {
      spdlog::debug("Creating SSL client for host: {}", host_);
      httplib::SSLClient client(host_);
      client.set_connection_timeout(30);
      client.set_read_timeout(120);

      auto res = client.Post("/v1/chat/completions", headers,
                             request_json.dump(), "application/json");
      if (res) {
        response = std::make_unique<httplib::Response>(*res);
      }
    } else {
      spdlog::debug("Creating HTTP client for host: {}", host_);
      httplib::Client client(host_);
      client.set_connection_timeout(30);
      client.set_read_timeout(120);

      auto res = client.Post("/v1/chat/completions", headers,
                             request_json.dump(), "application/json");
      if (res) {
        response = std::make_unique<httplib::Response>(*res);
      }
    }

    if (!response) {
      spdlog::error("Network error: Failed to connect to OpenAI API at {}",
                    host_);
      return GenerateResult("Network error: Failed to connect to OpenAI API");
    }

    spdlog::debug("Received response - status: {}, body length: {}",
                  response->status, response->body.length());

    if (response->status != 200) {
      spdlog::error("OpenAI API returned error status: {} - {}",
                    response->status, response->body);
      return parse_error_response(response->status, response->body);
    }

    // Parse successful response
    auto json_response = nlohmann::json::parse(response->body);
    spdlog::info("Text generation successful - model: {}, response_id: {}",
                 options.model, json_response.value("id", "unknown"));
    return parse_chat_completion_response(json_response);

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
      result.text = message.value("content", "");
      spdlog::debug("Extracted message content - length: {}",
                    result.text.length());

      // Add assistant response to messages
      result.response_messages.push_back({kMessageRoleAssistant, result.text});
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

}  // namespace openai
}  // namespace ai