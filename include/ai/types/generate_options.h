#pragma once

#include "enums.h"
#include "message.h"
#include "model.h"
#include "tool.h"
#include "usage.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ai {

/// Options for text generation
struct GenerateOptions {
  std::string model;   ///< Model identifier (e.g., "gpt-4o")
  std::string system;  ///< System prompt/instructions
  std::string prompt;  ///< User prompt/question
  Messages messages;   ///< Conversation history (alternative to prompt)
  std::optional<int> max_tokens;      ///< Maximum tokens to generate
  std::optional<double> temperature;  ///< Creativity level (0.0-2.0)
  std::optional<double> top_p;        ///< Nucleus sampling parameter (0.0-1.0)
  std::optional<int> seed;            ///< Seed for reproducible outputs
  std::optional<double> frequency_penalty;  ///< Reduce repetition (-2.0 to 2.0)
  std::optional<double>
      presence_penalty;  ///< Encourage new topics (-2.0 to 2.0)

  // Tool calling support
  ToolSet tools;  ///< Available tools for the model to call
  ToolChoice tool_choice =
      ToolChoice::auto_choice();  ///< Control tool selection behavior
  int max_steps = 1;  ///< Maximum tool calling steps (1 = no multi-step)
  std::vector<std::string>
      active_tools;  ///< Limit which tools are active (empty = all active)

  // Callbacks for tool calling
  std::optional<std::function<void(const GenerateStep&)>>
      on_step_finish;  ///< Called when each step completes

  /// Constructor with model and prompt
  GenerateOptions(std::string model_name, std::string user_prompt)
      : model(std::move(model_name)), prompt(std::move(user_prompt)) {}

  /// Constructor with model, system, and prompt
  GenerateOptions(std::string model_name,
                  std::string system_prompt,
                  std::string user_prompt)
      : model(std::move(model_name)),
        system(std::move(system_prompt)),
        prompt(std::move(user_prompt)) {}

  /// Constructor with model and messages
  GenerateOptions(std::string model_name, Messages conversation)
      : model(std::move(model_name)), messages(std::move(conversation)) {}

  /// Default constructor
  GenerateOptions() = default;

  /// Check if options are valid for API call
  bool is_valid() const {
    return !model.empty() && (!prompt.empty() || !messages.empty());
  }

  /// Check if using conversation messages instead of simple prompt
  bool has_messages() const { return !messages.empty(); }

  /// Check if tools are configured
  bool has_tools() const { return !tools.empty(); }

  /// Check if multi-step tool calling is enabled
  bool is_multi_step() const { return max_steps > 1; }

  /// Get active tool names (returns all if active_tools is empty)
  std::vector<std::string> get_active_tool_names() const {
    if (active_tools.empty()) {
      std::vector<std::string> all_names;
      for (const auto& [name, tool] : tools) {
        all_names.push_back(name);
      }
      return all_names;
    }
    return active_tools;
  }
};

/// Result from text generation
struct GenerateResult {
  std::string text;                                 ///< Generated text content
  FinishReason finish_reason = kFinishReasonError;  ///< Why generation stopped
  Usage usage;  ///< Token consumption statistics

  /// Additional metadata (like TypeScript SDK)
  std::optional<std::string> id;     ///< Unique identifier for the completion
  std::optional<std::string> model;  ///< Model used for generation
  std::optional<int64_t> created;    ///< Unix timestamp of creation
  std::optional<std::string>
      system_fingerprint;  ///< System configuration fingerprint

  /// Error handling
  std::optional<std::string> error;   ///< Error message if generation failed
  std::vector<std::string> warnings;  ///< Any warnings from the API

  /// Provider-specific metadata
  std::optional<std::string>
      provider_metadata;  ///< JSON string of provider metadata

  /// Response messages for multi-turn (includes the assistant's response)
  Messages response_messages;  ///< Full conversation including response

  // Tool calling results
  std::vector<ToolCall> tool_calls;      ///< Tool calls made by the model
  std::vector<ToolResult> tool_results;  ///< Results from tool execution
  std::vector<GenerateStep>
      steps;  ///< Multi-step breakdown (when max_steps > 1)

  /// Default constructor (indicates error state)
  GenerateResult() = default;

  /// Constructor for successful result
  GenerateResult(std::string generated_text,
                 FinishReason reason,
                 Usage token_usage)
      : text(std::move(generated_text)),
        finish_reason(reason),
        usage(token_usage) {}

  /// Constructor for error result
  explicit GenerateResult(std::string error_message)
      : error(std::move(error_message)) {}

  /// Check if generation was successful
  bool is_success() const {
    return !error.has_value() && finish_reason != kFinishReasonError;
  }

  /// Implicit bool conversion for easy checking
  explicit operator bool() const { return is_success(); }

  /// Get error message or empty string if successful
  std::string error_message() const { return error.value_or(""); }

  /// Get finish reason as string (for debugging/logging)
  std::string finishReasonToString() const {
    switch (finish_reason) {
      case kFinishReasonStop:
        return "stop";
      case kFinishReasonLength:
        return "length";
      case kFinishReasonContentFilter:
        return "content_filter";
      case kFinishReasonToolCalls:
        return "tool_calls";
      case kFinishReasonError:
        return "error";
      default:
        return "unknown";
    }
  }

  /// Check if result contains tool calls
  bool has_tool_calls() const { return !tool_calls.empty(); }

  /// Check if result contains tool results
  bool has_tool_results() const { return !tool_results.empty(); }

  /// Check if result used multi-step generation
  bool is_multi_step() const { return !steps.empty(); }

  /// Get total tool calls across all steps
  std::vector<ToolCall> get_all_tool_calls() const {
    std::vector<ToolCall> all_calls = tool_calls;
    for (const auto& step : steps) {
      all_calls.insert(all_calls.end(), step.tool_calls.begin(),
                       step.tool_calls.end());
    }
    return all_calls;
  }

  /// Get total tool results across all steps
  std::vector<ToolResult> get_all_tool_results() const {
    std::vector<ToolResult> all_results = tool_results;
    for (const auto& step : steps) {
      all_results.insert(all_results.end(), step.tool_results.begin(),
                         step.tool_results.end());
    }
    return all_results;
  }
};

}  // namespace ai