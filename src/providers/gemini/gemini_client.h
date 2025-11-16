#pragma once

#include "ai/retry/retry_policy.h"
#include "ai/types/stream_options.h"
#include "providers/base_provider_client.h"

#include <string>
#include <vector>

namespace ai {
namespace gemini {

class GeminiClient : public providers::BaseProviderClient {
 public:
  explicit GeminiClient(const std::string& api_key,
                        const std::string& base_url = "https://api.openai.com");

  explicit GeminiClient(const std::string& api_key,
                        const std::string& base_url,
                        const retry::RetryConfig& retry_config);

  StreamResult stream_text(const StreamOptions& options) override;
  GenerateResult generate_text_single_step(const GenerateOptions& options) override;
  std::string provider_name() const override;
  std::vector<std::string> supported_models() const override;
  bool supports_model(const std::string& model_name) const override;
  std::string config_info() const override;
  std::string default_model() const override;
};

}  // namespace gemini
}  // namespace ai
