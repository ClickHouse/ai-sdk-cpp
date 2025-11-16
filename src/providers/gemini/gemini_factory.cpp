#include "gemini_factory.h"

#include "ai/errors.h"
#include "gemini_client.h"
#include "ai/logger.h"

#include <cstdlib>
#include <memory>
#include <optional>

namespace ai {
namespace gemini {

namespace {
constexpr const char* kDefaultBaseUrl = "https://generativelanguage.googleapis.com";

std::string get_api_key_or_default(const std::string& api_key) {
  if (!api_key.empty()) {
    return api_key;
  }

  const char* env_api_key = std::getenv("GEMINI_API_KEY");
  if (!env_api_key) {
    throw ConfigurationError(
        "API key not provided and GEMINI_API_KEY environment variable not set");
  }
  return env_api_key;
}

std::string get_base_url_or_default(const std::string& base_url) {
  return base_url.empty() ? kDefaultBaseUrl : base_url;
}
}  // namespace

Client create_client() {
  auto key = get_api_key_or_default("");
  ai::logger::log_info("gemini_factory::create_client() called with api_key length={} base_url={}", key.size(), kDefaultBaseUrl);
  return Client(std::make_unique<GeminiClient>(key, kDefaultBaseUrl));
}

Client create_client(const std::string& api_key) {
  auto key = get_api_key_or_default(api_key);
  ai::logger::log_info("gemini_factory::create_client(api_key) called with api_key length={} base_url={}", key.size(), kDefaultBaseUrl);
  return Client(std::make_unique<GeminiClient>(key, kDefaultBaseUrl));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<GeminiClient>(get_api_key_or_default(api_key),
                                               get_base_url_or_default(base_url)));
}

std::optional<Client> try_create_client() {
  const char* api_key = std::getenv("GEMINI_API_KEY");
  if (!api_key) {
    return std::nullopt;
  }
  return Client(std::make_unique<GeminiClient>(api_key, kDefaultBaseUrl));
}

}  // namespace gemini
}  // namespace ai
