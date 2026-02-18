#include "input_validator.h"

#include <regex>
#include <sstream>

namespace ai {
namespace bedrock {

namespace {
// Model ID patterns for validation
// Standard Bedrock model ID: provider.model-name-version
// Examples: anthropic.claude-3-5-sonnet-20241022-v2:0, amazon.titan-text-express-v1
const std::regex kStandardModelIdPattern(
    R"(^[a-z0-9-]+\.[a-z0-9-]+(-[a-z0-9]+)*(-v[0-9]+)?(:[0-9]+)?$)",
    std::regex::icase);

// Cross-region inference profile format: region.provider.model-name-version
// Examples: us.anthropic.claude-sonnet-4-5-20250929-v1:0, eu.anthropic.claude-3-5-sonnet-v1:0
const std::regex kInferenceProfilePattern(
    R"(^[a-z]{2}\.[a-z0-9-]+\.[a-z0-9-]+(-[a-z0-9]+)*(-v[0-9]+)?(:[0-9]+)?$)",
    std::regex::icase);

// ARN format for custom/fine-tuned models
// Example: arn:aws:bedrock:us-east-1:123456789012:custom-model/my-model
const std::regex kArnModelIdPattern(
    R"(^arn:aws:bedrock:[a-z0-9-]+:[0-9]+:(custom-model|provisioned-model|foundation-model)/[a-zA-Z0-9-_/]+$)");

// Provisioned throughput ARN format
const std::regex kProvisionedArnPattern(
    R"(^arn:aws:bedrock:[a-z0-9-]+:[0-9]+:provisioned-model-throughput/[a-zA-Z0-9-_]+$)");

// Inference profile ARN format
// Example: arn:aws:bedrock:us-east-1:123456789012:inference-profile/us.anthropic.claude-3-5-sonnet-v1:0
const std::regex kInferenceProfileArnPattern(
    R"(^arn:aws:bedrock:[a-z0-9-]+:[0-9]+:inference-profile/[a-zA-Z0-9-_.:/]+$)");

}  // namespace

InputValidator::InputValidator() : config_() {}

InputValidator::InputValidator(const SecurityConfig& config) : config_(config) {}

std::optional<std::string> InputValidator::validate(
    const GenerateOptions& options) const {
  // Validate model ID
  if (options.model.empty()) {
    return "Model ID is required";
  }
  if (!is_valid_model_id(options.model)) {
    return "Invalid model ID format: " + options.model;
  }

  // Validate temperature if set
  if (options.temperature.has_value()) {
    if (!is_valid_temperature(options.temperature.value())) {
      return "Temperature must be between 0.0 and 1.0, got: " +
             std::to_string(options.temperature.value());
    }
  }

  // Validate top_p if set
  if (options.top_p.has_value()) {
    if (!is_valid_top_p(options.top_p.value())) {
      return "Top_p must be between 0.0 and 1.0, got: " +
             std::to_string(options.top_p.value());
    }
  }

  // Validate max_tokens if set
  if (options.max_tokens.has_value()) {
    if (!is_valid_max_tokens(options.max_tokens.value())) {
      return "Max tokens must be positive and <= " +
             std::to_string(config_.max_tokens_limit) +
             ", got: " + std::to_string(options.max_tokens.value());
    }
  }

  // Validate prompt length
  if (!options.prompt.empty()) {
    if (!is_valid_prompt_length(options.prompt)) {
      return "Prompt exceeds maximum length of " +
             std::to_string(config_.max_prompt_length) + " characters";
    }
  }

  // Validate message content length
  if (!options.messages.empty()) {
    if (!is_valid_message_length(options.messages)) {
      return "Total message content exceeds maximum length of " +
             std::to_string(config_.max_message_length) + " characters";
    }
  }

  // Validate system prompt length
  if (!options.system.empty()) {
    if (options.system.length() > config_.max_prompt_length) {
      return "System prompt exceeds maximum length of " +
             std::to_string(config_.max_prompt_length) + " characters";
    }
  }

  // Must have either prompt or messages
  if (options.prompt.empty() && options.messages.empty()) {
    return "Either prompt or messages must be provided";
  }

  return std::nullopt;  // Valid
}

bool InputValidator::is_valid_model_id(const std::string& model_id) const {
  if (model_id.empty()) {
    return false;
  }

  // Check standard Bedrock model ID format
  if (std::regex_match(model_id, kStandardModelIdPattern)) {
    return true;
  }

  // Check cross-region inference profile format (e.g., us.anthropic.claude-...)
  if (std::regex_match(model_id, kInferenceProfilePattern)) {
    return true;
  }

  // Check ARN format for custom models
  if (std::regex_match(model_id, kArnModelIdPattern)) {
    return true;
  }

  // Check provisioned throughput ARN format
  if (std::regex_match(model_id, kProvisionedArnPattern)) {
    return true;
  }

  // Check inference profile ARN format
  if (std::regex_match(model_id, kInferenceProfileArnPattern)) {
    return true;
  }

  return false;
}

bool InputValidator::is_valid_temperature(double temperature) const {
  return temperature >= 0.0 && temperature <= 1.0;
}

bool InputValidator::is_valid_top_p(double top_p) const {
  return top_p >= 0.0 && top_p <= 1.0;
}

bool InputValidator::is_valid_max_tokens(int max_tokens) const {
  return max_tokens > 0 && max_tokens <= config_.max_tokens_limit;
}

bool InputValidator::is_valid_prompt_length(const std::string& prompt) const {
  return prompt.length() <= config_.max_prompt_length;
}

bool InputValidator::is_valid_message_length(const Messages& messages) const {
  return calculate_message_length(messages) <= config_.max_message_length;
}

size_t InputValidator::calculate_message_length(
    const Messages& messages) const {
  size_t total_length = 0;

  for (const auto& message : messages) {
    for (const auto& part : message.content) {
      if (const auto* text_part = std::get_if<TextContentPart>(&part)) {
        total_length += text_part->text.length();
      }
    }
  }

  return total_length;
}

}  // namespace bedrock
}  // namespace ai
