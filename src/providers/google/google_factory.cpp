#include "ai/errors.h"
#include "ai/google.h"
#include "google_client.h"
#include "utils/env_utils.h"

#include <memory>
#include <optional>

namespace ai {
namespace google {

namespace {

/// GOOGLE_API_KEY takes precedence over GEMINI_API_KEY, as in the Google Gen
/// AI SDKs.
std::optional<std::string> api_key_from_env() {
  if (auto key = utils::non_empty_env("GOOGLE_API_KEY")) {
    return key;
  }
  return utils::non_empty_env("GEMINI_API_KEY");
}

std::string get_api_key_or_default(const std::string& api_key) {
  if (!api_key.empty()) {
    return api_key;
  }
  auto env_api_key = api_key_from_env();
  if (!env_api_key) {
    throw ConfigurationError(
        "API key not provided and neither GOOGLE_API_KEY nor GEMINI_API_KEY "
        "environment variable is set");
  }
  return *std::move(env_api_key);
}

std::string get_base_url_or_default(const std::string& base_url) {
  if (base_url.empty()) {
    return kDefaultBaseUrl;
  }
  // Paths are appended to the base URL.
  std::string url = base_url;
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }
  return url;
}

}  // namespace

Client create_client() {
  return Client(std::make_unique<GoogleClient>(get_api_key_or_default(""),
                                               kDefaultBaseUrl));
}

Client create_client(const std::string& api_key) {
  return Client(std::make_unique<GoogleClient>(get_api_key_or_default(api_key),
                                               kDefaultBaseUrl));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<GoogleClient>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url)));
}

Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config) {
  return Client(std::make_unique<GoogleClient>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url),
      retry_config));
}

std::optional<Client> try_create_client() {
  auto api_key = api_key_from_env();
  if (!api_key) {
    return std::nullopt;
  }
  return Client(std::make_unique<GoogleClient>(*api_key, kDefaultBaseUrl));
}

}  // namespace google
}  // namespace ai
