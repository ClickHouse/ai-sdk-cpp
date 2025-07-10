#pragma once

#include "ai/retry/retry_policy.h"
#include "ai/types/stream_options.h"
#include "providers/base_provider_client.h"

#include <string>
#include <vector>

namespace ai {
namespace openai {

class OpenAIClient : public providers::BaseProviderClient {
 public:
  explicit OpenAIClient(const std::string& api_key,
                        const std::string& base_url = "https://api.openai.com");

  explicit OpenAIClient(const std::string& api_key,
                        const std::string& base_url,
                        const retry::RetryConfig& retry_config);

  // Override only what's specific to OpenAI
  StreamResult stream_text(const StreamOptions& options) override;
  std::string provider_name() const override;
  std::vector<std::string> supported_models() const override;
  bool supports_model(const std::string& model_name) const override;
  std::string config_info() const override;

  // Member access for testing
  const std::string& get_api_key() const { return config_.api_key; }
  const std::string& get_base_url() const { return config_.base_url; }
};

}  // namespace openai
}  // namespace ai