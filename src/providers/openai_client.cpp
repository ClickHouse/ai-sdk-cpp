#include "ai/openai.h"
#include "ai/types/client.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"
#include "ai/types/stream_result.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai {
namespace openai {

namespace {

/// Internal implementation for streaming results
class OpenAIStreamImpl : public internal::StreamResultImpl {
 public:
  OpenAIStreamImpl() = default;
  ~OpenAIStreamImpl() { stop_stream(); }

  void start_stream(const std::string& url,
                    const httplib::Headers& headers,
                    const nlohmann::json& request_body) {
    stream_thread_ = std::thread([this, url, headers, request_body]() {
      run_stream(url, headers, request_body);
    });
  }

  StreamEvent get_next_event() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock,
                   [this] { return !event_queue_.empty() || is_complete_; });

    if (event_queue_.empty()) {
      return StreamEvent(kStreamEventTypeFinish);
    }

    auto event = std::move(event_queue_.front());
    event_queue_.pop();
    return event;
  }

  bool has_more_events() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !event_queue_.empty() || !is_complete_;
  }

  void stop_stream() {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      should_stop_ = true;
    }
    queue_cv_.notify_all();

    if (stream_thread_.joinable()) {
      stream_thread_.join();
    }
  }

 private:
  void run_stream(const std::string& url,
                  const httplib::Headers& headers,
                  const nlohmann::json& request_body) {
    try {
      httplib::SSLClient client("api.openai.com");
      client.set_connection_timeout(30);
      client.set_read_timeout(300);  // 5 minutes for long generations

      std::string accumulated_data;

      // Create request
      httplib::Request req;
      req.method = "POST";
      req.path = "/v1/chat/completions";
      req.headers = headers;
      req.body = request_body.dump();
      req.set_header("Content-Type", "application/json");

      // Set content receiver for streaming response
      req.content_receiver = [this, &accumulated_data](
                                 const char* data, size_t data_length,
                                 uint64_t /*offset*/,
                                 uint64_t /*total_length*/) {
        // Accumulate data and process complete lines
        accumulated_data.append(data, data_length);

        // Process complete lines
        size_t pos = 0;
        while ((pos = accumulated_data.find('\n')) != std::string::npos) {
          std::string line = accumulated_data.substr(0, pos);
          accumulated_data.erase(0, pos + 1);

          if (!line.empty() && line.back() == '\r') {
            line.pop_back();
          }

          {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (should_stop_) {
              return false;
            }
          }

          parse_sse_line(line);
        }

        return true;  // Continue receiving
      };

      httplib::Response res;
      httplib::Error error;

      if (!client.send(req, res, error)) {
        push_event(StreamEvent(kStreamEventTypeError,
                               "Network error: " + httplib::to_string(error)));
      } else if (res.status != 200) {
        push_event(StreamEvent(
            kStreamEventTypeError,
            "HTTP " + std::to_string(res.status) + " error: " + res.body));
      }
    } catch (const std::exception& e) {
      push_event(StreamEvent(kStreamEventTypeError, e.what()));
    }

    mark_complete();
  }

  void parse_sse_line(const std::string& line) {
    if (line.starts_with("data: ")) {
      auto data = line.substr(6);
      if (data == "[DONE]") {
        mark_complete();
        return;
      }

      try {
        auto json = nlohmann::json::parse(data);
        auto& choices = json["choices"];

        if (!choices.empty() && choices[0].contains("delta")) {
          auto& delta = choices[0]["delta"];
          if (delta.contains("content")) {
            push_event(StreamEvent(delta["content"].get<std::string>()));
          }
        }

        // Check for finish_reason
        if (!choices.empty() && choices[0].contains("finish_reason") &&
            !choices[0]["finish_reason"].is_null()) {
          auto finish_reason_str =
              choices[0]["finish_reason"].get<std::string>();
          FinishReason finish_reason = kFinishReasonStop;

          if (finish_reason_str == "stop") {
            finish_reason = kFinishReasonStop;
          } else if (finish_reason_str == "length") {
            finish_reason = kFinishReasonLength;
          } else if (finish_reason_str == "content_filter") {
            finish_reason = kFinishReasonContentFilter;
          }

          // Parse usage if available
          Usage usage;
          if (json.contains("usage")) {
            auto& usage_json = json["usage"];
            usage.prompt_tokens = usage_json.value("prompt_tokens", 0);
            usage.completion_tokens = usage_json.value("completion_tokens", 0);
            usage.total_tokens = usage_json.value("total_tokens", 0);
          }

          push_event(StreamEvent(kStreamEventTypeFinish, usage, finish_reason));
        }
      } catch (const std::exception& e) {
        spdlog::error("Failed to parse SSE line: {}", e.what());
      }
    }
  }

  void push_event(StreamEvent event) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      event_queue_.push(std::move(event));
    }
    queue_cv_.notify_one();
  }

  void mark_complete() {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      is_complete_ = true;
    }
    queue_cv_.notify_all();
  }

  mutable std::mutex queue_mutex_;
  mutable std::condition_variable queue_cv_;
  std::queue<StreamEvent> event_queue_;
  std::thread stream_thread_;
  bool is_complete_ = false;
  bool should_stop_ = false;
};

/// OpenAI client implementation
class OpenAIClient : public Client {
 public:
  OpenAIClient(const std::string& api_key, const std::string& base_url)
      : api_key_(api_key), base_url_(base_url) {
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
  }

  GenerateResult generate_text(const GenerateOptions& options) override {
    try {
      // Build request JSON
      auto request_json = build_request_json(options);

      // Make HTTP request
      httplib::Headers headers = {{"Authorization", "Bearer " + api_key_},
                                  {"Content-Type", "application/json"}};

      std::unique_ptr<httplib::Response> response;

      if (use_ssl_) {
        httplib::SSLClient client(host_);
        client.set_connection_timeout(30);
        client.set_read_timeout(120);

        auto res = client.Post("/v1/chat/completions", headers,
                               request_json.dump(), "application/json");
        if (res) {
          response = std::make_unique<httplib::Response>(*res);
        }
      } else {
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
        return GenerateResult("Network error: Failed to connect to OpenAI API");
      }

      if (response->status != 200) {
        return parse_error_response(response->status, response->body);
      }

      // Parse successful response
      auto json_response = nlohmann::json::parse(response->body);
      return parse_chat_completion_response(json_response);

    } catch (const std::exception& e) {
      return GenerateResult(std::string("Exception: ") + e.what());
    }
  }

  StreamResult stream_text(const StreamOptions& options) override {
    // Build request with stream: true
    auto request_json = build_request_json(options);
    request_json["stream"] = true;

    // Create headers
    httplib::Headers headers = {{"Authorization", "Bearer " + api_key_},
                                {"Content-Type", "application/json"},
                                {"Accept", "text/event-stream"}};

    // Create stream implementation
    auto impl = std::make_unique<OpenAIStreamImpl>();
    impl->start_stream(base_url_ + "/v1/chat/completions", headers,
                       request_json);

    // Return StreamResult with implementation
    return StreamResult(std::move(impl));
  }

  bool is_valid() const override { return !api_key_.empty(); }

  std::string provider_name() const override { return "openai"; }

  std::vector<std::string> supported_models() const override {
    return {models::kGpt4o,       models::kGpt4oMini,   models::kGpt4Turbo,
            models::kGpt35Turbo,  models::kGpt4,        "gpt-4-0125-preview",
            "gpt-4-1106-preview", "gpt-3.5-turbo-0125", "gpt-3.5-turbo-1106"};
  }

  bool supports_model(const std::string& model_name) const override {
    auto models = supported_models();
    return std::find(models.begin(), models.end(), model_name) != models.end();
  }

  std::string config_info() const override {
    return "OpenAI API (base_url: " + base_url_ + ")";
  }

 private:
  nlohmann::json build_request_json(const GenerateOptions& options) {
    nlohmann::json request{{"model", options.model},
                           {"messages", nlohmann::json::array()}};

    // Build messages array
    if (!options.messages.empty()) {
      // Use provided messages
      for (const auto& msg : options.messages) {
        request["messages"].push_back(
            {{"role", message_role_to_string(msg.role)},
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

  GenerateResult parse_chat_completion_response(
      const nlohmann::json& response) {
    GenerateResult result;

    // Extract basic fields
    result.id = response.value("id", "");
    result.model = response.value("model", "");
    result.created = response.value("created", 0);
    result.system_fingerprint = response.value("system_fingerprint", "");

    // Extract choices
    if (response.contains("choices") && !response["choices"].empty()) {
      auto& choice = response["choices"][0];

      // Extract message content
      if (choice.contains("message")) {
        auto& message = choice["message"];
        result.text = message.value("content", "");

        // Add assistant response to messages
        result.response_messages.push_back(
            {kMessageRoleAssistant, result.text});
      }

      // Extract finish reason
      if (choice.contains("finish_reason")) {
        auto finish_reason_str = choice["finish_reason"].get<std::string>();
        result.finish_reason = parse_finish_reason(finish_reason_str);
      }
    }

    // Extract usage
    if (response.contains("usage")) {
      auto& usage = response["usage"];
      result.usage.prompt_tokens = usage.value("prompt_tokens", 0);
      result.usage.completion_tokens = usage.value("completion_tokens", 0);
      result.usage.total_tokens = usage.value("total_tokens", 0);
    }

    // Store full metadata
    result.provider_metadata = response.dump();

    return result;
  }

  GenerateResult parse_error_response(int status_code,
                                      const std::string& body) {
    try {
      auto json = nlohmann::json::parse(body);
      if (json.contains("error")) {
        auto& error = json["error"];
        std::string message = error.value("message", "Unknown error");
        std::string type = error.value("type", "");
        return GenerateResult("OpenAI API error (" +
                              std::to_string(status_code) + "): " + message +
                              (type.empty() ? "" : " [" + type + "]"));
      }
    } catch (...) {
      // If JSON parsing fails, return raw error
    }

    return GenerateResult("HTTP " + std::to_string(status_code) +
                          " error: " + body);
  }

  std::string message_role_to_string(MessageRole role) {
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

  FinishReason parse_finish_reason(const std::string& reason) {
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

  std::string api_key_;
  std::string base_url_;
  std::string host_;
  bool use_ssl_ = true;
};

}  // namespace

// Factory function implementations
Client create_client() {
  const char* api_key = std::getenv("OPENAI_API_KEY");
  if (!api_key) {
    throw std::runtime_error("OPENAI_API_KEY environment variable not set");
  }
  return Client(
      std::make_unique<OpenAIClient>(api_key, "https://api.openai.com"));
}

Client create_client(const std::string& api_key) {
  return Client(
      std::make_unique<OpenAIClient>(api_key, "https://api.openai.com"));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<OpenAIClient>(api_key, base_url));
}

}  // namespace openai
}  // namespace ai