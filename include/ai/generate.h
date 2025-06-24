#pragma once

#include "types/generate_options.h"

namespace ai {

/// Generate text using the specified model and options
/// @param options Configuration for text generation
/// @return Result containing generated text or error information
GenerateResult generate_text(const GenerateOptions& options);

}  // namespace ai