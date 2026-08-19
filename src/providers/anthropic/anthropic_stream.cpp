#include "anthropic_stream.h"

#include "ai/logger.h"

#include <chrono>
#include <thread>

namespace {
constexpr auto kEventTimeout = static_cast<std::chrono::seconds>(30);
constexpr auto kSleepInterval = std::chrono::milliseconds(1);
}  // namespace

namespace ai {
namespace anthropic {

AnthropicStreamImpl::~AnthropicStreamImpl() {
  stop_stream();
}

void AnthropicStreamImpl::start_stream(const std::string& url,
                                       const httplib::Headers& headers,
                                       const nlohmann::json& request_body) {
  ai::logger::log_debug("Starting Anthropic stream to URL: {}", url);

  stop_requested_ = false;
  stream_complete_ = false;
  finish_event_pushed_ = false;
  sse_buffer_.clear();
  event_data_.clear();
  usage_ = Usage{};
  finish_reason_ = kFinishReasonStop;
  pending_tool_calls_.clear();

  // Start streaming in a separate thread
  stream_thread_ = std::thread([this, url, headers, request_body]() {
    try {
      run_stream(url, headers, request_body);
    } catch (const std::exception& e) {
      ai::logger::log_error("Stream thread exception: {}", e.what());
      StreamEvent error_event(kStreamEventTypeError,
                              std::string("Stream error: ") + e.what());
      push_event(error_event);
      mark_complete();
    }
  });
}

StreamEvent AnthropicStreamImpl::get_next_event() {
  StreamEvent event("");
  auto start_time = std::chrono::steady_clock::now();

  while (!event_queue_.try_dequeue(event)) {
    if (stream_complete_ && event_queue_.size_approx() == 0) {
      // Stream is complete and queue is empty
      ai::logger::log_debug(
          "Stream complete and queue empty, returning empty event");
      return StreamEvent("");
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() - start_time > kEventTimeout) {
      ai::logger::log_error(
          "Timeout waiting for next stream event after {} seconds",
          kEventTimeout.count());
      return StreamEvent(kStreamEventTypeError,
                         "Timeout waiting for next event");
    }

    std::this_thread::sleep_for(kSleepInterval);
  }

  ai::logger::log_debug("Dequeued event type: {}",
                        static_cast<int>(event.type));
  return event;
}

bool AnthropicStreamImpl::has_more_events() const {
  return event_queue_.size_approx() > 0 || !stream_complete_;
}

void AnthropicStreamImpl::stop_stream() {
  ai::logger::log_debug("Stopping Anthropic stream");
  stop_requested_ = true;
  if (stream_thread_.joinable()) {
    stream_thread_.join();
  }
}

void AnthropicStreamImpl::run_stream(const std::string& url,
                                     const httplib::Headers& headers,
                                     const nlohmann::json& request_body) {
  ai::logger::log_debug("Performing stream request");

  std::string_view url_view(url);
  const auto scheme_pos = url_view.find("://");
  const auto authority_start =
      scheme_pos == std::string_view::npos ? 0 : scheme_pos + 3;
  const auto slash_pos = url_view.find('/', authority_start);
  const std::string origin(url_view.substr(0, slash_pos));
  const std::string path = slash_pos == std::string_view::npos
                               ? "/v1/messages"
                               : std::string(url_view.substr(slash_pos));

  ai::logger::log_debug("Stream origin: {}, path: {}", origin, path);

  try {
    httplib::Client client(origin);
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    httplib::Request request;
    request.method = "POST";
    request.path = path;
    request.headers = headers;
    request.body = request_body.dump();
    request.set_header("Content-Type", "application/json");
    request.content_receiver = [this](const char* data, size_t length, uint64_t,
                                      uint64_t) {
      if (stop_requested_) {
        return false;
      }
      consume_sse_chunk(std::string(data, length));
      return !stop_requested_;
    };

    httplib::Response response;
    httplib::Error error;
    if (!client.send(request, response, error)) {
      handle_stream_error(0, "Network error: " + httplib::to_string(error));
    } else if (response.status != 200) {
      handle_stream_error(response.status, response.body);
    } else {
      if (!sse_buffer_.empty()) {
        consume_sse_chunk("\n");
      }
      if (!event_data_.empty()) {
        process_sse_event(event_data_);
        event_data_.clear();
      }
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Stream request exception: {}", e.what());
    handle_stream_error(0, std::string("Request failed: ") + e.what());
  }

  mark_complete();
}

void AnthropicStreamImpl::consume_sse_chunk(const std::string& chunk) {
  sse_buffer_ += chunk;

  std::size_t newline = 0;
  while ((newline = sse_buffer_.find('\n')) != std::string::npos) {
    auto line = sse_buffer_.substr(0, newline);
    sse_buffer_.erase(0, newline + 1);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty()) {
      if (!event_data_.empty()) {
        process_sse_event(event_data_);
        event_data_.clear();
      }
    } else if (line.starts_with("data:")) {
      auto data = line.substr(5);
      if (!data.empty() && data.front() == ' ') {
        data.erase(0, 1);
      }
      if (!event_data_.empty()) {
        event_data_ += '\n';
      }
      event_data_ += data;
    }
  }
}

void AnthropicStreamImpl::process_sse_event(const std::string& data) {
  if (data == "[DONE]") {
    ai::logger::log_debug("Received SSE [DONE] event");
    return;
  }

  try {
    auto json_event = nlohmann::json::parse(data);
    std::string event_type = json_event.value("type", "");

    ai::logger::log_debug("Processing SSE event type: {}", event_type);

    if (event_type == "message_start") {
      if (json_event.contains("message") &&
          json_event["message"].contains("usage")) {
        const auto& usage = json_event["message"]["usage"];
        usage_.prompt_tokens = usage.value("input_tokens", 0);
        usage_.total_tokens = usage_.prompt_tokens + usage_.completion_tokens;
      }
      return;
    } else if (event_type == "content_block_start") {
      if (json_event.contains("content_block") &&
          json_event["content_block"].value("type", "") == "tool_use") {
        const auto index = json_event.value("index", std::size_t{0});
        const auto& block = json_event["content_block"];
        auto& pending = pending_tool_calls_[index];
        pending.id = block.value("id", "");
        pending.name = block.value("name", "");
        if (block.contains("input") && !block["input"].empty()) {
          pending.arguments = block["input"].dump();
        }
      }
      return;
    } else if (event_type == "content_block_delta") {
      // Text delta - this is what we want to stream
      if (json_event.contains("delta") &&
          json_event["delta"].contains("text")) {
        std::string text = json_event["delta"]["text"];

        StreamEvent event(text);
        push_event(event);

        ai::logger::log_debug("Enqueued text delta: '{}'", text);
      } else if (json_event.contains("delta") &&
                 json_event["delta"].value("type", "") == "input_json_delta") {
        const auto index = json_event.value("index", std::size_t{0});
        pending_tool_calls_[index].arguments +=
            json_event["delta"].value("partial_json", "");
      }
    } else if (event_type == "content_block_stop") {
      flush_tool_call(json_event.value("index", std::size_t{0}));
      return;
    } else if (event_type == "message_delta") {
      if (json_event.contains("delta")) {
        const auto stop_reason = json_event["delta"].value("stop_reason", "");
        if (stop_reason == "max_tokens") {
          finish_reason_ = kFinishReasonLength;
        } else if (stop_reason == "tool_use") {
          finish_reason_ = kFinishReasonToolCalls;
        } else if (!stop_reason.empty()) {
          finish_reason_ = kFinishReasonStop;
        }
      }
      if (json_event.contains("usage")) {
        usage_.completion_tokens =
            json_event["usage"].value("output_tokens", 0);
        usage_.total_tokens = usage_.prompt_tokens + usage_.completion_tokens;
      }
      return;
    } else if (event_type == "message_stop") {
      push_finish_event_if_needed();
    } else if (event_type == "error") {
      const auto message = json_event.value("error", nlohmann::json::object())
                               .value("message", "Anthropic stream error");
      push_event(create_error_event(message));
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Failed to parse SSE event: {}", e.what());
  }
}

void AnthropicStreamImpl::push_event(const StreamEvent& event) {
  event_queue_.enqueue(event);
}

void AnthropicStreamImpl::flush_tool_call(std::size_t index) {
  const auto pending_it = pending_tool_calls_.find(index);
  if (pending_it == pending_tool_calls_.end()) {
    return;
  }

  const auto& pending = pending_it->second;
  try {
    auto arguments = pending.arguments.empty()
                         ? nlohmann::json::object()
                         : nlohmann::json::parse(pending.arguments);
    push_event(
        StreamEvent(ToolCall(pending.id, pending.name, std::move(arguments))));
  } catch (const nlohmann::json::exception& e) {
    push_event(
        create_error_event("Failed to parse streamed tool-call arguments: " +
                           std::string(e.what())));
  }
  pending_tool_calls_.erase(pending_it);
}

void AnthropicStreamImpl::mark_complete() {
  push_finish_event_if_needed();
  stream_complete_ = true;
}

void AnthropicStreamImpl::push_finish_event_if_needed() {
  bool expected = false;
  if (finish_event_pushed_.compare_exchange_strong(expected, true)) {
    push_event(StreamEvent(kStreamEventTypeFinish, usage_, finish_reason_));
    ai::logger::log_debug("Enqueued finish event");
  }
}

StreamEvent AnthropicStreamImpl::create_error_event(
    const std::string& message) {
  return StreamEvent(kStreamEventTypeError, message);
}

void AnthropicStreamImpl::handle_stream_error(int status_code,
                                              const std::string& error_body) {
  ai::logger::log_error("Stream error - status: {}, body: {}", status_code,
                        error_body);

  StreamEvent error_event(
      kStreamEventTypeError,
      "Stream error (" + std::to_string(status_code) + "): " + error_body);
  push_event(error_event);
}

}  // namespace anthropic
}  // namespace ai
