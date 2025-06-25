#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace ai {
namespace test {

// HTTP request representation
struct HttpRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;
  std::string body;

  // Helper methods
  bool has_header(const std::string& name) const;
  std::string get_header(const std::string& name) const;
  bool is_json() const;
};

// HTTP response representation
struct HttpResponse {
  int status_code = 200;
  std::map<std::string, std::string> headers;
  std::string body;

  // Helper constructors
  static HttpResponse ok(const std::string& body);
  static HttpResponse json(const std::string& json_body);
  static HttpResponse error(int status, const std::string& message);
  static HttpResponse not_found();
  static HttpResponse unauthorized();
  static HttpResponse rate_limited();
  static HttpResponse server_error();

  // Convenience methods
  void set_json_content_type();
  void set_cors_headers();
};

// Request handler function type
using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

// Mock HTTP server for testing
class MockHttpServer {
 public:
  explicit MockHttpServer(int port = 0);  // 0 = auto-assign port
  ~MockHttpServer();

  // Server lifecycle
  bool start();
  void stop();
  bool is_running() const;

  // Configuration
  int get_port() const;
  std::string get_base_url() const;

  // Request handling
  void set_default_handler(RequestHandler handler);
  void add_route(const std::string& method,
                 const std::string& path,
                 RequestHandler handler);
  void clear_routes();

  // OpenAI-specific helper methods
  void setup_openai_routes();
  void set_chat_completion_response(const std::string& response);
  void set_streaming_response(const std::vector<std::string>& events);
  void simulate_error(int status_code, const std::string& message);
  void simulate_timeout(int milliseconds);
  void simulate_rate_limit();

  // Request inspection
  std::vector<HttpRequest> get_received_requests() const;
  void clear_request_history();
  int get_request_count() const;
  HttpRequest get_last_request() const;

  // Behavior simulation
  void set_response_delay(int milliseconds);
  void set_random_failures(double failure_rate);  // 0.0 to 1.0
  void reset_behavior();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Helper class for common OpenAI API responses
class OpenAIResponseBuilder {
 public:
  static HttpResponse chat_completion(const std::string& content,
                                      const std::string& model = "gpt-4o",
                                      int prompt_tokens = 10,
                                      int completion_tokens = 20);

  static HttpResponse streaming_chunk(const std::string& content,
                                      const std::string& id = "chatcmpl-test",
                                      bool is_final = false);

  static HttpResponse streaming_done();

  static HttpResponse invalid_api_key();
  static HttpResponse model_not_found(const std::string& model);
  static HttpResponse rate_limit_exceeded();
  static HttpResponse invalid_request(const std::string& message);
  static HttpResponse server_error(
      const std::string& message = "Internal server error");

  // Malformed responses for error testing
  static HttpResponse malformed_json();
  static HttpResponse incomplete_response();
  static HttpResponse missing_fields();
};

// RAII helper for managing test server lifecycle
class TestServerScope {
 public:
  explicit TestServerScope(int port = 0);
  ~TestServerScope();

  MockHttpServer& server();
  std::string base_url() const;

  // Non-copyable
  TestServerScope(const TestServerScope&) = delete;
  TestServerScope& operator=(const TestServerScope&) = delete;

 private:
  std::unique_ptr<MockHttpServer> server_;
};

// Utility class for testing network conditions
class NetworkConditionSimulator {
 public:
  enum class Condition {
    kNormal,
    kSlow,
    kUnstable,
    kTimeout,
    kConnectionRefused,
    kDnsFailure
  };

  static void apply_condition(MockHttpServer& server, Condition condition);
  static void simulate_packet_loss(MockHttpServer& server, double loss_rate);
  static void simulate_bandwidth_limit(MockHttpServer& server,
                                       int bytes_per_second);
};

}  // namespace test
}  // namespace ai