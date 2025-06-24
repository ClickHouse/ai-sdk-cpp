#pragma once

#include "generate_options.h"

#include <functional>
#include <string>

namespace ai {

/// Options for streaming text generation (inherits from GenerateOptions)
struct StreamOptions : public GenerateOptions {
  /// Constructor from GenerateOptions with optional callbacks
  explicit StreamOptions(
      GenerateOptions options,
      std::function<void(const std::string&)> text_callback = nullptr,
      std::function<void(const GenerateResult&)> complete_callback = nullptr,
      std::function<void(const std::string&)> error_callback = nullptr)
      : GenerateOptions(std::move(options)),
        on_text_chunk(std::move(text_callback)),
        on_complete(std::move(complete_callback)),
        on_error(std::move(error_callback)) {}

  /// Default constructor
  StreamOptions() = default;

  /// Callback for handling streaming text chunks
  const std::function<void(const std::string&)> on_text_chunk;

  /// Callback for handling stream completion
  const std::function<void(const GenerateResult&)> on_complete;

  /// Callback for handling errors during streaming
  const std::function<void(const std::string&)> on_error;
};

}  // namespace ai