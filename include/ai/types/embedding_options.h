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

struct EmbeddingOptions {
  std::string model;
  nlohmann::json input;
  std::optional<int> dimensions;
  std::optional<std::string> encoding_format;
  std::optional<std::string> user;  // Optional user identifier for OpenAI

  EmbeddingOptions() = default;

  EmbeddingOptions(std::string model_name, nlohmann::json input_)
      : model(std::move(model_name)),
        input(std::move(input_)) {}

  EmbeddingOptions(std::string model_name, nlohmann::json input_, int dimensions_)
      : model(std::move(model_name)),
        input(std::move(input_)),
        dimensions(dimensions_) {}

  EmbeddingOptions(std::string model_name, nlohmann::json input_, int dimensions_, std::string encoding_format_)
      : model(std::move(model_name)),
        input(std::move(input_)),
        dimensions(dimensions_),
        encoding_format(std::move(encoding_format_)) {}

  bool is_valid() const {
    return !model.empty() && !input.is_null();
  }

  bool has_input() const { return !input.is_null(); }
};

struct EmbeddingResult {
  nlohmann::json data;
  Usage usage;

  /// Additional metadata (like TypeScript SDK)
  std::optional<std::string> model;

  /// Error handling
  std::optional<std::string> error;
  std::optional<bool> is_retryable;

  /// Provider-specific metadata
  std::optional<std::string> provider_metadata;

  EmbeddingResult() = default;

 // EmbeddingResult(std::string data_, Usage token_usage)
 //     : data(std::move(data_)), usage(token_usage) {}

  explicit EmbeddingResult(std::optional<std::string> error_message)
      : error(std::move(error_message)) {}

  bool is_success() const {
    return !error.has_value();
  }

  explicit operator bool() const { return is_success(); }

  std::string error_message() const { return error.value_or(""); }
};

}  // namespace ai
