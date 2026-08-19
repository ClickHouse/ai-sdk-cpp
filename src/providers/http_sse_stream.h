#pragma once

#include "ai/types/stream_result.h"
#include "providers/stream_utils.h"

#include <atomic>
#include <concurrentqueue.h>
#include <cstddef>
#include <httplib.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <nlohmann/json.hpp>

namespace ai {
namespace providers {
namespace streaming {

/// Threaded HTTP SSE stream shared by the provider stream implementations.
/// Owns the event queue, the stream thread, and the HTTP request plus SSE
/// line-splitting plumbing; derived classes only translate provider-specific
/// SSE lines into stream events.
///
/// Concrete classes must call stop_stream() in their destructor so the stream
/// thread is joined before their members are destroyed.
class HttpSseStream : public internal::StreamResultImpl {
 public:
  ~HttpSseStream() override;

  // Non-copyable, non-movable for thread safety
  HttpSseStream(const HttpSseStream&) = delete;
  HttpSseStream& operator=(const HttpSseStream&) = delete;
  HttpSseStream(HttpSseStream&&) = delete;
  HttpSseStream& operator=(HttpSseStream&&) = delete;

  void start_stream(const std::string& url,
                    const httplib::Headers& headers,
                    const nlohmann::json& request_body);

  StreamEvent get_next_event() override;
  bool has_more_events() const override;
  void stop_stream() override;

#ifdef AI_SDK_TESTING
  void process_sse_chunk_for_testing(const std::string& chunk) {
    consume_chunk(chunk.data(), chunk.size());
  }
  void process_sse_line_for_testing(const std::string& line) {
    process_sse_line(line);
  }
  std::size_t queued_event_count_for_testing() const {
    return event_queue_.size_approx();
  }
#endif

 protected:
  struct Options {
    std::string default_path;  // Used when the URL carries no path.
    time_t connection_timeout_seconds{30};
    time_t read_timeout_seconds{120};
  };

  explicit HttpSseStream(Options options);

  /// Reset provider-specific parse state before a new stream starts.
  virtual void reset_stream_state() {}

  /// Handle one decoded SSE line (trailing CR already stripped).
  virtual void process_sse_line(std::string_view line) = 0;

  /// Called after a successful response body has been fully consumed, before
  /// pending tool calls are flushed and the finish event is pushed; flush any
  /// provider-side leftovers here.
  virtual void finalize_stream() {}

  void push_event(StreamEvent event);
  static StreamEvent create_error_event(const std::string& message);
  /// Push the terminal finish event (at most once per stream).
  void push_finish_event_if_needed();
  /// Push the terminal finish event if still needed and mark the stream
  /// complete so consumers stop waiting for events.
  void mark_complete();
  /// Record that the stream failed; the finish event will carry
  /// kFinishReasonError and no usage.
  void note_stream_error() { stream_errored_ = true; }

  /// Emit all accumulated tool calls (or parse-error events) and clear them.
  void flush_all_pending_tool_calls();
  /// Emit one accumulated tool call by content-block index, if present.
  void flush_pending_tool_call(std::size_t index);

  // Terminal-finish payload and tool-call accumulator, maintained by the
  // derived parser and consumed when the finish event is pushed.
  FinishReason finish_reason_{kFinishReasonStop};
  std::optional<Usage> usage_;
  std::map<std::size_t, PendingToolCall> pending_tool_calls_;

 private:
  void run_stream(const std::string& url,
                  const httplib::Headers& headers,
                  const nlohmann::json& request_body);
  void consume_chunk(const char* data, std::size_t length);

  Options options_;
  moodycamel::ConcurrentQueue<StreamEvent> event_queue_;
  std::thread stream_thread_;
  std::mutex thread_mutex_;
  std::string line_buffer_;
  std::atomic<bool> is_complete_{false};
  std::atomic<bool> should_stop_{false};
  std::atomic<bool> finish_event_pushed_{false};
  std::atomic<bool> stream_errored_{false};
};

}  // namespace streaming
}  // namespace providers
}  // namespace ai
