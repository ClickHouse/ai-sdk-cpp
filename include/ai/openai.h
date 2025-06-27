#pragma once

#ifndef AI_SDK_HAS_OPENAI
#error "OpenAI component not available. Link with ai::openai or ai::sdk to use OpenAI functionality."
#endif

#include "types/client.h"

#include <string>

namespace ai {
namespace openai {

namespace models {
/// Common OpenAI model identifiers
constexpr const char* kGpt4o = "gpt-4o";
constexpr const char* kGpt4oMini = "gpt-4o-mini";
constexpr const char* kGpt4Turbo = "gpt-4-turbo";
constexpr const char* kGpt35Turbo = "gpt-3.5-turbo";
constexpr const char* kGpt4 = "gpt-4";
}  // namespace models

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