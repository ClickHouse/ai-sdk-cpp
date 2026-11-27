#pragma once

#include "ai/types/generate_options.h"
#include "providers/base_provider_client.h"

#include <cstddef>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace google {

/// Translate a candidate's `finishReason`; std::nullopt for the reasons that
/// mean the request failed (`MALFORMED_FUNCTION_CALL`, `OTHER`, ...).
/// A turn with function calls finishes with kFinishReasonToolCalls when it
/// stopped normally or at the output limit: the calls are complete either way
/// and have to be answered.
std::optional<FinishReason> parse_finish_reason(const std::string& reason,
                                                bool has_function_calls);

/// Map `usageMetadata`; thinking tokens count as completion tokens.
Usage parse_usage(const nlohmann::json& usage_metadata);

/// The error for a prompt the API refused to answer
/// (`promptFeedback.blockReason`), if any.
std::optional<std::string> prompt_block_error(const nlohmann::json& response);

/// The ID given to the `index`-th `functionCall` part of a response that has
/// no `id` (see `kSyntheticCallIdPrefix`).
std::string synthetic_call_id(std::size_t index);

class GoogleResponseParser : public providers::ResponseParser {
 public:
  GenerateResult parse_success_completion_response(
      const nlohmann::json& response) override;
  GenerateResult parse_error_completion_response(
      int status_code,
      const std::string& body) override;
  EmbeddingResult parse_success_embedding_response(
      const nlohmann::json& response) override;
  EmbeddingResult parse_error_embedding_response(
      int status_code,
      const std::string& body) override;
};

}  // namespace google
}  // namespace ai
