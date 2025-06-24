#pragma once

#include "types/stream_options.h"

namespace ai {

/// Stream text generation using the specified model and options
/// @param options Configuration for streaming text generation
/// @return Stream result that can be iterated for real-time text chunks
StreamResult stream_text(const StreamOptions& options);

}  // namespace ai