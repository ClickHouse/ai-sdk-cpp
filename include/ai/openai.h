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

// GPT-6 series (current frontier family). These models reject sampling
// parameters, and OpenAI serves their tool calling through the Responses API
// only: use them with `create_responses_client`. On Chat Completions
// (`create_client`) they generate text but cannot call tools.
constexpr const char* kGpt62Astra = "gpt-6.2-astra";
constexpr const char* kGpt62Sol = "gpt-6.2-sol";
constexpr const char* kGpt61Terra = "gpt-6.1-terra";
constexpr const char* kGpt6Luna = "gpt-6-luna";

// GPT-5.6 series (latest family with tool calling on Chat Completions)
constexpr const char* kGpt56 = "gpt-5.6";  // Alias for GPT-5.6 Sol
constexpr const char* kGpt56Sol = "gpt-5.6-sol";
constexpr const char* kGpt56Terra = "gpt-5.6-terra";
constexpr const char* kGpt56Luna = "gpt-5.6-luna";

// Earlier GPT-5 series
constexpr const char* kGpt55 = "gpt-5.5";
constexpr const char* kGpt54 = "gpt-5.4";
constexpr const char* kGpt54Pro = "gpt-5.4-pro";
constexpr const char* kGpt54Mini = "gpt-5.4-mini";
constexpr const char* kGpt54Nano = "gpt-5.4-nano";  // Shutdown: 2027-04-01

// GPT-5 small variants (deprecated, shutdown: 2026-12-11; use kGpt61Terra
// and kGpt6Luna)
constexpr const char* kGpt5Mini = "gpt-5-mini";
constexpr const char* kGpt5Nano = "gpt-5-nano";

// GPT-4.1 series (deprecated, shutdown: 2027-01-14; use kGpt6Luna)
constexpr const char* kGpt41 = "gpt-4.1";
constexpr const char* kGpt41Mini = "gpt-4.1-mini";

// Embeddings
constexpr const char* kTextEmbedding3Small = "text-embedding-3-small";
constexpr const char* kTextEmbedding3Large = "text-embedding-3-large";

/// Default model used when none is specified. It stays on GPT-5.6 because the
/// GPT-6 series does not support tool calling on Chat Completions.
constexpr const char* kDefaultModel = kGpt56;

/// Default model of the Responses API clients (`create_responses_client`).
constexpr const char* kDefaultResponsesModel = kGpt62Sol;

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

/// The `create_responses_client` functions create OpenAI clients that use the
/// Responses API (`POST /v1/responses`) instead of Chat Completions. It is
/// required for tool calling with the GPT-6 models. The `create_client`
/// functions keep using Chat Completions, which is what OpenAI-compatible
/// providers (OpenRouter and others) implement; neither kind of client falls
/// back to the other API.
///
/// Requests are stateless (`store: false`): nothing is stored on OpenAI's
/// side, and the conversation is replayed on every request. The `reasoning`
/// items of a reply are returned in `GenerateResult::response_messages` as
/// `ProviderContentPart`s, and replayed when those messages are sent back,
/// which the multi-step tool loop (`max_steps > 1`) does automatically.
/// `frequency_penalty`, `presence_penalty`, and `seed` are not supported by
/// the Responses API and are ignored with a warning.

/// Create an OpenAI Responses API client
/// Reads API key from OPENAI_API_KEY environment variable
/// @return Configured OpenAI Responses API client
Client create_responses_client();

/// Create an OpenAI Responses API client with explicit API key
/// @param api_key OpenAI API key
/// @return Configured OpenAI Responses API client
Client create_responses_client(const std::string& api_key);

/// Create an OpenAI Responses API client with a custom base URL
/// @param api_key OpenAI API key
/// @param base_url Custom base URL (for Responses-compatible APIs)
/// @return Configured OpenAI Responses API client
Client create_responses_client(const std::string& api_key,
                               const std::string& base_url);

/// Create an OpenAI Responses API client with custom retry settings
/// @param api_key OpenAI API key
/// @param base_url Custom base URL (for Responses-compatible APIs)
/// @param retry_config Custom retry configuration
/// @return Configured OpenAI Responses API client
Client create_responses_client(const std::string& api_key,
                               const std::string& base_url,
                               const retry::RetryConfig& retry_config);

/// Try to create an OpenAI Responses API client using environment variables
/// Reads API key from OPENAI_API_KEY environment variable
/// @return Optional client - has value if environment variable is set, empty
/// otherwise
std::optional<Client> try_create_responses_client();

}  // namespace openai
}  // namespace ai
