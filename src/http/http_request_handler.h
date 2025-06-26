#pragma once

#include "ai/types/generate_options.h"

#include <functional>
#include <httplib.h>
#include <string>

#include <nlohmann/json.hpp>

namespace ai {
namespace http {

struct HttpConfig {
  std::string host;
  bool use_ssl = true;
  int connection_timeout_sec = 30;
  int read_timeout_sec = 120;
  bool verify_ssl_cert = false;
};

class HttpRequestHandler {
 public:
  explicit HttpRequestHandler(const HttpConfig& config);

  // Makes a POST request and returns the raw response wrapped in GenerateResult
  GenerateResult post(const std::string& path,
                      const httplib::Headers& headers,
                      const std::string& body,
                      const std::string& content_type = "application/json");

  // Extracts host and SSL settings from a base URL
  static HttpConfig parse_base_url(const std::string& base_url);

 private:
  HttpConfig config_;

  // Handler for processing HTTP responses
  using ResponseHandler =
      std::function<GenerateResult(const httplib::Result&, const std::string&)>;
  GenerateResult make_request(const std::string& path,
                              const httplib::Headers& headers,
                              const std::string& body,
                              const std::string& content_type,
                              ResponseHandler handler);
};

}  // namespace http
}  // namespace ai