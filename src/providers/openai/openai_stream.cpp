#include "openai_stream.h"

#include "ai/logger.h"

namespace ai {
namespace openai {

OpenAIStreamImpl::~OpenAIStreamImpl() {
  // Join before this class's state is destroyed; the stream thread calls
  // process_sse_line.
  stop_stream();
}

void OpenAIStreamImpl::process_sse_line(std::string_view line) {
  if (!line.starts_with("data: ")) {
    if (!line.empty()) {
      ai::logger::log_debug("Ignoring non-data SSE line: {}", line);
    }
    return;
  }

  auto data = line.substr(6);

  ai::logger::log_debug("Processing SSE line - data length: {}", data.length());

  if (data == "[DONE]") {
    ai::logger::log_debug("Received [DONE] signal, stream ending");
    flush_all_pending_tool_calls();
    mark_complete();
    return;
  }

  try {
    auto json = nlohmann::json::parse(data);
    auto& choices = json["choices"];

    if (!choices.empty() && choices[0].contains("delta")) {
      auto& delta = choices[0]["delta"];
      if (delta.contains("content") && !delta["content"].is_null()) {
        std::string content = delta["content"].get<std::string>();
        ai::logger::log_debug("Received content chunk - length: {}",
                              content.length());
        push_event(StreamEvent(content));
      }

      if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
        for (const auto& fragment : delta["tool_calls"]) {
          const auto index = fragment.value("index", std::size_t{0});
          auto& pending = pending_tool_calls_[index];

          if (fragment.contains("id") && fragment["id"].is_string()) {
            pending.id = fragment["id"].get<std::string>();
          }
          if (fragment.contains("function") &&
              fragment["function"].is_object()) {
            const auto& function = fragment["function"];
            if (function.contains("name") && function["name"].is_string()) {
              pending.name = function["name"].get<std::string>();
            }
            if (function.contains("arguments") &&
                function["arguments"].is_string()) {
              pending.arguments += function["arguments"].get<std::string>();
            }
          }
        }
      }
    }

    // Check for finish_reason
    if (!choices.empty() && choices[0].contains("finish_reason") &&
        !choices[0]["finish_reason"].is_null()) {
      auto finish_reason_str = choices[0]["finish_reason"].get<std::string>();
      finish_reason_ = parse_finish_reason(finish_reason_str);

      if (finish_reason_ == kFinishReasonToolCalls) {
        flush_all_pending_tool_calls();
      }

      ai::logger::log_debug("Stream finished with reason: {}",
                            finish_reason_str);
    }

    if (json.contains("usage") && !json["usage"].is_null()) {
      usage_ = parse_usage(json["usage"]);
      ai::logger::log_info(
          "Stream completed - tokens used: {} prompt, {} completion, {} "
          "total",
          usage_->prompt_tokens, usage_->completion_tokens,
          usage_->total_tokens);
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Failed to parse SSE line: {} - Line content: {}",
                          e.what(), data);
  }
}

FinishReason OpenAIStreamImpl::parse_finish_reason(
    const std::string& reason_str) {
  if (reason_str == "stop") {
    return kFinishReasonStop;
  } else if (reason_str == "length") {
    return kFinishReasonLength;
  } else if (reason_str == "content_filter") {
    return kFinishReasonContentFilter;
  } else if (reason_str == "tool_calls") {
    return kFinishReasonToolCalls;
  }
  return kFinishReasonStop;
}

Usage OpenAIStreamImpl::parse_usage(const nlohmann::json& usage_json) {
  Usage usage;
  usage.prompt_tokens = usage_json.value("prompt_tokens", 0);
  usage.completion_tokens = usage_json.value("completion_tokens", 0);
  usage.total_tokens = usage_json.value("total_tokens", 0);
  return usage;
}

}  // namespace openai
}  // namespace ai
