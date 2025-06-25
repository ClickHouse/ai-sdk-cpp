#include "ai/anthropic.h"
#include "ai/types/client.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"
#include "ai/types/stream_result.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace ai {
namespace anthropic {

namespace {

/// Anthropic client implementation
class AnthropicClient : public Client {
 public:
  AnthropicClient(const std::string& api_key, const std::string& base_url)
      : api_key_(api_key), base_url_(base_url) {}

  GenerateResult generate_text(const GenerateOptions& options) override {
    // TODO(kaushik): Implement Anthropic API calls
    return GenerateResult("Anthropic client not yet implemented");
  }

  StreamResult stream_text(const StreamOptions& options) override {
    // TODO(kaushik): Implement Anthropic streaming
    return StreamResult();
  }

  bool is_valid() const override { return !api_key_.empty(); }

  std::string provider_name() const override { return "anthropic"; }

  std::vector<std::string> supported_models() const override {
    return {"claude-3-5-sonnet-20241022", "claude-3-5-haiku-20241022",
            "claude-3-opus-20240229", "claude-3-sonnet-20240229",
            "claude-3-haiku-20240307"};
  }

  bool supports_model(const std::string& model_name) const override {
    auto models = supported_models();
    return std::find(models.begin(), models.end(), model_name) != models.end();
  }

  std::string config_info() const override {
    return "Anthropic API (base_url: " + base_url_ + ")";
  }

 private:
  std::string api_key_;
  std::string base_url_;
};

}  // namespace

// Factory function implementations
Client create_client() {
  const char* api_key = std::getenv("ANTHROPIC_API_KEY");
  if (!api_key) {
    throw std::runtime_error("ANTHROPIC_API_KEY environment variable not set");
  }
  return Client(
      std::make_unique<AnthropicClient>(api_key, "https://api.anthropic.com"));
}

Client create_client(const std::string& api_key) {
  return Client(
      std::make_unique<AnthropicClient>(api_key, "https://api.anthropic.com"));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<AnthropicClient>(api_key, base_url));
}

}  // namespace anthropic
}  // namespace ai