#pragma once

/// Main convenience header for AI SDK C++
/// Include this header to get access to all public APIs

// Core API functions
#include "generate.h"
#include "stream.h"

// Provider-specific clients
#include "anthropic.h"
#include "openai.h"

// Type definitions
#include "types/client.h"
#include "types/enums.h"
#include "types/generate_options.h"
#include "types/message.h"
#include "types/model.h"
#include "types/stream_options.h"
#include "types/usage.h"

// Error handling
#include "errors.h"

/// AI SDK C++ - Modern C++ toolkit for AI-powered applications
///
/// Usage Examples:
///
/// Basic text generation:
/// ```cpp
/// #include <ai/ai.h>
///
/// auto result = ai::generate_text("gpt-4o", "Hello, world!");
/// if (result) {
///     std::cout << result.text << std::endl;
/// }
/// ```
///
/// Streaming text generation:
/// ```cpp
/// auto stream = ai::stream_text("gpt-4o", "Write a story");
/// for (const auto& event : stream) {
///     if (event.is_text_delta()) {
///         std::cout << event.text_delta << std::flush;
///     }
/// }
/// ```
///
/// Provider-specific clients:
/// ```cpp
/// auto client = ai::openai::create_client();
/// auto result = ai::generate_text(client.model("gpt-4o"), "Hello!");
/// ```
namespace ai {}
