#pragma once

#include "ai/types/stream_result.h"

#include <atomic>
#include <httplib.h>
#include <mutex>
#include <thread>

#include <concurrentqueue/concurrentqueue.h>
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

 private:
  void run_stream(const std::string& url,
                  const httplib::Headers& headers,
                  const nlohmann::json& request_body);
  void parse_sse_response(const std::string& response);
  void process_sse_event(const std::string& data);
  void push_event(const StreamEvent& event);
  void mark_complete();

  // Helper functions
  StreamEvent create_error_event(const std::string& message);
  void handle_stream_error(int status_code, const std::string& error_body);

  moodycamel::ConcurrentQueue<StreamEvent> event_queue_;
  std::thread stream_thread_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> stream_complete_{false};
};

}  // namespace anthropic
}  // namespace ai