#pragma once

#ifndef AI_SDK_HAS_OPENAI
#error \
    "OpenAI component not available. Link with ai::openai or ai::sdk to use OpenAI functionality."
#endif

#include "retry/retry_policy.h"
#include "types/client.h"

#include <optional>
#include <string>

namespace ai {
namespace openai {

namespace models {
/// Common OpenAI model identifiers

// O-series reasoning models
constexpr const char* kO1 = "o1";
constexpr const char* kO1Mini = "o1-mini";
constexpr const char* kO1Preview = "o1-preview";
constexpr const char* kO3 = "o3";
constexpr const char* kO3Mini = "o3-mini";
constexpr const char* kO4Mini = "o4-mini";

// GPT-4.1 series
constexpr const char* kGpt41 = "gpt-4.1";
constexpr const char* kGpt41Mini = "gpt-4.1-mini";
constexpr const char* kGpt41Nano = "gpt-4.1-nano";

// GPT-4o series
constexpr const char* kGpt4o = "gpt-4o";
constexpr const char* kGpt4oMini = "gpt-4o-mini";
constexpr const char* kGpt4oAudioPreview = "gpt-4o-audio-preview";

// GPT-4 series
constexpr const char* kGpt4Turbo = "gpt-4-turbo";
constexpr const char* kGpt4 = "gpt-4";

// GPT-3.5 series
constexpr const char* kGpt35Turbo = "gpt-3.5-turbo";

// Special models
constexpr const char* kChatGpt4oLatest = "chatgpt-4o-latest";

/// Default model used when none is specified
constexpr const char* kDefaultModel = kGpt4o;

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

/// Create an OpenAI client with custom configuration and retry settings
/// @param api_key OpenAI API key
/// @param base_url Custom base URL (for OpenAI-compatible APIs)
/// @param retry_config Custom retry configuration
/// @return Configured OpenAI client
Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config);

/// Try to create an OpenAI client using environment variables
/// Reads API key from OPENAI_API_KEY environment variable
/// @return Optional client - has value if environment variable is set, empty
/// otherwise
/// @note This is useful for chaining creation attempts with other providers
std::optional<Client> try_create_client();

}  // namespace openai
}  // namespace ai
