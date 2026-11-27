#pragma once

#include "openai_response_parser.h"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

/// Parses `POST /v1/responses` replies. Errors and embeddings are shared with
/// the Chat Completions parser.
///
/// The assistant turn is returned in `response_messages` with its items in
/// their original order: text as `TextContentPart`, function calls as
/// `ToolCallContentPart`, and the items that have to be replayed verbatim
/// (`reasoning`, and the item IDs of the function calls) as
/// `ProviderContentPart`s named `kResponsesProviderName`.
class OpenAIResponsesResponseParser : public OpenAIResponseParser {
 public:
  GenerateResult parse_success_completion_response(
      const nlohmann::json& response) override;

  /// Token usage of a Responses API `usage` object.
  static Usage parse_usage(const nlohmann::json& usage);

  /// The finish reason of a response with the given `status` and
  /// `incomplete_details` (`has_function_calls`: whether it has completed
  /// function calls), or nullopt when the response did not finish
  /// (failed, cancelled, still in progress, or incomplete for a reason without
  /// a finish reason), in which case `error` describes why.
  static std::optional<FinishReason> parse_finish_reason(
      const nlohmann::json& response,
      bool has_function_calls,
      std::string& error);
};

}  // namespace openai
}  // namespace ai
