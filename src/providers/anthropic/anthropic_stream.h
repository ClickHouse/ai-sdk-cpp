#pragma once

#include "ai/types/stream_result.h"

#include <atomic>
#include <concurrentqueue.h>
#include <cstddef>
#include <httplib.h>
#include <map>
#include <mutex>
#include <thread>

#include <nlohmann/json.hpp>

namespace ai {
namespace anthropic {

class AnthropicStreamImpl : public internal::StreamResultImpl {
 public:
  AnthropicStreamImpl() = default;
  ~AnthropicStreamImpl();

  // Non-copyable, non-movable for thread safety
  AnthropicStreamImpl(const AnthropicStreamImpl&) = delete;
  AnthropicStreamImpl& operator=(const AnthropicStreamImpl&) = delete;
  AnthropicStreamImpl(AnthropicStreamImpl&&) = delete;
  AnthropicStreamImpl& operator=(AnthropicStreamImpl&&) = delete;

  void start_stream(const std::string& url,
                    const httplib::Headers& headers,
                    const nlohmann::json& request_body);

  StreamEvent get_next_event() override;
  bool has_more_events() const override;
  void stop_stream() override;

#ifdef AI_SDK_TESTING
  void process_sse_chunk_for_testing(const std::string& chunk) {
    consume_sse_chunk(chunk);
  }
  std::size_t queued_event_count_for_testing() const {
    return event_queue_.size_approx();
  }
#endif

 private:
  struct PendingToolCall {
    std::string id;
    std::string name;
    std::string arguments;
  };

  void run_stream(const std::string& url,
                  const httplib::Headers& headers,
                  const nlohmann::json& request_body);
  void consume_sse_chunk(const std::string& chunk);
  void process_sse_event(const std::string& data);
  void push_event(const StreamEvent& event);
  void flush_tool_call(std::size_t index);
  void push_finish_event_if_needed();
  void mark_complete();

  // Helper functions
  StreamEvent create_error_event(const std::string& message);
  void handle_stream_error(int status_code, const std::string& error_body);

  moodycamel::ConcurrentQueue<StreamEvent> event_queue_;
  std::thread stream_thread_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> stream_complete_{false};
  std::atomic<bool> finish_event_pushed_{false};
  std::string sse_buffer_;
  std::string event_data_;
  Usage usage_;
  FinishReason finish_reason_{kFinishReasonStop};
  std::map<std::size_t, PendingToolCall> pending_tool_calls_;
};

}  // namespace anthropic
}  // namespace ai
