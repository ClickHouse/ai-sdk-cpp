#pragma once

#include "providers/http_sse_stream.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace ai {
namespace google {

/// `streamGenerateContent?alt=sse`: every SSE event carries a complete
/// `GenerateContentResponse` chunk; the last one has the `finishReason`.
class GoogleStreamImpl : public providers::streaming::HttpSseStream {
 public:
  GoogleStreamImpl()
      : HttpSseStream({.default_path = "/",
                       .connection_timeout_seconds = 30,
                       .read_timeout_seconds = 120}) {}
  ~GoogleStreamImpl() override;

 private:
  void reset_stream_state() override;
  void process_sse_line(std::string_view line) override;
  void finalize_stream() override;

  void process_sse_event(const std::string& data);

  std::string event_data_;
  std::size_t call_index_ = 0;
  bool saw_function_call_ = false;
};

}  // namespace google
}  // namespace ai
