#include "http_request_handler.h"

#include <spdlog/spdlog.h>

namespace ai {
namespace http {

HttpRequestHandler::HttpRequestHandler(const HttpConfig& config)
    : config_(config) {
  spdlog::debug("HttpRequestHandler initialized - host: {}, use_ssl: {}",
                config_.host, config_.use_ssl);
}

HttpConfig HttpRequestHandler::parse_base_url(const std::string& base_url) {
  HttpConfig config;
  std::string url = base_url;

  // Extract protocol and host
  if (url.starts_with("https://")) {
    config.host = url.substr(8);
    config.use_ssl = true;
  } else if (url.starts_with("http://")) {
    config.host = url.substr(7);
    config.use_ssl = false;
  } else {
    config.host = url;
    config.use_ssl = true;
  }

  // Remove trailing slash and path if any
  if (auto pos = config.host.find('/'); pos != std::string::npos) {
    config.host = config.host.substr(0, pos);
  }

  return config;
}

GenerateResult HttpRequestHandler::post(const std::string& path,
                                        const httplib::Headers& headers,
                                        const std::string& body,
                                        const std::string& content_type) {
  auto handler = [](const httplib::Result& res,
                    const std::string& protocol) -> GenerateResult {
    if (!res) {
      spdlog::error("{} request failed - no response", protocol);
      return GenerateResult("Network error: Failed to connect to API");
    }

    spdlog::debug("Got response: status={}, body_size={}", res->status,
                  res->body.size());

    if (res->status == 200) {
      GenerateResult result;
      result.text = res->body;
      result.finish_reason = kFinishReasonStop;  // HTTP request succeeded
      return result;
    }

    // For non-200 responses, return error with full body for parsing
    GenerateResult error_result;
    error_result.error = res->body;
    error_result.finish_reason = kFinishReasonError;
    error_result.provider_metadata = std::to_string(res->status);
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
    spdlog::debug("Making {} request to {}:{}{}",
                  config_.use_ssl ? "HTTPS" : "HTTP", config_.host, path,
                  " with body size: " + std::to_string(body.size()));

    if (config_.use_ssl) {
      httplib::SSLClient cli(config_.host);
      cli.set_connection_timeout(config_.connection_timeout_sec, 0);
      cli.set_read_timeout(config_.read_timeout_sec, 0);
      cli.enable_server_certificate_verification(config_.verify_ssl_cert);

      auto res = cli.Post(path, headers, body, content_type);
      return handler(res, "HTTPS");
    } else {
      httplib::Client cli(config_.host);
      cli.set_connection_timeout(config_.connection_timeout_sec, 0);
      cli.set_read_timeout(config_.read_timeout_sec, 0);

      auto res = cli.Post(path, headers, body, content_type);
      return handler(res, "HTTP");
    }
  } catch (const std::exception& e) {
    spdlog::error("Exception in make_request: {}", e.what());
    return GenerateResult(std::string("Request failed: ") + e.what());
  } catch (...) {
    spdlog::error("Unknown exception in make_request");
    return GenerateResult("Request failed: Unknown error");
  }
}

}  // namespace http
}  // namespace ai