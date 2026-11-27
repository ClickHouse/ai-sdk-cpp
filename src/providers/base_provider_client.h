#pragma once

#include "ai/retry/retry_policy.h"
#include "ai/types/client.h"
#include "ai/types/embedding_options.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"
#include "http/http_request_handler.h"

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace providers {

// Configuration for a provider
struct ProviderConfig {
  std::string api_key;
  std::string base_url;
  std::string completions_endpoint_path;  // e.g. "/v1/chat/completions"
  std::string embeddings_endpoint_path;
  std::string auth_header_name;    // e.g., "Authorization" or "x-api-key"
  std::string auth_header_prefix;  // e.g., "Bearer " or ""
  httplib::Headers extra_headers;  // Additional headers like anthropic-version

  // Optional retry configuration
  std::optional<retry::RetryConfig> retry_config;

  // Endpoint listing the available models (OpenAI and Anthropic shape:
  // `{"data": [{"id": ...}, ...]}`, Anthropic paginated with `has_more` and
  // `last_id`).
  std::string models_endpoint_path;
};

// Interface for provider-specific request building
class RequestBuilder {
 public:
  virtual ~RequestBuilder() = default;
  virtual nlohmann::json build_request_json(const GenerateOptions& options) = 0;
  virtual nlohmann::json build_request_json(
      const EmbeddingOptions& options) = 0;
  virtual httplib::Headers build_headers(const ProviderConfig& config) = 0;
};

// Interface for provider-specific response parsing
class ResponseParser {
 public:
  virtual ~ResponseParser() = default;
  virtual GenerateResult parse_success_completion_response(
      const nlohmann::json& response) = 0;
  virtual GenerateResult parse_error_completion_response(
      int status_code,
      const std::string& body) = 0;
  virtual EmbeddingResult parse_success_embedding_response(
      const nlohmann::json& response) = 0;
  virtual EmbeddingResult parse_error_embedding_response(
      int status_code,
      const std::string& body) = 0;
};

// Base client that uses composition to share common functionality
class BaseProviderClient : public Client {
 public:
  BaseProviderClient(const ProviderConfig& config,
                     std::unique_ptr<RequestBuilder> request_builder,
                     std::unique_ptr<ResponseParser> response_parser);

  // Implements the common flow using the composed components
  GenerateResult generate_text(const GenerateOptions& options) override;
  StreamResult stream_text(const StreamOptions& options) override;
  EmbeddingResult embeddings(const EmbeddingOptions& options) override;
  ModelListResult list_models() override;

  bool is_valid() const override { return !config_.api_key.empty(); }

 protected:
  // GET `path` and parse the JSON body into `out`. Returns the error message
  // (parsed by the provider's error parser for HTTP errors) on failure.
  std::optional<std::string> get_json(const std::string& path,
                                      const httplib::Headers& headers,
                                      nlohmann::json& out);

  // Hook for derived classes to customize behavior
  virtual GenerateResult generate_text_single_step(
      const GenerateOptions& options);

  // Path of the completion request; providers that address the model in the
  // URL (such as Gemini's `/v1beta/models/{model}:generateContent`) override
  // it.
  virtual std::string completions_path(const GenerateOptions& options) const {
    (void)options;
    return config_.completions_endpoint_path;
  }

  ProviderConfig config_;
  std::unique_ptr<http::HttpRequestHandler> http_handler_;
  std::unique_ptr<RequestBuilder> request_builder_;
  std::unique_ptr<ResponseParser> response_parser_;
};

}  // namespace providers
}  // namespace ai