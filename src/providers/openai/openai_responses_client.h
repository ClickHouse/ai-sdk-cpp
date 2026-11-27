#pragma once

#include "ai/retry/retry_policy.h"
#include "ai/types/stream_options.h"
#include "openai_client.h"

#include <string>

namespace ai {
namespace openai {

/// OpenAI client speaking the Responses API (`POST /v1/responses`) instead of
/// Chat Completions. Embeddings still use `/v1/embeddings`.
class OpenAIResponsesClient : public OpenAIClient {
 public:
  explicit OpenAIResponsesClient(
      const std::string& api_key,
      const std::string& base_url = "https://api.openai.com");

  OpenAIResponsesClient(const std::string& api_key,
                        const std::string& base_url,
                        const retry::RetryConfig& retry_config);

  StreamResult stream_text(const StreamOptions& options) override;
  std::string config_info() const override;
  std::string default_model() const override;
};

}  // namespace openai
}  // namespace ai
