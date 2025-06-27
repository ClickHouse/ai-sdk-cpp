#pragma once

/// Core AI SDK C++ header - Base functionality only
/// Use this when you only need core types and tools without specific providers

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

/// AI SDK C++ Core - Base functionality
/// 
/// This header provides access to core types and functionality
/// without pulling in specific provider implementations.
///
/// Usage:
/// ```cpp
/// #include <ai/core.h>
/// 
/// // Use core types like ai::GenerateOptions, ai::Message, etc.
/// ai::GenerateOptions options{
///     .model = "some-model",
///     .prompt = "Hello world"
/// };
/// ```
namespace ai {}