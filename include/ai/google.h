#pragma once

#ifndef AI_SDK_HAS_GOOGLE
#error \
    "Google component not available. Link with ai::google or ai::sdk to use Google Gemini functionality."
#endif

#include "retry/retry_policy.h"
#include "types/client.h"

#include <optional>
#include <string>

namespace ai {
namespace google {

namespace models {
/// Gemini 4 model identifiers
///
/// Gemini 4 and Gemini 3.6+ models ignore `temperature` and `top_p` (and warn
/// about them), so the client does not send them for those models.
///
/// Thinking tokens count against `max_tokens` (`maxOutputTokens`). Like the
/// OpenAI client with its lowest reasoning effort, the client asks the models
/// listed here (and Gemini 2.5) for the least thinking they accept:
/// `thinkingLevel` `low` on Gemini 4 and Gemini 3.8 Flash, `minimal` on Gemini
/// 3.6 Flash and the Flash-Lite models, and the smallest `thinkingBudget` on
/// Gemini 2.5.
constexpr const char* kGemini4Carbon = "gemini-4-carbon";
constexpr const char* kGemini4Neon = "gemini-4-neon";
constexpr const char* kGemini4Hydrogen = "gemini-4-hydrogen";
constexpr const char* kGemini4Argon = "gemini-4-argon";

/// Gemini 3 model identifiers (stable)
constexpr const char* kGemini38Flash = "gemini-3.8-flash";
constexpr const char* kGemini36Flash = "gemini-3.6-flash";
constexpr const char* kGemini35FlashLite = "gemini-3.5-flash-lite";
constexpr const char* kGemini31FlashLite = "gemini-3.1-flash-lite";

/// Default model used when none is specified (Google's recommended
/// general-purpose model)
constexpr const char* kDefaultModel = kGemini4Hydrogen;
}  // namespace models

/// Create a Google Gemini client with default configuration
/// Reads the API key from the GOOGLE_API_KEY or GEMINI_API_KEY environment
/// variable (GOOGLE_API_KEY takes precedence when both are set, like in the
/// official Google Gen AI SDKs)
/// @return Configured Gemini client
Client create_client();

/// Create a Google Gemini client with explicit API key
/// @param api_key Gemini API key
/// @return Configured Gemini client
Client create_client(const std::string& api_key);

/// Create a Google Gemini client with custom configuration
/// @param api_key Gemini API key
/// @param base_url Custom base URL (defaults to
/// https://generativelanguage.googleapis.com); requests go to
/// `{base_url}/v1beta/models/{model}:generateContent`
/// @return Configured Gemini client
Client create_client(const std::string& api_key, const std::string& base_url);

/// Create a Google Gemini client with custom configuration and retry settings
/// @param api_key Gemini API key
/// @param base_url Custom base URL
/// @param retry_config Custom retry configuration
/// @return Configured Gemini client
Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config);

/// Try to create a Google Gemini client using environment variables
/// Reads the API key from GOOGLE_API_KEY or GEMINI_API_KEY
/// @return Optional client - has value if an environment variable is set,
/// empty otherwise
/// @note This is useful for chaining creation attempts with other providers
std::optional<Client> try_create_client();

}  // namespace google
}  // namespace ai
