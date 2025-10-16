#pragma once

#include "ai/types/generate_options.h"
#include "providers/base_provider_client.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace anthropic {

class AnthropicResponseParser : public providers::ResponseParser {
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

 private:
  static FinishReason parse_stop_reason(const std::string& reason);
};

}  // namespace anthropic
}  // namespace ai