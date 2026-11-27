#include "anthropic_stream.h"

#include "ai/logger.h"

namespace {
// Null-tolerant JSON field accessors: gateways may deliver fields as JSON
// null, which nlohmann's value() would throw on (discarding the whole event).
int int_or_zero(const nlohmann::json& obj, const char* key) {
  const auto it = obj.find(key);
  return it != obj.end() && it->is_number() ? it->get<int>() : 0;
}

std::string string_or_empty(const nlohmann::json& obj, const char* key) {
  const auto it = obj.find(key);
  return it != obj.end() && it->is_string() ? it->get<std::string>()
                                            : std::string{};
}

std::size_t index_or_zero(const nlohmann::json& obj) {
  const auto it = obj.find("index");
  return it != obj.end() && it->is_number_unsigned() ? it->get<std::size_t>()
                                                     : std::size_t{0};
}
}  // namespace

namespace ai {
namespace anthropic {

AnthropicStreamImpl::~AnthropicStreamImpl() {
  // Join before this class's state is destroyed; the stream thread calls
  // process_sse_line.
  stop_stream();
}

void AnthropicStreamImpl::reset_stream_state() {
  event_data_.clear();
}

void AnthropicStreamImpl::process_sse_line(std::string_view line) {
  if (line.empty()) {
    // Blank line terminates the SSE event
    if (!event_data_.empty()) {
      process_sse_event(event_data_);
      event_data_.clear();
    }
  } else if (line.starts_with("data:")) {
    auto data = line.substr(5);
    if (!data.empty() && data.front() == ' ') {
      data.remove_prefix(1);
    }
    if (!event_data_.empty()) {
      event_data_ += '\n';
    }
    event_data_.append(data);
  }
}

void AnthropicStreamImpl::finalize_stream() {
  // Dispatch an event whose terminating blank line never arrived.
  if (!event_data_.empty()) {
    process_sse_event(event_data_);
    event_data_.clear();
  }
}

Usage& AnthropicStreamImpl::ensure_usage() {
  if (!usage_) {
    usage_ = Usage{};
  }
  return *usage_;
}

void AnthropicStreamImpl::process_sse_event(const std::string& data) {
  if (data == "[DONE]") {
    ai::logger::log_debug("Received SSE [DONE] event");
    return;
  }

  try {
    auto json_event = nlohmann::json::parse(data);
    std::string event_type = string_or_empty(json_event, "type");

    ai::logger::log_debug("Processing SSE event type: {}", event_type);

    if (event_type == "message_start") {
      if (json_event.contains("message") && json_event["message"].is_object()) {
        const auto& message = json_event["message"];
        if (message.contains("usage") && message["usage"].is_object()) {
          auto& usage = ensure_usage();
          usage.prompt_tokens = int_or_zero(message["usage"], "input_tokens");
          usage.total_tokens = usage.prompt_tokens + usage.completion_tokens;
        }
      }
      return;
    } else if (event_type == "content_block_start") {
      if (json_event.contains("content_block") &&
          json_event["content_block"].is_object() &&
          string_or_empty(json_event["content_block"], "type") == "tool_use") {
        const auto index = index_or_zero(json_event);
        const auto& block = json_event["content_block"];
        auto& pending = pending_tool_calls_[index];
        pending.id = string_or_empty(block, "id");
        pending.name = string_or_empty(block, "name");
        if (block.contains("input") && block["input"].is_object() &&
            !block["input"].empty()) {
          // Keep separately from the delta accumulator: appending deltas to
          // an already complete JSON object would produce invalid JSON.
          pending.initial_input = block["input"].dump();
        }
      }
      return;
    } else if (event_type == "content_block_delta") {
      // Text delta - this is what we want to stream
      if (json_event.contains("delta") && json_event["delta"].is_object() &&
          json_event["delta"].contains("text") &&
          json_event["delta"]["text"].is_string()) {
        std::string text = json_event["delta"]["text"];

        push_event(StreamEvent(text));

        ai::logger::log_debug("Enqueued text delta: '{}'", text);
      } else if (json_event.contains("delta") &&
                 json_event["delta"].is_object() &&
                 string_or_empty(json_event["delta"], "type") ==
                     "input_json_delta") {
        const auto index = index_or_zero(json_event);
        pending_tool_calls_[index].arguments +=
            string_or_empty(json_event["delta"], "partial_json");
      }
    } else if (event_type == "content_block_stop") {
      flush_pending_tool_call(index_or_zero(json_event));
      return;
    } else if (event_type == "message_delta") {
      if (json_event.contains("delta") && json_event["delta"].is_object()) {
        const auto stop_reason =
            string_or_empty(json_event["delta"], "stop_reason");
        if (stop_reason == "max_tokens") {
          finish_reason_ = kFinishReasonLength;
        } else if (stop_reason == "tool_use") {
          finish_reason_ = kFinishReasonToolCalls;
        } else if (!stop_reason.empty()) {
          finish_reason_ = kFinishReasonStop;
        }
      }
      if (json_event.contains("usage") && json_event["usage"].is_object()) {
        auto& usage = ensure_usage();
        usage.completion_tokens =
            int_or_zero(json_event["usage"], "output_tokens");
        usage.total_tokens = usage.prompt_tokens + usage.completion_tokens;
      }
      return;
    } else if (event_type == "message_stop") {
      // The stream is semantically finished; complete immediately so
      // consumers are not left waiting for the connection to close.
      mark_complete();
    } else if (event_type == "error") {
      const auto message = json_event.value("error", nlohmann::json::object())
                               .value("message", "Anthropic stream error");
      note_stream_error();
      push_event(create_error_event(message));
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Failed to parse SSE event: {}", e.what());
  }
}

}  // namespace anthropic
}  // namespace ai
