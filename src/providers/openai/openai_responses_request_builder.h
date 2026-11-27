#pragma once

#include "openai_request_builder.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

/// Provider name of the `ProviderContentPart`s that the Responses API parser
/// produces and the Responses API request builder replays.
inline constexpr const char* kResponsesProviderName = "openai.responses";

/// Builds `POST /v1/responses` requests. Embedding requests and headers are
/// shared with the Chat Completions builder.
///
/// Requests are stateless (`store: false`): the conversation, including the
/// `reasoning` items of earlier turns (carried as `ProviderContentPart`s), is
/// replayed in `input` on every request instead of being referenced by
/// `previous_response_id`.
class OpenAIResponsesRequestBuilder : public OpenAIRequestBuilder {
 public:
  using OpenAIRequestBuilder::build_request_json;
  nlohmann::json build_request_json(const GenerateOptions& options) override;
};

}  // namespace openai
}  // namespace ai
