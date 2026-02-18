#pragma once

#include "ai/bedrock.h"  // For SecurityConfig
#include "ai/types/generate_options.h"
#include "ai/types/message.h"

#include <chrono>
#include <optional>
#include <regex>
#include <string>

namespace ai {
namespace bedrock {

/// Validates all input parameters before making API calls
class InputValidator {
 public:
  /// Create validator with default security config
  InputValidator();

  /// Create validator with custom security config
  explicit InputValidator(const SecurityConfig& config);

  /// Validate GenerateOptions, returns error message if invalid
  /// @param options The options to validate
  /// @return std::nullopt if valid, error message string if invalid
  std::optional<std::string> validate(const GenerateOptions& options) const;

  /// Validate model ID format
  /// Accepts: provider.model-name-version patterns and ARN formats
  /// @param model_id The model identifier to validate
  /// @return true if valid format
  bool is_valid_model_id(const std::string& model_id) const;

  /// Validate temperature range [0.0, 1.0]
  /// @param temperature The temperature value
  /// @return true if within valid range
  bool is_valid_temperature(double temperature) const;

  /// Validate top_p range [0.0, 1.0]
  /// @param top_p The top_p value
  /// @return true if within valid range
  bool is_valid_top_p(double top_p) const;

  /// Validate max_tokens is positive and within bounds
  /// @param max_tokens The max_tokens value
  /// @return true if valid
  bool is_valid_max_tokens(int max_tokens) const;

  /// Validate prompt length against configured maximum
  /// @param prompt The prompt string
  /// @return true if within length limit
  bool is_valid_prompt_length(const std::string& prompt) const;

  /// Validate total message content length
  /// @param messages The messages to validate
  /// @return true if total content within limit
  bool is_valid_message_length(const Messages& messages) const;

  /// Get the current security configuration
  const SecurityConfig& config() const { return config_; }

 private:
  SecurityConfig config_;

  /// Calculate total content length across all messages
  size_t calculate_message_length(const Messages& messages) const;
};

}  // namespace bedrock
}  // namespace ai
