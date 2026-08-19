#pragma once

#include "ai/types/stream_result.h"

#include <atomic>
#include <concurrentqueue.h>
#include <httplib.h>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

class OpenAIStreamImpl : public internal::StreamResultImpl {
 public:
  OpenAIStreamImpl() = default;
  ~OpenAIStreamImpl();

  // Non-copyable, non-movable for thread safety
  OpenAIStreamImpl(const OpenAIStreamImpl&) = delete;
  OpenAIStreamImpl& operator=(const OpenAIStreamImpl&) = delete;
  OpenAIStreamImpl(OpenAIStreamImpl&&) = delete;
  OpenAIStreamImpl& operator=(OpenAIStreamImpl&&) = delete;

  void start_stream(const std::string& url,
                    const httplib::Headers& headers,
                    const nlohmann::json& request_body);

  StreamEvent get_next_event() override;
  bool has_more_events() const override;
  void stop_stream() override;

#ifdef AI_SDK_TESTING
  void process_sse_line_for_testing(const std::string& line) {
    parse_sse_line(line);
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
  void parse_sse_line(const std::string& line);
  void push_event(StreamEvent event);
  void push_finish_event_if_needed();
  void flush_tool_calls();
  void mark_complete();

  // Helper functions
  StreamEvent create_error_event(const std::string& message);
  FinishReason parse_finish_reason(const std::string& reason_str);
  Usage parse_usage(const nlohmann::json& usage_json);

  moodycamel::ConcurrentQueue<StreamEvent> event_queue_;
  std::thread stream_thread_;
  std::mutex thread_mutex_;
  std::atomic<bool> is_complete_{false};
  std::atomic<bool> should_stop_{false};
  std::atomic<bool> finish_event_pushed_{false};
  std::map<std::size_t, PendingToolCall> pending_tool_calls_;
  FinishReason pending_finish_reason_{kFinishReasonStop};
  std::optional<Usage> pending_usage_;
};

}  // namespace openai
}  // namespace ai
