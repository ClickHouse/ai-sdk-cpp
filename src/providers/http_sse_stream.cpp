#include "http_sse_stream.h"

#include "ai/logger.h"

#include <chrono>
#include <utility>

namespace {
constexpr auto kEventTimeout = static_cast<std::chrono::seconds>(30);
constexpr auto kSleepInterval = std::chrono::milliseconds(1);
}  // namespace

namespace ai {
namespace providers {
namespace streaming {

HttpSseStream::HttpSseStream(Options options) : options_(std::move(options)) {}

HttpSseStream::~HttpSseStream() {
  // Concrete classes join in their own destructor (their state is still alive
  // for the stream thread there); this is a backstop for the base state.
  stop_stream();
}

void HttpSseStream::start_stream(const std::string& url,
                                 const httplib::Headers& headers,
                                 const nlohmann::json& request_body) {
  ai::logger::log_debug("Starting stream to URL: {}", url);

  std::lock_guard<std::mutex> lock(thread_mutex_);

  if (stream_thread_.joinable()) {
    ai::logger::log_debug(
        "Stream thread already running, not starting new one");
    return;  // Already running
  }

  // Reset state for new stream
  should_stop_ = false;
  is_complete_ = false;
  finish_event_pushed_ = false;
  stream_errored_ = false;
  line_buffer_.clear();
  pending_tool_calls_.clear();
  finish_reason_ = kFinishReasonStop;
  usage_.reset();
  reset_stream_state();

  ai::logger::log_info("Launching stream thread");

  stream_thread_ = std::thread([this, url, headers, request_body]() {
    run_stream(url, headers, request_body);
  });
}

StreamEvent HttpSseStream::get_next_event() {
  StreamEvent event("");
  auto start_time = std::chrono::steady_clock::now();

  while (!event_queue_.try_dequeue(event)) {
    if (is_complete_ && event_queue_.size_approx() == 0) {
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

bool HttpSseStream::has_more_events() const {
  // No locks needed - these are atomic operations
  return event_queue_.size_approx() > 0 || !is_complete_;
}

void HttpSseStream::stop_stream() {
  ai::logger::log_debug("Stopping stream");

  should_stop_ = true;  // Atomic write

  std::lock_guard<std::mutex> lock(thread_mutex_);
  if (stream_thread_.joinable()) {
    ai::logger::log_debug("Waiting for stream thread to finish");
    stream_thread_.join();
    ai::logger::log_info("Stream stopped successfully");
  }
}

void HttpSseStream::run_stream(const std::string& url,
                               const httplib::Headers& headers,
                               const nlohmann::json& request_body) {
  // Extract origin and path from URL. Using httplib::Client with the scheme
  // preserved supports both HTTPS provider endpoints and HTTP test/gateway
  // endpoints.
  const auto [origin, path] = split_origin_and_path(url, options_.default_path);

  ai::logger::log_debug(
      "Stream thread started - connecting to {} with path: {}", origin, path);

  try {
    httplib::Client client(origin);
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(options_.connection_timeout_seconds, 0);
    client.set_read_timeout(options_.read_timeout_seconds, 0);

    int response_status = 0;
    std::string error_body;

    httplib::Request request;
    request.method = "POST";
    request.path = path;
    request.headers = headers;
    request.body = request_body.dump();
    request.set_header("Content-Type", "application/json");

    ai::logger::log_debug(
        "Stream request prepared - path: {}, body size: {} bytes", path,
        request.body.length());

    // Capture the status before the body arrives so error bodies can be
    // preserved (httplib routes the body of every status through the content
    // receiver, leaving response.body empty).
    request.response_handler =
        [&response_status](const httplib::Response& response) {
          response_status = response.status;
          return true;
        };

    request.content_receiver = [this, &response_status, &error_body](
                                   const char* data, size_t length,
                                   uint64_t /*offset*/,
                                   uint64_t /*total_length*/) {
      if (should_stop_) {
        return false;
      }
      if (response_status != 200) {
        // Error responses are not SSE; keep the body for diagnostics.
        error_body.append(data, length);
        return true;
      }
      consume_chunk(data, length);
      return !should_stop_;
    };

    httplib::Response response;
    httplib::Error error;

    ai::logger::log_info("Sending stream request");

    if (!client.send(request, response, error)) {
      // A deliberate stop makes the receiver return false (Error::Canceled);
      // do not report that as a network failure.
      if (!should_stop_) {
        std::string message = "Network error: " + httplib::to_string(error);
        ai::logger::log_error("Failed to send stream request: {}", message);
        note_stream_error();
        push_event(create_error_event(message));
      }
    } else if (response.status != 200) {
      ai::logger::log_error("Stream API returned status {} - body: {}",
                            response.status, error_body);
      note_stream_error();
      push_event(create_error_event("HTTP " + std::to_string(response.status) +
                                    " error: " + error_body));
    } else {
      if (!line_buffer_.empty()) {
        // Process a trailing line that arrived without a final newline.
        consume_chunk("\n", 1);
      }
      if (!stream_errored_) {
        finalize_stream();
        // Tool calls whose terminating event never arrived (some gateways
        // close the connection early) must not be silently dropped.
        flush_all_pending_tool_calls();
      }
      ai::logger::log_info("Stream completed successfully");
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Exception in stream thread: {}", e.what());
    note_stream_error();
    push_event(create_error_event(e.what()));
  }

  mark_complete();
  ai::logger::log_debug("Stream thread exiting");
}

void HttpSseStream::consume_chunk(const char* data, std::size_t length) {
  line_buffer_.append(data, length);

  std::size_t start = 0;
  std::size_t newline = 0;
  while ((newline = line_buffer_.find('\n', start)) != std::string::npos) {
    std::string_view line(line_buffer_.data() + start, newline - start);
    start = newline + 1;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    process_sse_line(line);
  }
  line_buffer_.erase(0, start);
}

void HttpSseStream::push_event(StreamEvent event) {
  event_queue_.enqueue(std::move(event));
}

StreamEvent HttpSseStream::create_error_event(const std::string& message) {
  ai::logger::log_debug("Creating error event: {}", message);
  return StreamEvent(kStreamEventTypeError, message);
}

void HttpSseStream::push_finish_event_if_needed() {
  bool expected = false;
  if (!finish_event_pushed_.compare_exchange_strong(expected, true)) {
    ai::logger::log_debug("Finish event already pushed, skipping");
    return;
  }

  ai::logger::log_debug("Pushing finish event to queue");
  if (stream_errored_) {
    // Do not stamp a failed stream with a clean finish reason or usage.
    push_event(StreamEvent(kStreamEventTypeFinish, kFinishReasonError));
  } else if (usage_) {
    push_event(StreamEvent(kStreamEventTypeFinish, *usage_, finish_reason_));
  } else {
    push_event(StreamEvent(kStreamEventTypeFinish, finish_reason_));
  }
}

void HttpSseStream::mark_complete() {
  push_finish_event_if_needed();
  is_complete_ = true;  // Atomic write
}

void HttpSseStream::flush_all_pending_tool_calls() {
  for (const auto& entry : pending_tool_calls_) {
    push_event(make_tool_call_event(entry.second));
  }
  pending_tool_calls_.clear();
}

void HttpSseStream::flush_pending_tool_call(std::size_t index) {
  const auto pending_it = pending_tool_calls_.find(index);
  if (pending_it == pending_tool_calls_.end()) {
    return;
  }
  push_event(make_tool_call_event(pending_it->second));
  pending_tool_calls_.erase(pending_it);
}

}  // namespace streaming
}  // namespace providers
}  // namespace ai
