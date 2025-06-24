#pragma once

#include "enums.h"

#include <optional>
#include <string>

namespace ai {

/// Event in a text generation stream
struct StreamEvent {
  /// Constructor for text delta events
  explicit StreamEvent(std::string text)
      : type(kStreamEventTypeTextDelta), text_delta(std::move(text)) {}

  /// Constructor for error events
  StreamEvent(StreamEventType event_type, std::string error_msg)
      : type(event_type), error(std::move(error_msg)) {}

  /// Constructor for other event types
  explicit StreamEvent(StreamEventType event_type) : type(event_type) {}

  /// Check if this is a text delta event
  bool isTextDelta() const { return type == kStreamEventTypeTextDelta; }

  /// Check if this is an error event
  bool isError() const { return type == kStreamEventTypeError; }

  /// Check if this is a finish event
  bool isFinish() const { return type == kStreamEventTypeFinish; }

  const StreamEventType type;    ///< Type of stream event
  const std::string text_delta;  ///< Text chunk (for TextDelta events)
  const std::optional<std::string> error;  ///< Error message (for Error events)
};

}  // namespace ai