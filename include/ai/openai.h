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

// GPT-5.4 series (current general-purpose)
constexpr const char* kGpt54 = "gpt-5.4";
constexpr const char* kGpt54Pro = "gpt-5.4-pro";
constexpr const char* kGpt54Mini = "gpt-5.4-mini";
constexpr const char* kGpt54Nano = "gpt-5.4-nano";

// GPT-5 small variants (current)
constexpr const char* kGpt5Mini = "gpt-5-mini";
constexpr const char* kGpt5Nano = "gpt-5-nano";

// GPT-4.1 series (still current non-reasoning leaders)
constexpr const char* kGpt41 = "gpt-4.1";
constexpr const char* kGpt41Mini = "gpt-4.1-mini";

// Embeddings
constexpr const char* kTextEmbedding3Small = "text-embedding-3-small";
constexpr const char* kTextEmbedding3Large = "text-embedding-3-large";

/// Legacy / deprecated model identifiers (retained for backward compatibility).
/// These are scheduled for retirement; prefer the GPT-5 series above.

// Deprecated GPT-5 snapshots (superseded by 5.4)
constexpr const char* kGpt5 = "gpt-5";     // DEPRECATED
constexpr const char* kGpt51 = "gpt-5.1";  // DEPRECATED
constexpr const char* kGpt52 = "gpt-5.2";  // DEPRECATED

// Deprecated GPT-4.1 nano
constexpr const char* kGpt41Nano = "gpt-4.1-nano";  // DEPRECATED

// Deprecated GPT-4o family (retired in ChatGPT; deprecated in API)
constexpr const char* kGpt4o = "gpt-4o";           // DEPRECATED
constexpr const char* kGpt4oMini = "gpt-4o-mini";  // DEPRECATED
constexpr const char* kGpt4oAudioPreview =
    "gpt-4o-audio-preview";                                    // DEPRECATED
constexpr const char* kChatGpt4oLatest = "chatgpt-4o-latest";  // DEPRECATED

// Deprecated GPT-4 series (shutdown 2026-10-23)
constexpr const char* kGpt4Turbo = "gpt-4-turbo";  // DEPRECATED
constexpr const char* kGpt4 = "gpt-4";             // DEPRECATED

// Deprecated GPT-3.5 series (shutdown 2026-10-23)
constexpr const char* kGpt35Turbo = "gpt-3.5-turbo";  // DEPRECATED

// Deprecated o-series reasoning models (shutdown 2026-10-23)
constexpr const char* kO1 = "o1";                 // DEPRECATED
constexpr const char* kO1Mini = "o1-mini";        // DEPRECATED
constexpr const char* kO1Preview = "o1-preview";  // DEPRECATED
constexpr const char* kO3 = "o3";                 // DEPRECATED
constexpr const char* kO3Mini = "o3-mini";        // DEPRECATED
constexpr const char* kO4Mini = "o4-mini";        // DEPRECATED

/// Default model used when none is specified
constexpr const char* kDefaultModel = kGpt54;

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
