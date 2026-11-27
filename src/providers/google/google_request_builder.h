#pragma once

#include "ai/types/embedding_options.h"
#include "ai/types/generate_options.h"
#include "providers/base_provider_client.h"

#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace google {

/// `ProviderContentPart::provider` of the Gemini-specific parts of an
/// assistant turn: thought signatures and parts with no provider-neutral
/// representation (thoughts, signature-only parts).
inline constexpr const char* kGoogleProviderName = "google";

/// `ProviderContentPart::data["type"]` of a thought signature that belongs to
/// the text or tool-call part right after it; `data["thoughtSignature"]` holds
/// the signature.
inline constexpr const char* kThoughtSignaturePartType = "thought_signature";

/// `ProviderContentPart::data["type"]` of a Gemini part replayed verbatim from
/// `data["part"]`.
inline constexpr const char* kRawPartType = "part";

/// Prefix of the tool-call IDs the client makes up for `functionCall` parts
/// that come without an `id` (Gemini 2.5). They are only used to pair calls
/// with their results and are not sent to the API.
inline constexpr const char* kSyntheticCallIdPrefix = "gemini-call-";

/// Thought signature that the Gemini API documents for function calls it did
/// not produce itself (for example a conversation built from streamed tool
/// calls or from another provider): it skips the signature validation that
/// Gemini 3 and later models apply to the function calls of the current turn.
inline constexpr const char* kSkipThoughtSignatureValidator =
    "skip_thought_signature_validator";

/// Model name for the URL path: accepts both "gemini-…" and "models/gemini-…".
std::string model_resource_name(const std::string& model);

/// Ask for thought summaries (`includeThoughts`) in a request that thinks
/// (has a `thinkingConfig` other than `thinkingBudget: 0`); streams emit them
/// as reasoning events.
void include_thought_summaries(nlohmann::json& request);

class GoogleRequestBuilder : public providers::RequestBuilder {
 public:
  /// The `GenerateContentRequest` body; the model is part of the URL.
  nlohmann::json build_request_json(const GenerateOptions& options) override;
  nlohmann::json build_request_json(const EmbeddingOptions& options) override;
  httplib::Headers build_headers(
      const providers::ProviderConfig& config) override;
};

}  // namespace google
}  // namespace ai
