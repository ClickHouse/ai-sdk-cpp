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

struct GenerateOptions {
  std::string model;
  std::string system;
  std::string prompt;
  Messages messages;
  std::optional<nlohmann::json> response_format {};
  std::optional<int> max_tokens;
  std::optional<double> temperature;
  std::optional<double> top_p;
  std::optional<int> seed;
  std::optional<double> frequency_penalty;
  std::optional<double> presence_penalty;

  // Tool calling support
  ToolSet tools;
  ToolChoice tool_choice = ToolChoice::auto_choice();
  int max_steps = 1;
  std::vector<std::string> active_tools;

  // Callbacks for tool calling
  std::optional<std::function<void(const GenerateStep&)>> on_step_finish;
  std::optional<std::function<void(const ToolCall&)>> on_tool_call_start;
  std::optional<std::function<void(const ToolResult&)>> on_tool_call_finish;

  GenerateOptions(std::string model_name, std::string user_prompt)
      : model(std::move(model_name)), prompt(std::move(user_prompt)) {}

  GenerateOptions(std::string model_name,
                  std::string system_prompt,
                  std::string user_prompt)
      : model(std::move(model_name)),
        system(std::move(system_prompt)),
        prompt(std::move(user_prompt)) {}

  GenerateOptions(std::string model_name, Messages conversation)
      : model(std::move(model_name)), messages(std::move(conversation)) {}

  GenerateOptions() = default;

  bool is_valid() const {
    return !model.empty() && (!prompt.empty() || !messages.empty());
  }

  bool has_messages() const { return !messages.empty(); }

  bool has_tools() const { return !tools.empty(); }

  bool is_multi_step() const { return max_steps > 1; }

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

struct GenerateResult {
  std::string text;
  FinishReason finish_reason = kFinishReasonError;
  Usage usage;

  /// Additional metadata (like TypeScript SDK)
  std::optional<std::string> id;
  std::optional<std::string> model;
  std::optional<int64_t> created;
  std::optional<std::string> system_fingerprint;

  /// Error handling
  std::optional<std::string> error;
  std::vector<std::string> warnings;
  std::optional<bool> is_retryable;

  /// Provider-specific metadata
  std::optional<std::string> provider_metadata;

  /// Response messages for multi-turn (includes the assistant's response)
  Messages response_messages;

  // Tool calling results
  std::vector<ToolCall> tool_calls;
  std::vector<ToolResult> tool_results;
  std::vector<GenerateStep> steps;

  GenerateResult() = default;

  GenerateResult(std::string generated_text,
                 FinishReason reason,
                 Usage token_usage)
      : text(std::move(generated_text)),
        finish_reason(reason),
        usage(token_usage) {}

  explicit GenerateResult(std::string error_message)
      : error(std::move(error_message)) {}

  bool is_success() const {
    return !error.has_value() && finish_reason != kFinishReasonError;
  }

  explicit operator bool() const { return is_success(); }

  std::string error_message() const { return error.value_or(""); }

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

  bool has_tool_calls() const { return !tool_calls.empty(); }

  bool has_tool_results() const { return !tool_results.empty(); }

  bool is_multi_step() const { return !steps.empty(); }

  std::vector<ToolCall> get_all_tool_calls() const {
    std::vector<ToolCall> all_calls = tool_calls;
    for (const auto& step : steps) {
      all_calls.insert(all_calls.end(), step.tool_calls.begin(),
                       step.tool_calls.end());
    }
    return all_calls;
  }

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