#pragma once

#include "enums.h"
#include "tool.h"
#include "usage.h"

#include <optional>
#include <string>

namespace ai {

struct StreamEvent {
  explicit StreamEvent(std::string text)
      : type(kStreamEventTypeTextDelta), text_delta(std::move(text)) {}

  StreamEvent(StreamEventType event_type, std::string error_msg)
      : type(event_type), error(std::move(error_msg)) {}

  StreamEvent(StreamEventType event_type,
              Usage usage_stats,
              FinishReason reason)
      : type(event_type), usage(usage_stats), finish_reason(reason) {}

  StreamEvent(StreamEventType event_type, FinishReason reason)
      : type(event_type), finish_reason(reason) {}

  explicit StreamEvent(StreamEventType event_type) : type(event_type) {}

  explicit StreamEvent(ToolCall call)
      : type(kStreamEventTypeToolCall), tool_call(std::move(call)) {}

  bool is_text_delta() const { return type == kStreamEventTypeTextDelta; }

  bool is_error() const { return type == kStreamEventTypeError; }

  bool is_tool_call() const { return type == kStreamEventTypeToolCall; }

  bool is_finish() const { return type == kStreamEventTypeFinish; }

  StreamEventType type;
  std::string text_delta;
  std::optional<std::string> error;
  std::optional<Usage> usage;
  std::optional<FinishReason> finish_reason;
  std::optional<ToolCall> tool_call;
  std::optional<std::string> metadata;
};

}  // namespace ai
