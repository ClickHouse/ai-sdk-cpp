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

// GPT-5.6 series (current frontier family)
constexpr const char* kGpt56 = "gpt-5.6";  // Alias for GPT-5.6 Sol
constexpr const char* kGpt56Sol = "gpt-5.6-sol";
constexpr const char* kGpt56Terra = "gpt-5.6-terra";
constexpr const char* kGpt56Luna = "gpt-5.6-luna";

// Earlier current GPT-5 series
constexpr const char* kGpt55 = "gpt-5.5";
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

/// Default model used when none is specified
constexpr const char* kDefaultModel = kGpt56;

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
