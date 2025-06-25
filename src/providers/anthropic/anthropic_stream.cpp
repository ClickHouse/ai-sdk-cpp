#include "anthropic_stream.h"

#include <chrono>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

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
  spdlog::debug("Starting Anthropic stream to URL: {}", url);

  // Start streaming in a separate thread
  stream_thread_ = std::thread([this, url, headers, request_body]() {
    try {
      run_stream(url, headers, request_body);
    } catch (const std::exception& e) {
      spdlog::error("Stream thread exception: {}", e.what());
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
      spdlog::debug("Stream complete and queue empty, returning empty event");
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

  spdlog::debug("Dequeued event type: {}", static_cast<int>(event.type));
  return event;
}

bool AnthropicStreamImpl::has_more_events() const {
  return event_queue_.size_approx() > 0 || !stream_complete_;
}

void AnthropicStreamImpl::stop_stream() {
  spdlog::debug("Stopping Anthropic stream");
  stop_requested_ = true;
  if (stream_thread_.joinable()) {
    stream_thread_.join();
  }
}

void AnthropicStreamImpl::run_stream(const std::string& url,
                                     const httplib::Headers& headers,
                                     const nlohmann::json& request_body) {
  spdlog::debug("Performing stream request");

  // Parse URL to extract host and path
  std::string host, path;
  bool use_ssl = true;

  if (url.starts_with("https://")) {
    host = url.substr(8);
    use_ssl = true;
  } else if (url.starts_with("http://")) {
    host = url.substr(7);
    use_ssl = false;
  }

  if (auto pos = host.find('/'); pos != std::string::npos) {
    path = host.substr(pos);
    host = host.substr(0, pos);
  } else {
    path = "/v1/messages";
  }

  spdlog::debug("Stream host: {}, path: {}, SSL: {}", host, path, use_ssl);

  try {
    if (use_ssl) {
      httplib::SSLClient client(host);
      client.set_connection_timeout(30, 0);
      client.set_read_timeout(120, 0);
      client.enable_server_certificate_verification(false);

      auto result =
          client.Post(path, headers, request_body.dump(), "application/json");

      if (result && result->status == 200) {
        parse_sse_response(result->body);
      } else {
        handle_stream_error(result ? result->status : 0,
                            result ? result->body : "Connection failed");
      }
    } else {
      httplib::Client client(host);
      client.set_connection_timeout(30, 0);
      client.set_read_timeout(120, 0);

      auto result =
          client.Post(path, headers, request_body.dump(), "application/json");

      if (result && result->status == 200) {
        parse_sse_response(result->body);
      } else {
        handle_stream_error(result ? result->status : 0,
                            result ? result->body : "Connection failed");
      }
    }
  } catch (const std::exception& e) {
    spdlog::error("Stream request exception: {}", e.what());
    handle_stream_error(0, std::string("Request failed: ") + e.what());
  }

  mark_complete();
}

void AnthropicStreamImpl::parse_sse_response(const std::string& response) {
  spdlog::debug("Processing SSE response, size: {}", response.size());

  std::istringstream stream(response);
  std::string line;
  std::string event_data;

  while (std::getline(stream, line) && !stop_requested_) {
    if (line.empty()) {
      // Empty line signals end of event
      if (!event_data.empty()) {
        process_sse_event(event_data);
        event_data.clear();
      }
    } else if (line.starts_with("data: ")) {
      event_data = line.substr(6);
    }
  }

  spdlog::debug("SSE processing complete");
}

void AnthropicStreamImpl::process_sse_event(const std::string& data) {
  if (data == "[DONE]") {
    spdlog::debug("Received SSE [DONE] event");
    return;
  }

  try {
    auto json_event = nlohmann::json::parse(data);
    std::string event_type = json_event.value("type", "");

    spdlog::debug("Processing SSE event type: {}", event_type);

    if (event_type == "message_start") {
      // Start of message - could extract metadata here
      return;
    } else if (event_type == "content_block_start") {
      // Start of content block
      return;
    } else if (event_type == "content_block_delta") {
      // Text delta - this is what we want to stream
      if (json_event.contains("delta") &&
          json_event["delta"].contains("text")) {
        std::string text = json_event["delta"]["text"];

        StreamEvent event(text);
        push_event(event);

        spdlog::debug("Enqueued text delta: '{}'", text);
      }
    } else if (event_type == "content_block_stop") {
      // End of content block
      return;
    } else if (event_type == "message_delta") {
      // Message-level delta (could contain stop reason)
      return;
    } else if (event_type == "message_stop") {
      // End of message
      StreamEvent event(kStreamEventTypeFinish, Usage{}, kFinishReasonStop);
      push_event(event);

      spdlog::debug("Enqueued finish event");
    }
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse SSE event: {}", e.what());
  }
}

void AnthropicStreamImpl::push_event(StreamEvent event) {
  event_queue_.enqueue(event);
}

void AnthropicStreamImpl::mark_complete() {
  stream_complete_ = true;

  // Push final finish event if not already done
  StreamEvent finish_event(kStreamEventTypeFinish);
  push_event(finish_event);
}

StreamEvent AnthropicStreamImpl::create_error_event(
    const std::string& message) {
  return StreamEvent(kStreamEventTypeError, message);
}

void AnthropicStreamImpl::handle_stream_error(int status_code,
                                              const std::string& error_body) {
  spdlog::error("Stream error - status: {}, body: {}", status_code, error_body);

  StreamEvent error_event(
      kStreamEventTypeError,
      "Stream error (" + std::to_string(status_code) + "): " + error_body);
  push_event(error_event);
}

}  // namespace anthropic
}  // namespace ai