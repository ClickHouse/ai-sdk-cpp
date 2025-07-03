#pragma once

#include "ai/types/stream_result.h"

#include <atomic>
#include <concurrentqueue.h>
#include <httplib.h>
#include <mutex>
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

 private:
  void run_stream(const std::string& url,
                  const httplib::Headers& headers,
                  const nlohmann::json& request_body);
  void parse_sse_line(const std::string& line);
  void push_event(StreamEvent event);
  void push_finish_event_if_needed();
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
};

}  // namespace openai
}  // namespace ai