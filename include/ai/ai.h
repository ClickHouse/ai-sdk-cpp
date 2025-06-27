#pragma once

/// Main convenience header for AI SDK C++
/// Include this header to get access to all public APIs

// Provider-specific clients
#include "anthropic.h"
#include "openai.h"

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
/// auto result = client.generate_text({
///     .model = ai::openai::models::kGpt4o,
///     .system = "You are a friendly assistant!",
///     .prompt = "Why is the sky blue?"
/// });
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
/// auto stream = client.stream_text({
///     .model = ai::openai::models::kGpt4o,
///     .system = "You are a helpful assistant.",
///     .prompt = "Write a short story about a robot."
/// });
///
/// for (const auto& chunk : stream) {
///     if (chunk.text) {
///         std::cout << chunk.text.value() << std::flush;
///     }
/// }
/// ```
///
/// Anthropic Integration:
/// ```cpp
/// auto client = ai::anthropic::create_client();
/// auto result = client.generate_text({
///     .model = ai::anthropic::models::kClaude35Sonnet,
///     .system = "You are a helpful assistant.",
///     .prompt = "Explain quantum computing in simple terms."
/// });
///
/// if (result) {
///     std::cout << result->text << std::endl;
/// }
/// ```
namespace ai {}
