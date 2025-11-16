#include "http_request_handler.h"

#include "ai/logger.h"
#include "ai/retry/retry_policy.h"
#include "utils/response_utils.h"

namespace ai {
namespace http {

HttpRequestHandler::HttpRequestHandler(const HttpConfig& config)
    : config_(config) {
  ai::logger::log_debug(
      "HttpRequestHandler initialized - host: {}, use_ssl: {}", config_.host,
      config_.use_ssl);
}

HttpConfig HttpRequestHandler::parse_base_url(const std::string& base_url) {
  HttpConfig config;
  std::string url = base_url;

  // Extract protocol
  if (url.starts_with("https://")) {
    url = url.substr(8);
    config.use_ssl = true;
  } else if (url.starts_with("http://")) {
    url = url.substr(7);
    config.use_ssl = false;
  } else {
    config.use_ssl = true;
  }

  // Extract host and path
  auto pos = url.find('/');
  config.host = (pos != std::string::npos) ? url.substr(0, pos) : url;
  config.base_path = (pos != std::string::npos) ? url.substr(pos) : "";

  // Remove trailing slash from base_path if present
  if (!config.base_path.empty() && config.base_path.back() == '/') {
    config.base_path.pop_back();
  }

  return config;
}

GenerateResult HttpRequestHandler::post(const std::string& path,
                                        const httplib::Headers& headers,
                                        const std::string& body,
                                        const std::string& content_type) {
  // Create a retry policy with the configured settings
  retry::RetryPolicy retry_policy(config_.retry_config);

  // Define the function to execute with retry
  auto execute_request = [this, &path, &headers, &body,
                          &content_type]() -> GenerateResult {
    return execute_single_request(path, headers, body, content_type);
  };

  // Define the function to check if a result is retryable
  std::function<bool(const GenerateResult&)> is_retryable =
      [](const GenerateResult& result) -> bool {
    return result.is_retryable.value_or(false);
  };

  try {
    return retry_policy.execute_with_retry(execute_request, is_retryable);
  } catch (const retry::RetryError& e) {
    ai::logger::log_error("Request failed after retries: {}", e.what());
    GenerateResult error_result(e.what());
    error_result.is_retryable = false;  // Already retried
    return error_result;
  }
}

GenerateResult HttpRequestHandler::execute_single_request(
    const std::string& path,
    const httplib::Headers& headers,
    const std::string& body,
    const std::string& content_type) {
  auto handler = [](const httplib::Result& res,
                    const std::string& protocol) -> GenerateResult {
    if (!res) {
      ai::logger::log_error("{} request failed - no response", protocol);
      GenerateResult result("Network error: Failed to connect to API");
      result.is_retryable = true;  // Network errors are retryable
      return result;
    }

    ai::logger::log_debug("Got response: status={}, body_size={}", res->status,
                          res->body.size());

    if (res->status == 200) {
      GenerateResult result;
      result.text = res->body;
      result.finish_reason = kFinishReasonStop;  // HTTP request succeeded
      return result;
    }

    // For non-200 responses, log status and body (truncated) so callers
    // running inside host processes (e.g., Postgres) can see the server
    // response in logs and we can diagnose errors.
    const size_t kMaxLogBody = 4096;
    std::string truncated_body = res->body.substr(0, std::min(kMaxLogBody, res->body.size()));
    ai::logger::log_error("HTTP request returned non-200 status={} body={}{}",
                         res->status,
                         truncated_body,
                         (res->body.size() > kMaxLogBody ? "...[truncated]" : ""));

    // For non-200 responses, return error with full body for parsing
    GenerateResult error_result;
    error_result.error = res->body;
    error_result.finish_reason = kFinishReasonError;
    error_result.provider_metadata = std::to_string(res->status);
    error_result.is_retryable = is_status_code_retryable(res->status);
    return error_result;
  };

  return make_request(path, headers, body, content_type, handler);
}

GenerateResult HttpRequestHandler::make_request(const std::string& path,
                                                const httplib::Headers& headers,
                                                const std::string& body,
                                                const std::string& content_type,
                                                ResponseHandler handler) {
  try {
    // Combine base_path with the endpoint path
    std::string full_path = config_.base_path + path;

    ai::logger::log_debug("Making {} request to {}:{}{}",
                          config_.use_ssl ? "HTTPS" : "HTTP", config_.host,
                          full_path,
                          " with body size: " + std::to_string(body.size()));

    if (config_.use_ssl) {
      httplib::SSLClient cli(config_.host);
      cli.set_connection_timeout(config_.connection_timeout_sec, 0);
      cli.set_read_timeout(config_.read_timeout_sec, 0);
      cli.enable_server_certificate_verification(config_.verify_ssl_cert);

      auto res = cli.Post(full_path, headers, body, content_type);
      return handler(res, "HTTPS");
    } else {
      httplib::Client cli(config_.host);
      cli.set_connection_timeout(config_.connection_timeout_sec, 0);
      cli.set_read_timeout(config_.read_timeout_sec, 0);

      auto res = cli.Post(full_path, headers, body, content_type);
      return handler(res, "HTTP");
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Exception in make_request: {}", e.what());
    return GenerateResult(std::string("Request failed: ") + e.what());
  } catch (...) {
    ai::logger::log_error("Unknown exception in make_request");
    return GenerateResult("Request failed: Unknown error");
  }
}

}  // namespace http
}  // namespace ai