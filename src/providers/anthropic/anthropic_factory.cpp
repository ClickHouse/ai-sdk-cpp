#include "anthropic_factory.h"

#include "ai/errors.h"
#include "anthropic_client.h"

#include <cstdlib>
#include <memory>
#include <optional>

namespace ai {
namespace anthropic {

namespace {
constexpr const char* kDefaultBaseUrl = "https://api.anthropic.com";

std::string get_api_key_or_default(const std::string& api_key) {
  if (!api_key.empty()) {
    return api_key;
  }

  const char* env_api_key = std::getenv("ANTHROPIC_API_KEY");
  if (!env_api_key) {
    throw ConfigurationError(
        "API key not provided and ANTHROPIC_API_KEY environment variable not "
        "set");
  }
  return env_api_key;
}

std::string get_base_url_or_default(const std::string& base_url) {
  return base_url.empty() ? kDefaultBaseUrl : base_url;
}
}  // namespace

Client create_client() {
  return Client(std::make_unique<AnthropicClient>(get_api_key_or_default(""),
                                                  kDefaultBaseUrl));
}

Client create_client(const std::string& api_key) {
  return Client(std::make_unique<AnthropicClient>(
      get_api_key_or_default(api_key), kDefaultBaseUrl));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<AnthropicClient>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url)));
}

std::optional<Client> try_create_client() {
  const char* api_key = std::getenv("ANTHROPIC_API_KEY");
  if (!api_key) {
    return std::nullopt;
  }
  return Client(std::make_unique<AnthropicClient>(api_key, kDefaultBaseUrl));
}

}  // namespace anthropic
}  // namespace ai