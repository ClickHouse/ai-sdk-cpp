#pragma once

/// Main convenience header for AI SDK C++
/// Include this header to get access to all public APIs

// Provider-specific clients (conditionally included)
#ifdef AI_SDK_HAS_OPENAI
#include "openai.h"
#endif

#ifdef AI_SDK_HAS_ANTHROPIC
#include "anthropic.h"
#endif

// Type definitions
#include "types/client.h"
#include "types/enums.h"
#include "types/generate_options.h"
#include "types/message.h"
#include "types/model.h"
#include "types/stream_event.h"
#include "types/stream_options.h"
#include "types/stream_result.h"
#include "types/tool.h"
#include "types/usage.h"

// Tool functionality
#include "tools.h"

// Error handling
#include "errors.h"

/// AI SDK C++ - Modern C++ toolkit for AI-powered applications
///
/// Usage Examples:
///
/// OpenAI Integration:
/// ```cpp
/// #include <ai/ai.h>
/// #include <iostream>
///
/// // Ensure OPENAI_API_KEY environment variable is set
/// auto client = ai::openai::create_client();
///
/// ai::GenerateOptions options(ai::openai::models::kGpt56,
///                             "Why is the sky blue?");
/// options.system = "You are a friendly assistant!";
/// auto result = client.generate_text(options);
///
/// if (result) {
///     std::cout << result->text << std::endl;
/// }
/// ```
///
/// Streaming text generation:
/// ```cpp
/// auto client = ai::openai::create_client();
///
/// ai::GenerateOptions options(ai::openai::models::kGpt56,
///                             "Write a short story about a robot.");
/// options.system = "You are a helpful assistant.";
/// auto stream = client.stream_text(ai::StreamOptions(options));
///
/// for (const auto& event : stream) {
///     if (event.is_text_delta()) {
///         std::cout << event.text_delta << std::flush;
///     }
/// }
/// ```
///
/// Anthropic Integration:
/// ```cpp
/// auto client = ai::anthropic::create_client();
/// ai::GenerateOptions options(ai::anthropic::models::kClaudeSonnet5,
///                             "Explain quantum computing in simple terms.");
/// options.system = "You are a helpful assistant.";
/// auto result = client.generate_text(options);
///
/// if (result) {
///     std::cout << result->text << std::endl;
/// }
/// ```
namespace ai {}
