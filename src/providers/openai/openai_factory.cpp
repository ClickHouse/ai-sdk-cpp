#include "openai_factory.h"

#include "ai/errors.h"
#include "openai_client.h"
#include "openai_responses_client.h"
#include "utils/env_utils.h"

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

  auto env_api_key = utils::non_empty_env("OPENAI_API_KEY");
  if (!env_api_key) {
    throw ConfigurationError(
        "API key not provided and OPENAI_API_KEY environment variable not set");
  }
  return *std::move(env_api_key);
}

std::string get_base_url_or_default(const std::string& base_url) {
  return base_url.empty() ? kDefaultBaseUrl : base_url;
}

template <typename ClientImpl>
Client make_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<ClientImpl>(
      get_api_key_or_default(api_key), get_base_url_or_default(base_url)));
}

template <typename ClientImpl>
Client make_client(const std::string& api_key,
                   const std::string& base_url,
                   const retry::RetryConfig& retry_config) {
  return Client(std::make_unique<ClientImpl>(get_api_key_or_default(api_key),
                                             get_base_url_or_default(base_url),
                                             retry_config));
}

template <typename ClientImpl>
std::optional<Client> try_make_client() {
  auto api_key = utils::non_empty_env("OPENAI_API_KEY");
  if (!api_key) {
    return std::nullopt;
  }
  return Client(std::make_unique<ClientImpl>(*api_key, kDefaultBaseUrl));
}
}  // namespace

Client create_client() {
  return make_client<OpenAIClient>("", "");
}

Client create_client(const std::string& api_key) {
  return make_client<OpenAIClient>(api_key, "");
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return make_client<OpenAIClient>(api_key, base_url);
}

Client create_client(const std::string& api_key,
                     const std::string& base_url,
                     const retry::RetryConfig& retry_config) {
  return make_client<OpenAIClient>(api_key, base_url, retry_config);
}

std::optional<Client> try_create_client() {
  return try_make_client<OpenAIClient>();
}

Client create_responses_client() {
  return make_client<OpenAIResponsesClient>("", "");
}

Client create_responses_client(const std::string& api_key) {
  return make_client<OpenAIResponsesClient>(api_key, "");
}

Client create_responses_client(const std::string& api_key,
                               const std::string& base_url) {
  return make_client<OpenAIResponsesClient>(api_key, base_url);
}

Client create_responses_client(const std::string& api_key,
                               const std::string& base_url,
                               const retry::RetryConfig& retry_config) {
  return make_client<OpenAIResponsesClient>(api_key, base_url, retry_config);
}

std::optional<Client> try_create_responses_client() {
  return try_make_client<OpenAIResponsesClient>();
}

}  // namespace openai
}  // namespace ai
