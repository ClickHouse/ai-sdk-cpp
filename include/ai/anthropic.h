#pragma once

#include "types/client.h"

#include <string>

namespace ai {
namespace anthropic {

namespace models {
/// Common Anthropic model identifiers
constexpr const char* kClaude35Sonnet = "claude-3-5-sonnet-20241022";
constexpr const char* kClaude35Haiku = "claude-3-5-haiku-20241022";
constexpr const char* kClaude3Opus = "claude-3-opus-20240229";
constexpr const char* kClaude3Sonnet = "claude-3-sonnet-20240229";
constexpr const char* kClaude3Haiku = "claude-3-haiku-20240307";
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

}  // namespace anthropic
}  // namespace ai