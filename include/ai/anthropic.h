#pragma once

#ifndef AI_SDK_HAS_ANTHROPIC
#error \
    "Anthropic component not available. Link with ai::anthropic or ai::sdk to use Anthropic functionality."
#endif

#include "types/client.h"

#include <optional>
#include <string>

namespace ai {
namespace anthropic {

namespace models {
/// Common Anthropic model identifiers (Latest models)
constexpr const char* kClaudeSonnet45 =
    "claude-sonnet-4-5";  // claude-sonnet-4-5-20250929
constexpr const char* kClaudeHaiku45 =
    "claude-haiku-4-5";  // claude-haiku-4-5-20251001
constexpr const char* kClaudeOpus41 =
    "claude-opus-4-1";  // claude-opus-4-1-20250805

/// Legacy model identifiers (retained for backward compatibility)
constexpr const char* kClaudeOpus4 =
    "claude-opus-4-0";  // claude-opus-4-20250514
constexpr const char* kClaudeSonnet4 =
    "claude-sonnet-4-0";  // claude-sonnet-4-20250514
constexpr const char* kClaudeSonnet37 =
    "claude-3-7-sonnet-latest";  // claude-3-7-sonnet-20250219
constexpr const char* kClaudeSonnet35 =
    "claude-3-5-sonnet-latest";  // claude-3-5-sonnet-20241022 (DEPRECATED)
constexpr const char* kClaudeHaiku35 =
    "claude-3-5-haiku-latest";  // claude-3-5-haiku-20241022

/// Default model used when none is specified
constexpr const char* kDefaultModel = kClaudeSonnet45;
}  // namespace models

/// Create an Anthropic client with default configuration
/// Reads API key from ANTHROPIC_API_KEY environment variable
/// @return Configured Anthropic client
Client create_client();

/// Create an Anthropic client with explicit API key
/// @param api_key Anthropic API key
/// @return Configured Anthropic client
Client create_client(const std::string& api_key);

/// Create an Anthropic client with custom configuration
/// @param api_key Anthropic API key
/// @param base_url Custom base URL (for Anthropic-compatible APIs)
/// @return Configured Anthropic client
Client create_client(const std::string& api_key, const std::string& base_url);

/// Try to create an Anthropic client using environment variables
/// Reads API key from ANTHROPIC_API_KEY environment variable
/// @return Optional client - has value if environment variable is set, empty
/// otherwise
/// @note This is useful for chaining creation attempts with other providers
std::optional<Client> try_create_client();

}  // namespace anthropic
}  // namespace ai