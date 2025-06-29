#pragma once

#include "generate_options.h"

#include <functional>
#include <string>

namespace ai {

struct StreamOptions : public GenerateOptions {
  explicit StreamOptions(
      GenerateOptions options,
      std::function<void(const std::string&)> text_callback = nullptr,
      std::function<void(const GenerateResult&)> complete_callback = nullptr,
      std::function<void(const std::string&)> error_callback = nullptr)
      : GenerateOptions(std::move(options)),
        on_text_chunk(std::move(text_callback)),
        on_complete(std::move(complete_callback)),
        on_error(std::move(error_callback)) {}

  StreamOptions() = default;

  const std::function<void(const std::string&)> on_text_chunk;

  const std::function<void(const GenerateResult&)> on_complete;

  const std::function<void(const std::string&)> on_error;
};

}  // namespace ai