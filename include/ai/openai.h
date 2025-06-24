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

/// Commonly used OpenAI model identifiers
namespace models {
// O-series models (latest generation)
inline constexpr const char* kO1 = "o1";
inline constexpr const char* kO1_20241217 = "o1-2024-12-17";
inline constexpr const char* kO1Mini = "o1-mini";
inline constexpr const char* kO1Mini20240912 = "o1-mini-2024-09-12";
inline constexpr const char* kO1Preview = "o1-preview";
inline constexpr const char* kO1Preview20240912 = "o1-preview-2024-09-12";
inline constexpr const char* kO3Mini = "o3-mini";
inline constexpr const char* kO3Mini20250131 = "o3-mini-2025-01-31";
inline constexpr const char* kO3 = "o3";
inline constexpr const char* kO3_20250416 = "o3-2025-04-16";
inline constexpr const char* kO4Mini = "o4-mini";
inline constexpr const char* kO4Mini20250416 = "o4-mini-2025-04-16";

// GPT-4.1 series
inline constexpr const char* kGpt41 = "gpt-4.1";
inline constexpr const char* kGpt41_20250414 = "gpt-4.1-2025-04-14";
inline constexpr const char* kGpt41Mini = "gpt-4.1-mini";
inline constexpr const char* kGpt41Mini20250414 = "gpt-4.1-mini-2025-04-14";
inline constexpr const char* kGpt41Nano = "gpt-4.1-nano";
inline constexpr const char* kGpt41Nano20250414 = "gpt-4.1-nano-2025-04-14";

// GPT-4.5 series
inline constexpr const char* kGpt45Preview = "gpt-4.5-preview";
inline constexpr const char* kGpt45Preview20250227 =
    "gpt-4.5-preview-2025-02-27";

// GPT-4o series
inline constexpr const char* kGpt4o = "gpt-4o";
inline constexpr const char* kGpt4o20240513 = "gpt-4o-2024-05-13";
inline constexpr const char* kGpt4o20240806 = "gpt-4o-2024-08-06";
inline constexpr const char* kGpt4o20241120 = "gpt-4o-2024-11-20";
inline constexpr const char* kGpt4oAudioPreview = "gpt-4o-audio-preview";
inline constexpr const char* kGpt4oAudioPreview20241001 =
    "gpt-4o-audio-preview-2024-10-01";
inline constexpr const char* kGpt4oAudioPreview20241217 =
    "gpt-4o-audio-preview-2024-12-17";
inline constexpr const char* kGpt4oSearchPreview = "gpt-4o-search-preview";
inline constexpr const char* kGpt4oSearchPreview20250311 =
    "gpt-4o-search-preview-2025-03-11";
inline constexpr const char* kGpt4oMini = "gpt-4o-mini";
inline constexpr const char* kGpt4oMini20240718 = "gpt-4o-mini-2024-07-18";
inline constexpr const char* kGpt4oMiniSearchPreview =
    "gpt-4o-mini-search-preview";
inline constexpr const char* kGpt4oMiniSearchPreview20250311 =
    "gpt-4o-mini-search-preview-2025-03-11";

// GPT-4 Turbo and legacy GPT-4
inline constexpr const char* kGpt4Turbo = "gpt-4-turbo";
inline constexpr const char* kGpt4Turbo20240409 = "gpt-4-turbo-2024-04-09";
inline constexpr const char* kGpt4TurboPreview = "gpt-4-turbo-preview";
inline constexpr const char* kGpt4_0125Preview = "gpt-4-0125-preview";
inline constexpr const char* kGpt4_1106Preview = "gpt-4-1106-preview";
inline constexpr const char* kGpt4 = "gpt-4";
inline constexpr const char* kGpt4_0613 = "gpt-4-0613";

// GPT-3.5 series
inline constexpr const char* kGpt35Turbo = "gpt-3.5-turbo";
inline constexpr const char* kGpt35Turbo0125 = "gpt-3.5-turbo-0125";
inline constexpr const char* kGpt35Turbo1106 = "gpt-3.5-turbo-1106";

// ChatGPT models
inline constexpr const char* kChatGpt4oLatest = "chatgpt-4o-latest";

// Backward compatibility aliases
inline constexpr const char* kGPT_4O = kGpt4o;
inline constexpr const char* kGPT_4O_MINI = kGpt4oMini;
inline constexpr const char* kGPT_4_TURBO = kGpt4Turbo;
inline constexpr const char* kGPT_3_5_TURBO = kGpt35Turbo;
inline constexpr const char* kO1_PREVIEW = kO1Preview;
inline constexpr const char* kO1_MINI = kO1Mini;
}  // namespace models

}  // namespace openai
}  // namespace ai