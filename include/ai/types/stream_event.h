#pragma once

#include "enums.h"
#include "usage.h"

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

  /// Constructor for finish events with usage
  StreamEvent(StreamEventType event_type,
              Usage usage_stats,
              FinishReason reason)
      : type(event_type), usage(usage_stats), finish_reason(reason) {}

  /// Constructor for other event types
  explicit StreamEvent(StreamEventType event_type) : type(event_type) {}

  /// Check if this is a text delta event
  bool is_text_delta() const { return type == kStreamEventTypeTextDelta; }

  /// Check if this is an error event
  bool is_error() const { return type == kStreamEventTypeError; }

  /// Check if this is a finish event
  bool is_finish() const { return type == kStreamEventTypeFinish; }

  StreamEventType type;              ///< Type of stream event
  std::string text_delta;            ///< Text chunk (for TextDelta events)
  std::optional<std::string> error;  ///< Error message (for Error events)
  std::optional<Usage> usage;        ///< Usage statistics (for Finish events)
  std::optional<FinishReason>
      finish_reason;                    ///< Finish reason (for Finish events)
  std::optional<std::string> metadata;  ///< Additional metadata as JSON string
};

}  // namespace ai