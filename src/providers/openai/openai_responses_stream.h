#pragma once

#include "providers/http_sse_stream.h"

#include <string_view>

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

/// Translates the server-sent events of a streaming `POST /v1/responses`
/// request: text deltas, function calls (emitted when their output item is
/// done), and the terminal `response.completed`, `response.incomplete`,
/// `response.failed`, or `error` event.
class OpenAIResponsesStreamImpl : public providers::streaming::HttpSseStream {
 public:
  OpenAIResponsesStreamImpl()
      : HttpSseStream({.default_path = "/v1/responses",
                       .connection_timeout_seconds = 30,
                       // 5 minutes for long generations
                       .read_timeout_seconds = 300}) {}
  ~OpenAIResponsesStreamImpl() override;

 private:
  void reset_stream_state() override;
  void process_sse_line(std::string_view line) override;
  void finalize_stream() override;

  void handle_event(const nlohmann::json& event);
  void finish_response(const nlohmann::json& response);
  void fail(const std::string& message);

  bool has_function_calls_ = false;
  bool finished_ = false;
};

}  // namespace openai
}  // namespace ai
