#pragma once

#include "ai/types/embedding_options.h"
#include "ai/types/generate_options.h"
#include "providers/base_provider_client.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace gemini {

class GeminiRequestBuilder : public providers::RequestBuilder {
 public:
  explicit GeminiRequestBuilder(const providers::ProviderConfig& config);
  nlohmann::json build_request_json(const GenerateOptions& options) override;
  nlohmann::json build_request_json(const EmbeddingOptions& options) override;
  httplib::Headers build_headers(const providers::ProviderConfig& config) override;

 private:
  providers::ProviderConfig config_;
};

}  // namespace gemini
}  // namespace ai
