#include "mock_http_server.h"

#include <chrono>
#include <httplib.h>
#include <random>
#include <sstream>
#include <thread>

#include <nlohmann/json.hpp>

namespace ai {
namespace test {

// HttpRequest implementation
bool HttpRequest::has_header(const std::string& name) const {
  return headers.find(name) != headers.end();
}

std::string HttpRequest::get_header(const std::string& name) const {
  auto it = headers.find(name);
  return it != headers.end() ? it->second : "";
}

bool HttpRequest::is_json() const {
  auto content_type = get_header("Content-Type");
  return content_type.find("application/json") != std::string::npos;
}

// HttpResponse implementation
HttpResponse HttpResponse::ok(const std::string& body) {
  HttpResponse response;
  response.status_code = 200;
  response.body = body;
  return response;
}

HttpResponse HttpResponse::json(const std::string& json_body) {
  HttpResponse response = ok(json_body);
  response.set_json_content_type();
  return response;
}

HttpResponse HttpResponse::error(int status, const std::string& message) {
  HttpResponse response;
  response.status_code = status;
  response.body = message;
  return response;
}

HttpResponse HttpResponse::not_found() {
  return error(404, "Not Found");
}

HttpResponse HttpResponse::unauthorized() {
  return error(401, "Unauthorized");
}

HttpResponse HttpResponse::rate_limited() {
  return error(429, "Rate Limit Exceeded");
}

HttpResponse HttpResponse::server_error() {
  return error(500, "Internal Server Error");
}

void HttpResponse::set_json_content_type() {
  headers["Content-Type"] = "application/json";
}

void HttpResponse::set_cors_headers() {
  headers["Access-Control-Allow-Origin"] = "*";
  headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
  headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
}

// MockHttpServer::Impl - Private implementation
class MockHttpServer::Impl {
 public:
  int port_;
  std::unique_ptr<httplib::Server> server_;
  std::thread server_thread_;
  std::atomic<bool> running_;

  std::map<std::string, RequestHandler> routes_;
  RequestHandler default_handler_;

  std::vector<HttpRequest> request_history_;
  mutable std::mutex history_mutex_;

  int response_delay_ms_;
  double failure_rate_;
  std::mt19937 rng_;

  Impl(int port)
      : port_(port),
        running_(false),
        response_delay_ms_(0),
        failure_rate_(0.0) {
    server_ = std::make_unique<httplib::Server>();
    rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
  }

  ~Impl() {
    if (running_) {
      stop();
    }
  }

  bool start() {
    if (running_)
      return true;

    setup_server();

    server_thread_ = std::thread([this]() {
      running_ = true;
      if (port_ == 0) {
        // Let httplib choose an available port
        server_->listen("localhost", 0);
      } else {
        server_->listen("localhost", port_);
      }
    });

    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return running_;
  }

  void stop() {
    if (!running_)
      return;

    running_ = false;
    server_->stop();

    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  void setup_server() {
    server_->set_pre_routing_handler(
        [this](const httplib::Request& req, httplib::Response& res) {
          // Apply response delay
          if (response_delay_ms_ > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(response_delay_ms_));
          }

          // Simulate random failures
          if (failure_rate_ > 0.0) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            if (dist(rng_) < failure_rate_) {
              res.status = 500;
              res.set_content("Simulated server error", "text/plain");
              return httplib::Server::HandlerResponse::Handled;
            }
          }

          return httplib::Server::HandlerResponse::Unhandled;
        });

    // Set up catch-all routes for common HTTP methods
    auto catch_all_handler = [this](const httplib::Request& req,
                                    httplib::Response& res) {
      // Record request
      HttpRequest request;
      request.method = req.method;
      request.path = req.path;
      for (const auto& header : req.headers) {
        request.headers[header.first] = header.second;
      }
      request.body = req.body;

      {
        std::lock_guard<std::mutex> lock(history_mutex_);
        request_history_.push_back(request);
      }

      // Find handler
      std::string route_key = request.method + ":" + request.path;
      auto it = routes_.find(route_key);

      HttpResponse response;
      if (it != routes_.end()) {
        response = it->second(request);
      } else if (default_handler_) {
        response = default_handler_(request);
      } else {
        response = HttpResponse::not_found();
      }

      // Apply response
      res.status = response.status_code;
      for (const auto& header : response.headers) {
        res.set_header(header.first, header.second);
      }
      res.set_content(response.body, response.headers.count("Content-Type")
                                         ? response.headers.at("Content-Type")
                                         : "text/plain");
    };

    // Set up catch-all routes for common HTTP methods
    server_->Get(".*", catch_all_handler);
    server_->Post(".*", catch_all_handler);
    server_->Put(".*", catch_all_handler);
    server_->Delete(".*", catch_all_handler);
  }
};

// MockHttpServer implementation
MockHttpServer::MockHttpServer(int port)
    : impl_(std::make_unique<Impl>(port)) {}

MockHttpServer::~MockHttpServer() = default;

bool MockHttpServer::start() {
  return impl_->start();
}

void MockHttpServer::stop() {
  impl_->stop();
}

bool MockHttpServer::is_running() const {
  return impl_->running_;
}

int MockHttpServer::get_port() const {
  return impl_->port_;
}

std::string MockHttpServer::get_base_url() const {
  return "http://localhost:" + std::to_string(get_port());
}

void MockHttpServer::set_default_handler(RequestHandler handler) {
  impl_->default_handler_ = handler;
}

void MockHttpServer::add_route(const std::string& method,
                               const std::string& path,
                               RequestHandler handler) {
  std::string key = method + ":" + path;
  impl_->routes_[key] = handler;
}

void MockHttpServer::clear_routes() {
  impl_->routes_.clear();
}

void MockHttpServer::setup_openai_routes() {
  add_route("POST", "/v1/chat/completions", [](const HttpRequest& req) {
    // Default successful response
    return OpenAIResponseBuilder::chat_completion(
        "Hello! How can I help you today?");
  });

  add_route("GET", "/v1/models", [](const HttpRequest& req) {
    nlohmann::json models = {
        {"object", "list"},
        {"data",
         nlohmann::json::array(
             {{{"id", "gpt-4o"}, {"object", "model"}, {"owned_by", "openai"}},
              {{"id", "gpt-4o-mini"},
               {"object", "model"},
               {"owned_by", "openai"}}})}};
    return HttpResponse::json(models.dump());
  });
}

void MockHttpServer::set_chat_completion_response(const std::string& response) {
  add_route("POST", "/v1/chat/completions", [response](const HttpRequest& req) {
    return HttpResponse::json(response);
  });
}

void MockHttpServer::set_streaming_response(
    const std::vector<std::string>& events) {
  add_route("POST", "/v1/chat/completions", [events](const HttpRequest& req) {
    HttpResponse response;
    response.status_code = 200;
    response.headers["Content-Type"] = "text/event-stream";
    response.headers["Cache-Control"] = "no-cache";
    response.headers["Connection"] = "keep-alive";

    std::ostringstream oss;
    for (const auto& event : events) {
      oss << event;
    }
    response.body = oss.str();

    return response;
  });
}

void MockHttpServer::simulate_error(int status_code,
                                    const std::string& message) {
  set_default_handler([status_code, message](const HttpRequest& req) {
    return HttpResponse::error(status_code, message);
  });
}

void MockHttpServer::simulate_timeout(int milliseconds) {
  set_response_delay(milliseconds);
}

void MockHttpServer::simulate_rate_limit() {
  set_default_handler([](const HttpRequest& req) {
    return OpenAIResponseBuilder::rate_limit_exceeded();
  });
}

std::vector<HttpRequest> MockHttpServer::get_received_requests() const {
  std::lock_guard<std::mutex> lock(impl_->history_mutex_);
  return impl_->request_history_;
}

void MockHttpServer::clear_request_history() {
  std::lock_guard<std::mutex> lock(impl_->history_mutex_);
  impl_->request_history_.clear();
}

int MockHttpServer::get_request_count() const {
  std::lock_guard<std::mutex> lock(impl_->history_mutex_);
  return static_cast<int>(impl_->request_history_.size());
}

HttpRequest MockHttpServer::get_last_request() const {
  std::lock_guard<std::mutex> lock(impl_->history_mutex_);
  return impl_->request_history_.empty() ? HttpRequest{}
                                         : impl_->request_history_.back();
}

void MockHttpServer::set_response_delay(int milliseconds) {
  impl_->response_delay_ms_ = milliseconds;
}

void MockHttpServer::set_random_failures(double failure_rate) {
  impl_->failure_rate_ = std::clamp(failure_rate, 0.0, 1.0);
}

void MockHttpServer::reset_behavior() {
  impl_->response_delay_ms_ = 0;
  impl_->failure_rate_ = 0.0;
  clear_routes();
  set_default_handler(nullptr);
}

// OpenAIResponseBuilder implementation
HttpResponse OpenAIResponseBuilder::chat_completion(const std::string& content,
                                                    const std::string& model,
                                                    int prompt_tokens,
                                                    int completion_tokens) {
  nlohmann::json response = {
      {"id", "chatcmpl-test123"},
      {"object", "chat.completion"},
      {"created", 1234567890},
      {"model", model},
      {"system_fingerprint", "fp_test123"},
      {"choices",
       nlohmann::json::array(
           {{{"index", 0},
             {"message", {{"role", "assistant"}, {"content", content}}},
             {"finish_reason", "stop"},
             {"logprobs", nullptr}}})},
      {"usage",
       {{"prompt_tokens", prompt_tokens},
        {"completion_tokens", completion_tokens},
        {"total_tokens", prompt_tokens + completion_tokens}}}};

  return HttpResponse::json(response.dump());
}

HttpResponse OpenAIResponseBuilder::streaming_chunk(const std::string& content,
                                                    const std::string& id,
                                                    bool is_final) {
  nlohmann::json chunk = {
      {"id", id},
      {"object", "chat.completion.chunk"},
      {"created", 1234567890},
      {"model", "gpt-4o"},
      {"choices",
       nlohmann::json::array(
           {{{"index", 0},
             {"delta", is_final ? nlohmann::json{}
                                : nlohmann::json{{"content", content}}},
             {"finish_reason", is_final ? "stop" : nullptr}}})}};

  return HttpResponse::ok("data: " + chunk.dump() + "\n\n");
}

HttpResponse OpenAIResponseBuilder::streaming_done() {
  return HttpResponse::ok("data: [DONE]\n\n");
}

HttpResponse OpenAIResponseBuilder::invalid_api_key() {
  nlohmann::json error = {{"error",
                           {{"message", "Incorrect API key provided"},
                            {"type", "invalid_request_error"},
                            {"param", nullptr},
                            {"code", "invalid_api_key"}}}};

  HttpResponse response = HttpResponse::json(error.dump());
  response.status_code = 401;
  return response;
}

HttpResponse OpenAIResponseBuilder::model_not_found(const std::string& model) {
  nlohmann::json error = {
      {"error",
       {{"message", "The model '" + model + "' does not exist"},
        {"type", "invalid_request_error"},
        {"param", "model"},
        {"code", "model_not_found"}}}};

  HttpResponse response = HttpResponse::json(error.dump());
  response.status_code = 404;
  return response;
}

HttpResponse OpenAIResponseBuilder::rate_limit_exceeded() {
  nlohmann::json error = {{"error",
                           {{"message", "Rate limit reached for requests"},
                            {"type", "rate_limit_exceeded"},
                            {"param", nullptr},
                            {"code", "rate_limit_exceeded"}}}};

  HttpResponse response = HttpResponse::json(error.dump());
  response.status_code = 429;
  return response;
}

HttpResponse OpenAIResponseBuilder::invalid_request(
    const std::string& message) {
  nlohmann::json error = {{"error",
                           {{"message", message},
                            {"type", "invalid_request_error"},
                            {"param", nullptr},
                            {"code", nullptr}}}};

  HttpResponse response = HttpResponse::json(error.dump());
  response.status_code = 400;
  return response;
}

HttpResponse OpenAIResponseBuilder::server_error(const std::string& message) {
  nlohmann::json error = {{"error",
                           {{"message", message},
                            {"type", "server_error"},
                            {"param", nullptr},
                            {"code", nullptr}}}};

  HttpResponse response = HttpResponse::json(error.dump());
  response.status_code = 500;
  return response;
}

HttpResponse OpenAIResponseBuilder::malformed_json() {
  return HttpResponse::ok(
      "{\"id\":\"incomplete\",\"choices\":[{\"message\":{\"content\":\"test");
}

HttpResponse OpenAIResponseBuilder::incomplete_response() {
  nlohmann::json partial = {
      {"id", "test"},
      {"choices", nlohmann::json::array()}  // Missing message content
  };

  return HttpResponse::json(partial.dump());
}

HttpResponse OpenAIResponseBuilder::missing_fields() {
  nlohmann::json minimal = {
      {"choices",
       nlohmann::json::array({{{"message", {{"content", "response"}}}}})}
      // Missing id, model, usage, etc.
  };

  return HttpResponse::json(minimal.dump());
}

// TestServerScope implementation
TestServerScope::TestServerScope(int port) {
  server_ = std::make_unique<MockHttpServer>(port);
  if (!server_->start()) {
    throw std::runtime_error("Failed to start test server");
  }
}

TestServerScope::~TestServerScope() {
  if (server_) {
    server_->stop();
  }
}

MockHttpServer& TestServerScope::server() {
  return *server_;
}

std::string TestServerScope::base_url() const {
  return server_->get_base_url();
}

// NetworkConditionSimulator implementation
void NetworkConditionSimulator::apply_condition(MockHttpServer& server,
                                                Condition condition) {
  switch (condition) {
    case Condition::kNormal:
      server.reset_behavior();
      break;
    case Condition::kSlow:
      server.set_response_delay(2000);  // 2 second delay
      break;
    case Condition::kUnstable:
      server.set_random_failures(0.3);  // 30% failure rate
      break;
    case Condition::kTimeout:
      server.set_response_delay(60000);  // 60 second delay (timeout)
      break;
    case Condition::kConnectionRefused:
      server.simulate_error(0, "Connection refused");
      break;
    case Condition::kDnsFailure:
      server.simulate_error(-1, "DNS resolution failed");
      break;
  }
}

void NetworkConditionSimulator::simulate_packet_loss(MockHttpServer& server,
                                                     double loss_rate) {
  server.set_random_failures(loss_rate);
}

void NetworkConditionSimulator::simulate_bandwidth_limit(MockHttpServer& server,
                                                         int bytes_per_second) {
  // Simplified simulation - add delay based on response size
  int delay_per_kb = 1000 / (bytes_per_second / 1024);  // ms per KB
  server.set_response_delay(delay_per_kb);
}

}  // namespace test
}  // namespace ai