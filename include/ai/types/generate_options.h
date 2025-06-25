#pragma once

#include "enums.h"
#include "message.h"
#include "model.h"
#include "usage.h"

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
  bool is_success() const { return !error.has_value(); }

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
};

}  // namespace ai