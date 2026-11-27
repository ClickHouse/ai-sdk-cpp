#pragma once

#ifndef AI_SDK_HAS_ANTHROPIC
#error \
    "Anthropic component not available. Link with ai::anthropic or ai::sdk to use Anthropic functionality."
#endif

#include "retry/retry_policy.h"
#include "types/client.h"

#include <optional>
#include <string>

namespace ai {
namespace anthropic {

namespace models {
/// Current Anthropic model identifiers
///
/// Claude Fable 5.1 and later, Claude Opus 5.5 and Claude Sonnet 5.5 reject a
/// forced tool choice (`ToolChoice::required()` and `ToolChoice::specific()`)
/// with HTTP 400; use `ToolChoice::auto_choice()` and name the tool in the
/// prompt instead.
constexpr const char* kClaudeFable52 = "claude-fable-5-2";
constexpr const char* kClaudeOpus55 = "claude-opus-5-5";
constexpr const char* kClaudeSonnet55 = "claude-sonnet-5-5";
constexpr const char* kClaudeHaiku55 = "claude-haiku-5-5";

/// Previous-generation model identifiers (still available)
constexpr const char* kClaudeFable51 = "claude-fable-5-1";
constexpr const char* kClaudeFable5 = "claude-fable-5";
constexpr const char* kClaudeOpus5 = "claude-opus-5";
constexpr const char* kClaudeOpus48 = "claude-opus-4-8";
constexpr const char* kClaudeSonnet5 = "claude-sonnet-5";
constexpr const char* kClaudeOpus47 = "claude-opus-4-7";
constexpr const char* kClaudeSonnet46 = "claude-sonnet-4-6";
constexpr const char* kClaudeHaiku45 =
    "claude-haiku-4-5";  // claude-haiku-4-5-20251001
constexpr const char* kClaudeHaiku45Snapshot = "claude-haiku-4-5-20251001";

/// Legacy model identifiers (still available; consider migrating)
constexpr const char* kClaudeOpus46 = "claude-opus-4-6";
constexpr const char* kClaudeOpus45 =
    "claude-opus-4-5";  // claude-opus-4-5-20251101
constexpr const char* kClaudeOpus45Snapshot = "claude-opus-4-5-20251101";
constexpr const char* kClaudeSonnet45 =
    "claude-sonnet-4-5";  // claude-sonnet-4-5-20250929
constexpr const char* kClaudeSonnet45Snapshot = "claude-sonnet-4-5-20250929";
constexpr const char* kClaudeOpus41 =
    "claude-opus-4-1";  // claude-opus-4-1-20250805
constexpr const char* kClaudeOpus41Snapshot = "claude-opus-4-1-20250805";

/// Default model used when none is specified
constexpr const char* kDefaultModel = kClaudeSonnet55;
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

/// Create an Anthropic client with custom configuration and retry settings
/// @param api_key Anthropic API key
/// @param base_url Custom base URL (for Anthropic-compatible APIs)
/// @param retry_config Custom retry configuration
/// @return Configured Anthropic client
Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config);

/// Try to create an Anthropic client using environment variables
/// Reads API key from ANTHROPIC_API_KEY environment variable
/// @return Optional client - has value if environment variable is set, empty
/// otherwise
/// @note This is useful for chaining creation attempts with other providers
std::optional<Client> try_create_client();

}  // namespace anthropic
}  // namespace ai
