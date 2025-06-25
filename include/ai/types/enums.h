#pragma once

namespace ai {

/// Role of a message in a conversation
enum MessageRole {
  kMessageRoleSystem,    ///< System instructions/context
  kMessageRoleUser,      ///< User input/question
  kMessageRoleAssistant  ///< AI assistant response
};

/// Reason why text generation finished
enum FinishReason {
  kFinishReasonStop,           ///< Natural completion (model decided to stop)
  kFinishReasonLength,         ///< Maximum token limit reached
  kFinishReasonContentFilter,  ///< Content policy violation detected
  kFinishReasonToolCalls,      ///< Model wants to call tools/functions
  kFinishReasonError           ///< Error occurred during generation
};

/// Type of streaming event
enum StreamEventType {
  kStreamEventTypeTextDelta,   ///< New text chunk received
  kStreamEventTypeToolCall,    ///< Tool/function call initiated
  kStreamEventTypeToolResult,  ///< Result from tool execution
  kStreamEventTypeStepStart,   ///< Step started (for multi-step generation)
  kStreamEventTypeStepFinish,  ///< Step finished
  kStreamEventTypeFinish,      ///< Stream completed successfully
  kStreamEventTypeError        ///< Error occurred in stream
};

}  // namespace ai