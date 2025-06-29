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
  kStreamEventTypeError
};

}  // namespace ai