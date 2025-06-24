#pragma once

#include "types/client.h"

#include <string>

namespace ai {
namespace anthropic {

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
/// @param base_url Custom base URL (for testing or proxies)
/// @return Configured Anthropic client
Client create_client(const std::string& api_key, const std::string& base_url);

/// Commonly used Anthropic model identifiers
namespace models {
// Claude-4 models
inline constexpr const char* kClaude4Opus = "claude-4-opus-20250514";
inline constexpr const char* kClaude4Sonnet = "claude-4-sonnet-20250514";

// Claude-3.7 models
inline constexpr const char* kClaude37Sonnet = "claude-3-7-sonnet-20250219";

// Claude-3.5 models
inline constexpr const char* kClaude35SonnetLatest = "claude-3-5-sonnet-latest";
inline constexpr const char* kClaude35Sonnet20241022 =
    "claude-3-5-sonnet-20241022";
inline constexpr const char* kClaude35Sonnet20240620 =
    "claude-3-5-sonnet-20240620";
inline constexpr const char* kClaude35HaikuLatest = "claude-3-5-haiku-latest";
inline constexpr const char* kClaude35Haiku20241022 =
    "claude-3-5-haiku-20241022";

// Claude-3 models
inline constexpr const char* kClaude3OpusLatest = "claude-3-opus-latest";
inline constexpr const char* kClaude3Opus = "claude-3-opus-20240229";
inline constexpr const char* kClaude3Sonnet = "claude-3-sonnet-20240229";
inline constexpr const char* kClaude3Haiku = "claude-3-haiku-20240307";

// Backward compatibility aliases
inline constexpr const char* kClaude35Sonnet = kClaude35Sonnet20241022;
inline constexpr const char* kClaude35Haiku = kClaude35Haiku20241022;
}  // namespace models

}  // namespace anthropic
}  // namespace ai