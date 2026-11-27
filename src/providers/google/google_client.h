#pragma once

#include "ai/retry/retry_policy.h"
#include "ai/types/stream_options.h"
#include "providers/base_provider_client.h"

#include <optional>
#include <string>

namespace ai {
namespace google {

inline constexpr const char* kDefaultBaseUrl =
    "https://generativelanguage.googleapis.com";

/// Native Gemini API client (`generateContent` and
/// `streamGenerateContent?alt=sse` under `/v1beta`), authenticated with the
/// `x-goog-api-key` header.
class GoogleClient : public providers::BaseProviderClient {
 public:
  explicit GoogleClient(const std::string& api_key,
                        const std::string& base_url = kDefaultBaseUrl);

  GoogleClient(const std::string& api_key,
               const std::string& base_url,
               const retry::RetryConfig& retry_config);

  StreamResult stream_text(const StreamOptions& options) override;
  EmbeddingResult embeddings(const EmbeddingOptions& options) override;
  std::string provider_name() const override;
  /// Any model name is passed to the API, which rejects unknown ones.
  bool supports_model(const std::string& model_name) const override;
  ModelListResult list_models() override;
  std::string config_info() const override;
  std::string default_model() const override;

  // Member access for testing
  const std::string& get_api_key() const { return config_.api_key; }
  const std::string& get_base_url() const { return config_.base_url; }

  /// `/v1beta/models/{model}:{method}`
  static std::string model_path(const std::string& model,
                                const std::string& method);

 protected:
  std::string completions_path(const GenerateOptions& options) const override;

 private:
  GoogleClient(const std::string& api_key,
               const std::string& base_url,
               std::optional<retry::RetryConfig> retry_config);
};

}  // namespace google
}  // namespace ai
