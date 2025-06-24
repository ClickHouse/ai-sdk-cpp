#pragma once

#include "types/client.h"

#include <string>

namespace ai {
namespace openai {

/// Create an OpenAI client with default configuration
/// Reads API key from OPENAI_API_KEY environment variable
/// @return Configured OpenAI client
Client create_client();

/// Create an OpenAI client with explicit API key
/// @param api_key OpenAI API key
/// @return Configured OpenAI client
Client create_client(const std::string& api_key);

/// Create an OpenAI client with custom configuration
/// @param api_key OpenAI API key
/// @param base_url Custom base URL (for OpenAI-compatible APIs)
/// @return Configured OpenAI client
Client create_client(const std::string& api_key, const std::string& base_url);

}  // namespace openai
}  // namespace ai