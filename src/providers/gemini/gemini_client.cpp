#include "gemini_client.h"

#include "ai/logger.h"
#include "providers/gemini/gemini_request_builder.h"
#include "providers/gemini/gemini_response_parser.h"

#include <algorithm>
#include <memory>

namespace ai {
namespace gemini {

GeminiClient::GeminiClient(const std::string& api_key,
                           const std::string& base_url)
    : BaseProviderClient(
          providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              .completions_endpoint_path = "/v1/chat/completions",
              .embeddings_endpoint_path = "/v1/embeddings",
              .auth_header_name = "Authorization",
              .auth_header_prefix = "Bearer ",
              .extra_headers = {}},
                std::make_unique<::ai::gemini::GeminiRequestBuilder>(providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              .completions_endpoint_path = "/v1/chat/completions",
              .embeddings_endpoint_path = "/v1/embeddings",
              .auth_header_name = "Authorization",
              .auth_header_prefix = "Bearer ",
              .extra_headers = {}}),
                std::make_unique<::ai::gemini::GeminiResponseParser>()) {
  ai::logger::log_debug("Gemini client initialized with base_url: {}", base_url);
}

GeminiClient::GeminiClient(const std::string& api_key,
                           const std::string& base_url,
                           const retry::RetryConfig& retry_config)
    : BaseProviderClient(
          providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              .completions_endpoint_path = "/v1/chat/completions",
              .embeddings_endpoint_path = "/v1/embeddings",
              .auth_header_name = "Authorization",
              .auth_header_prefix = "Bearer ",
              .extra_headers = {},
              .retry_config = retry_config},
                std::make_unique<::ai::gemini::GeminiRequestBuilder>(providers::ProviderConfig{
              .api_key = api_key,
              .base_url = base_url,
              .completions_endpoint_path = "/v1/chat/completions",
              .embeddings_endpoint_path = "/v1/embeddings",
              .auth_header_name = "Authorization",
              .auth_header_prefix = "Bearer ",
              .extra_headers = {} ,
              .retry_config = retry_config}),
                std::make_unique<::ai::gemini::GeminiResponseParser>()) {
  ai::logger::log_debug(
      "Gemini client initialized with base_url: {} and custom retry config",
      base_url);
}

StreamResult GeminiClient::stream_text(const StreamOptions& options) {
  ai::logger::log_debug("Starting Gemini text streaming - model: {}, prompt length: {}", options.model, options.prompt.length());

  auto request_json = request_builder_->build_request_json(options);
  request_json["stream"] = true;
  auto headers = request_builder_->build_headers(config_);
  headers.emplace("Accept", "text/event-stream");

  // Not implementing streaming specifics here; reuse base behavior if needed
  ai::logger::log_info("Gemini streaming not fully implemented; returning empty StreamResult");
  return StreamResult();
}

GenerateResult GeminiClient::generate_text_single_step(const GenerateOptions& options) {
  // Build request JSON using the provider-specific builder
  auto request_json = request_builder_->build_request_json(options);
  std::string json_body = request_json.dump();
  ai::logger::log_debug("Gemini single-step request JSON: {}", json_body);

  // Build headers (this will add X-goog-api-key for Google GL base_url)
  auto headers = request_builder_->build_headers(config_);

  // Build the GL-specific path: /v1beta/models/{model}:generateContent
  std::string model = options.model.empty() ? default_model() : options.model;
  std::string path = "/v1beta/models/" + model + ":generateContent";

  // Log the outgoing request path and headers at info level so Postgres
  // logs capture them even if debug is not enabled.
  std::string headers_str;
  size_t header_chars = 0;
  for (const auto& [k, v] : headers) {
    if (!headers_str.empty()) headers_str += ", ";
    headers_str += k + ":" + (v.size() > 64 ? v.substr(0, 64) + "...[trunc]" : v);
    header_chars += k.size() + v.size();
    if (header_chars > 1024) { headers_str += "...[trunc]"; break; }
  }
  ai::logger::log_info("Gemini request -> path='{}' headers='{}' body_size={}", path, headers_str, json_body.size());

  // Make the request using application/json
  auto result = http_handler_->post(path, headers, json_body, "application/json");

  ai::logger::log_info("Gemini request returned: success={} provider_metadata='{}' error_size={}", result.is_success(), result.provider_metadata.value_or(""), result.error.has_value() ? result.error->size() : 0);

  if (!result.is_success()) {
    if (result.provider_metadata.has_value()) {
      int status_code = std::stoi(result.provider_metadata.value());
      return response_parser_->parse_error_completion_response(
          status_code, result.error.value_or(""));
    }
    return result;
  }

  // Parse JSON response
  nlohmann::json json_response;
  try {
    json_response = nlohmann::json::parse(result.text);
  } catch (const nlohmann::json::exception& e) {
    ai::logger::log_error("Failed to parse Gemini response JSON: {}", e.what());
    // Include a truncated version of the raw response in the returned error
    const size_t kMaxErrBody = 4096;
    std::string truncated = result.text.substr(0, std::min(kMaxErrBody, result.text.size()));
    std::string err = std::string("Failed to parse response: ") + e.what() + "; raw_response=" + truncated + (result.text.size() > kMaxErrBody ? "...[truncated]" : "");
    return GenerateResult(err);
  }

  auto parsed_result = response_parser_->parse_success_completion_response(json_response);
  if (!parsed_result) {
    const size_t kMaxErrBody = 4096;
    std::string raw = json_response.dump();
    std::string truncated = raw.substr(0, std::min(kMaxErrBody, raw.size()));
    std::string err = parsed_result.error_message() + "; raw_response=" + truncated + (raw.size() > kMaxErrBody ? "...[truncated]" : "");
    return GenerateResult(err);
  }
  return parsed_result;
}

std::string GeminiClient::provider_name() const { return "gemini"; }

std::vector<std::string> GeminiClient::supported_models() const {
  return {"gemini-2.0-flash", "gemini-2.0"};
}

bool GeminiClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string GeminiClient::config_info() const {
  return "Gemini API (base_url: " + config_.base_url + ")";
}

std::string GeminiClient::default_model() const { return "gemini-2.0-flash"; }

}  // namespace gemini
}  // namespace ai
