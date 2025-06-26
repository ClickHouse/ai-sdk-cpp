#pragma once

#include "ai/types/generate_options.h"
#include "providers/base_provider_client.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace anthropic {

class AnthropicRequestBuilder : public providers::RequestBuilder {
 public:
  nlohmann::json build_request_json(const GenerateOptions& options) override;
  httplib::Headers build_headers(
      const providers::ProviderConfig& config) override;
};

}  // namespace anthropic
}  // namespace ai