#include "openai_stream.h"

#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>

namespace {
constexpr auto kEventTimeout = static_cast<std::chrono::seconds>(30);
constexpr auto kSleepInterval = std::chrono::milliseconds(1);
constexpr auto kConnectionTimeout = 30;  // seconds
constexpr auto kReadTimeout = 300;       // 5 minutes for long generations
const std::string kOpenAIHost = "api.openai.com";
const std::string kChatCompletionsPath = "/v1/chat/completions";
}  // namespace

namespace ai {
namespace openai {

OpenAIStreamImpl::~OpenAIStreamImpl() {
  stop_stream();
}

void OpenAIStreamImpl::start_stream(const std::string& url,
                                    const httplib::Headers& headers,
                                    const nlohmann::json& request_body) {
  std::lock_guard<std::mutex> lock(thread_mutex_);

  if (stream_thread_.joinable()) {
    return;  // Already running
  }

  // Reset state for new stream
  should_stop_ = false;
  is_complete_ = false;
  finish_event_pushed_ = false;

  stream_thread_ = std::thread([this, url, headers, request_body]() {
    run_stream(url, headers, request_body);
  });
}

StreamEvent OpenAIStreamImpl::get_next_event() {
  StreamEvent event("");
  auto start_time = std::chrono::steady_clock::now();

  while (!event_queue_.try_dequeue(event)) {
    if (is_complete_ && event_queue_.size_approx() == 0) {
      // Stream is complete and queue is empty
      return StreamEvent("");
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() - start_time > kEventTimeout) {
      spdlog::error("Timeout waiting for next stream event after {} seconds",
                    kEventTimeout.count());
      return StreamEvent(kStreamEventTypeError,
                         "Timeout waiting for next event");
    }

    std::this_thread::sleep_for(kSleepInterval);
  }

  return event;
}

bool OpenAIStreamImpl::has_more_events() const {
  // No locks needed - these are atomic operations
  return event_queue_.size_approx() > 0 || !is_complete_;
}

void OpenAIStreamImpl::stop_stream() {
  should_stop_ = true;  // Atomic write

  std::lock_guard<std::mutex> lock(thread_mutex_);
  if (stream_thread_.joinable()) {
    stream_thread_.join();
  }
}

void OpenAIStreamImpl::run_stream(const std::string& /*url*/,
                                  const httplib::Headers& headers,
                                  const nlohmann::json& request_body) {
  try {
    httplib::SSLClient client(kOpenAIHost);
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(kConnectionTimeout);
    client.set_read_timeout(kReadTimeout);

    std::string accumulated_data;

    // Create request
    httplib::Request req;
    req.method = "POST";
    req.path = kChatCompletionsPath;
    req.headers = headers;
    req.body = request_body.dump();
    req.set_header("Content-Type", "application/json");

    // Set content receiver for streaming response
    req.content_receiver = [this, &accumulated_data](
                               const char* data, size_t data_length,
                               uint64_t /*offset*/, uint64_t /*total_length*/) {
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

        // Check if we should stop - atomic read, no lock needed
        if (should_stop_) {
          return false;
        }

        parse_sse_line(line);
      }

      return true;  // Continue receiving
    };

    httplib::Response res;
    httplib::Error error;

    if (!client.send(req, res, error)) {
      std::string error_msg = "Network error: " + httplib::to_string(error);
      spdlog::error("Failed to send stream request: {}", error_msg);
      push_event(create_error_event(error_msg));
    } else if (res.status != 200) {
      spdlog::error("OpenAI stream API returned status {} - body: {}",
                    res.status, res.body);
      push_event(create_error_event("HTTP " + std::to_string(res.status) +
                                    " error: " + res.body));
    } else {
    }
  } catch (const std::exception& e) {
    spdlog::error("Exception in stream thread: {}", e.what());
    push_event(create_error_event(e.what()));
  }

  mark_complete();
}

void OpenAIStreamImpl::parse_sse_line(const std::string& line) {
  if (line.starts_with("data: ")) {
    auto data = line.substr(6);

    if (data == "[DONE]") {
      push_finish_event_if_needed();
      mark_complete();
      return;
    }

    try {
      auto json = nlohmann::json::parse(data);
      auto& choices = json["choices"];

      if (!choices.empty() && choices[0].contains("delta")) {
        auto& delta = choices[0]["delta"];
        if (delta.contains("content") && !delta["content"].is_null()) {
          std::string content = delta["content"].get<std::string>();
          push_event(StreamEvent(content));
        }
      }

      // Check for finish_reason
      if (!choices.empty() && choices[0].contains("finish_reason") &&
          !choices[0]["finish_reason"].is_null()) {
        auto finish_reason_str = choices[0]["finish_reason"].get<std::string>();
        auto finish_reason = parse_finish_reason(finish_reason_str);

        finish_event_pushed_ = true;

        if (json.contains("usage")) {
          auto usage = parse_usage(json["usage"]);
          push_event(StreamEvent(kStreamEventTypeFinish, usage, finish_reason));
        } else {
          push_event(StreamEvent(kStreamEventTypeFinish));
        }
      }
    } catch (const std::exception& e) {
      spdlog::error("Failed to parse SSE line: {} - Line content: {}", e.what(),
                    data);
    }
  } else if (!line.empty()) {
  }
}

void OpenAIStreamImpl::push_event(StreamEvent event) {
  event_queue_.enqueue(std::move(event));
}

void OpenAIStreamImpl::push_finish_event_if_needed() {
  bool expected = false;
  if (finish_event_pushed_.compare_exchange_strong(expected, true)) {
    event_queue_.enqueue(StreamEvent(kStreamEventTypeFinish));
  } else {
  }
}

void OpenAIStreamImpl::mark_complete() {
  is_complete_ = true;  // Atomic write
}

StreamEvent OpenAIStreamImpl::create_error_event(const std::string& message) {
  return StreamEvent(kStreamEventTypeError, message);
}

FinishReason OpenAIStreamImpl::parse_finish_reason(
    const std::string& reason_str) {
  if (reason_str == "stop") {
    return kFinishReasonStop;
  } else if (reason_str == "length") {
    return kFinishReasonLength;
  } else if (reason_str == "content_filter") {
    return kFinishReasonContentFilter;
  }
  return kFinishReasonStop;
}

Usage OpenAIStreamImpl::parse_usage(const nlohmann::json& usage_json) {
  Usage usage;
  usage.prompt_tokens = usage_json.value("prompt_tokens", 0);
  usage.completion_tokens = usage_json.value("completion_tokens", 0);
  usage.total_tokens = usage_json.value("total_tokens", 0);
  return usage;
}

}  // namespace openai
}  // namespace ai
