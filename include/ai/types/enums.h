#pragma once

namespace ai {

enum MessageRole {
  kMessageRoleSystem,
  kMessageRoleUser,
  kMessageRoleAssistant
};

enum FinishReason {
  kFinishReasonStop,
  kFinishReasonLength,
  kFinishReasonContentFilter,
  kFinishReasonToolCalls,
  kFinishReasonError
};

enum StreamEventType {
  kStreamEventTypeTextDelta,
  kStreamEventTypeToolCall,
  kStreamEventTypeToolResult,
  kStreamEventTypeStepStart,
  kStreamEventTypeStepFinish,
  kStreamEventTypeFinish,
  kStreamEventTypeError,
  /// A fragment of the model's reasoning (thinking) text, in `reasoning_delta`.
  /// It can be empty when the provider does not return the reasoning itself.
  kStreamEventTypeReasoningDelta
};

}  // namespace ai