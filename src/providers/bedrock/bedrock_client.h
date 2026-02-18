#pragma once

#include "ai/types/client.h"
#include "ai/types/embedding_options.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"
#include "ai/types/stream_result.h"

#include <memory>
#include <string>
#include <vector>

// Forward declaration for BedrockConfig
namespace ai {
namespace bedrock {
struct BedrockConfig;
}
}  // namespace ai

namespace ai {
namespace bedrock {

/// Bedrock client implementation
/// Unlike OpenAI/Anthropic providers, this does not extend BaseProviderClient
/// because it uses AWS SDK rather than direct HTTP calls via httplib.
class BedrockClient : public Client {
 public:
  explicit BedrockClient(const BedrockConfig& config);
  ~BedrockClient();

  // Non-copyable, non-movable due to PImpl
  BedrockClient(const BedrockClient&) = delete;
  BedrockClient& operator=(const BedrockClient&) = delete;
  BedrockClient(BedrockClient&&) = delete;
  BedrockClient& operator=(BedrockClient&&) = delete;

  // Client interface
  GenerateResult generate_text(const GenerateOptions& options) override;
  EmbeddingResult embeddings(const EmbeddingOptions& options) override;
  StreamResult stream_text(const StreamOptions& options) override;

  bool is_valid() const override;
  std::string provider_name() const override;
  std::vector<std::string> supported_models() const override;
  bool supports_model(const std::string& model_name) const override;
  std::string config_info() const override;
  std::string default_model() const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace bedrock
}  // namespace ai
