#pragma once

#include "providers/http_sse_stream.h"

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

class OpenAIStreamImpl : public providers::streaming::HttpSseStream {
 public:
  OpenAIStreamImpl()
      : HttpSseStream({.default_path = "/v1/chat/completions",
                       .connection_timeout_seconds = 30,
                       // 5 minutes for long generations
                       .read_timeout_seconds = 300}) {}
  ~OpenAIStreamImpl() override;

 private:
  void process_sse_line(std::string_view line) override;

  // Helper functions
  static FinishReason parse_finish_reason(const std::string& reason_str);
  static Usage parse_usage(const nlohmann::json& usage_json);
};

}  // namespace openai
}  // namespace ai
