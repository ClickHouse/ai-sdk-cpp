#include "openai_factory.h"

#include "ai/errors.h"
#include "openai_client.h"

#include <cstdlib>
#include <memory>
#include <optional>

namespace ai {
namespace openai {

namespace {
constexpr const char* kDefaultBaseUrl = "https://api.openai.com";

std::string get_api_key_or_default(const std::string& api_key) {
  if (!api_key.empty()) {
    return api_key;
  }

  const char* env_api_key = std::getenv("OPENAI_API_KEY");
  if (!env_api_key) {
    throw ConfigurationError(
        "API key not provided and OPENAI_API_KEY environment variable not set");
  }
  return env_api_key;
}

std::string get_base_url_or_default(const std::string& base_url) {
  return base_url.empty() ? kDefaultBaseUrl : base_url;
}
}  // namespace

Client create_client() {
  return Client(std::make_unique<OpenAIClient>(get_api_key_or_default(""),
                                               kDefaultBaseUrl));
}

Client create_client(const std::string& api_key) {
  return Client(std::make_unique<OpenAIClient>(get_api_key_or_default(api_key),
                                               kDefaultBaseUrl));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<OpenAIClient>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url)));
}

Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config) {
  return Client(std::make_unique<OpenAIClient>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url),
      retry_config));
}

std::optional<Client> try_create_client() {
  const char* api_key = std::getenv("OPENAI_API_KEY");
  if (!api_key) {
    return std::nullopt;
  }
  return Client(std::make_unique<OpenAIClient>(api_key, kDefaultBaseUrl));
}

}  // namespace openai
}  // namespace ai