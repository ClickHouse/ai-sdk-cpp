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

}  // namespace anthropic
}  // namespace ai