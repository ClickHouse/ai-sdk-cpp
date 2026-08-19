#pragma once

#include "providers/http_sse_stream.h"

#include <string>
#include <string_view>

namespace ai {
namespace anthropic {

class AnthropicStreamImpl : public providers::streaming::HttpSseStream {
 public:
  AnthropicStreamImpl()
      : HttpSseStream({.default_path = "/v1/messages",
                       .connection_timeout_seconds = 30,
                       .read_timeout_seconds = 120}) {}
  ~AnthropicStreamImpl() override;

 private:
  void reset_stream_state() override;
  void process_sse_line(std::string_view line) override;
  void finalize_stream() override;

  void process_sse_event(const std::string& data);
  Usage& ensure_usage();

  // Anthropic SSE events span multiple "data:" lines; accumulate them here
  // until the blank line that terminates the event.
  std::string event_data_;
};

}  // namespace anthropic
}  // namespace ai
