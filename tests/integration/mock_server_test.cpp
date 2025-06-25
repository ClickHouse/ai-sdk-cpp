#include "../utils/mock_http_server.h"
#include "../utils/mock_openai_client.h"
#include "../utils/test_fixtures.h"
#include "ai/errors.h"
#include "ai/types/generate_options.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

class MockServerTest : public OpenAITestFixture {
 protected:
  void SetUp() override {
    OpenAITestFixture::SetUp();

    server_scope_ = std::make_unique<TestServerScope>();
    server_scope_->server().setup_openai_routes();
  }

  void TearDown() override {
    server_scope_.reset();
    OpenAITestFixture::TearDown();
  }

  std::unique_ptr<TestServerScope> server_scope_;
};

// Basic HTTP Server Tests
TEST_F(MockServerTest, ServerStartsAndStops) {
  EXPECT_TRUE(server_scope_->server().is_running());
  EXPECT_GT(server_scope_->server().get_port(), 0);
  EXPECT_FALSE(server_scope_->base_url().empty());
}

TEST_F(MockServerTest, ServerRecordsRequests) {
  auto& server = server_scope_->server();

  // Initially no requests
  EXPECT_EQ(server.get_request_count(), 0);

  // Simulate making requests by directly calling the server
  // In a real test, you'd use an HTTP client to make requests
  server.clear_request_history();
  EXPECT_EQ(server.get_request_count(), 0);
}

// OpenAI API Simulation Tests
TEST_F(MockServerTest, ChatCompletionSuccess) {
  auto& server = server_scope_->server();

  // Set up successful response
  std::string response_json = ResponseBuilder::buildSuccessResponse(
      "Hello! How can I help you today?", "gpt-4o", 15, 25);
  server.set_chat_completion_response(response_json);

  // In a real test, you'd create an OpenAI client pointing to the mock server
  // and make an actual HTTP request. For now, we'll test the response builder
  auto parsed_response = nlohmann::json::parse(response_json);

  EXPECT_EQ(parsed_response["choices"][0]["message"]["content"],
            "Hello! How can I help you today?");
  EXPECT_EQ(parsed_response["model"], "gpt-4o");
  EXPECT_EQ(parsed_response["usage"]["prompt_tokens"], 15);
  EXPECT_EQ(parsed_response["usage"]["completion_tokens"], 25);
}

TEST_F(MockServerTest, ChatCompletionWithCustomContent) {
  auto& server = server_scope_->server();

  std::string custom_content =
      "I am a helpful AI assistant specialized in coding.";
  auto response =
      OpenAIResponseBuilder::chat_completion(custom_content, "gpt-4", 20, 30);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_THAT(response.headers,
              testing::Contains(testing::Key("Content-Type")));

  auto parsed = nlohmann::json::parse(response.body);
  EXPECT_EQ(parsed["choices"][0]["message"]["content"], custom_content);
  EXPECT_EQ(parsed["model"], "gpt-4");
}

// Error Response Tests
TEST_F(MockServerTest, InvalidApiKeyError) {
  auto& server = server_scope_->server();

  auto error_response = OpenAIResponseBuilder::invalid_api_key();

  EXPECT_EQ(error_response.status_code, 401);

  auto parsed = nlohmann::json::parse(error_response.body);
  EXPECT_EQ(parsed["error"]["type"], "invalid_request_error");
  EXPECT_THAT(std::string(parsed["error"]["message"]),
              testing::HasSubstr("API key"));
}

TEST_F(MockServerTest, ModelNotFoundError) {
  auto& server = server_scope_->server();

  auto error_response =
      OpenAIResponseBuilder::model_not_found("invalid-model-123");

  EXPECT_EQ(error_response.status_code, 404);

  auto parsed = nlohmann::json::parse(error_response.body);
  EXPECT_EQ(parsed["error"]["type"], "invalid_request_error");
  EXPECT_THAT(std::string(parsed["error"]["message"]),
              testing::HasSubstr("invalid-model-123"));
}

TEST_F(MockServerTest, RateLimitError) {
  auto& server = server_scope_->server();

  auto error_response = OpenAIResponseBuilder::rate_limit_exceeded();

  EXPECT_EQ(error_response.status_code, 429);

  auto parsed = nlohmann::json::parse(error_response.body);
  EXPECT_EQ(parsed["error"]["type"], "rate_limit_exceeded");
}

TEST_F(MockServerTest, ServerError) {
  auto& server = server_scope_->server();

  auto error_response =
      OpenAIResponseBuilder::server_error("Database connection failed");

  EXPECT_EQ(error_response.status_code, 500);

  auto parsed = nlohmann::json::parse(error_response.body);
  EXPECT_EQ(parsed["error"]["type"], "server_error");
  EXPECT_EQ(parsed["error"]["message"], "Database connection failed");
}

// Malformed Response Tests
TEST_F(MockServerTest, MalformedJsonResponse) {
  auto& server = server_scope_->server();

  auto malformed = OpenAIResponseBuilder::malformed_json();

  EXPECT_EQ(malformed.status_code, 200);

  // Should fail to parse as valid JSON
  EXPECT_THROW(nlohmann::json::parse(malformed.body),
               nlohmann::json::parse_error);
}

TEST_F(MockServerTest, IncompleteResponse) {
  auto& server = server_scope_->server();

  auto incomplete = OpenAIResponseBuilder::incomplete_response();

  EXPECT_EQ(incomplete.status_code, 200);

  auto parsed = nlohmann::json::parse(incomplete.body);
  EXPECT_TRUE(parsed.contains("id"));
  EXPECT_TRUE(parsed.contains("choices"));
  EXPECT_TRUE(parsed["choices"].is_array());
  EXPECT_TRUE(parsed["choices"].empty());  // Missing actual content
}

TEST_F(MockServerTest, MissingFieldsResponse) {
  auto& server = server_scope_->server();

  auto missing_fields = OpenAIResponseBuilder::missing_fields();

  EXPECT_EQ(missing_fields.status_code, 200);

  auto parsed = nlohmann::json::parse(missing_fields.body);
  EXPECT_FALSE(parsed.contains("id"));      // Missing ID
  EXPECT_FALSE(parsed.contains("model"));   // Missing model
  EXPECT_FALSE(parsed.contains("usage"));   // Missing usage
  EXPECT_TRUE(parsed.contains("choices"));  // Has choices
}

// Streaming Response Tests
TEST_F(MockServerTest, StreamingResponse) {
  auto& server = server_scope_->server();

  std::vector<std::string> events = {
      "data: " +
          OpenAIResponseBuilder::streaming_chunk("Hello", "chatcmpl-stream1")
              .body,
      "data: " +
          OpenAIResponseBuilder::streaming_chunk(" world", "chatcmpl-stream1")
              .body,
      "data: " +
          OpenAIResponseBuilder::streaming_chunk("!", "chatcmpl-stream1", true)
              .body,
      OpenAIResponseBuilder::streaming_done().body};

  server.set_streaming_response(events);

  // Verify events are well-formed
  for (const auto& event : events) {
    if (event.find("[DONE]") == std::string::npos) {
      EXPECT_THAT(event, testing::StartsWith("data:"));
    }
  }
}

TEST_F(MockServerTest, StreamingChunkStructure) {
  auto chunk =
      OpenAIResponseBuilder::streaming_chunk("test content", "chatcmpl-123");

  EXPECT_EQ(chunk.status_code, 200);

  // Extract JSON from SSE format
  std::string json_part = chunk.body.substr(5);  // Remove "data: "
  json_part =
      json_part.substr(0, json_part.find("\n\n"));  // Remove trailing newlines

  auto parsed = nlohmann::json::parse(json_part);
  EXPECT_EQ(parsed["id"], "chatcmpl-123");
  EXPECT_EQ(parsed["object"], "chat.completion.chunk");
  EXPECT_EQ(parsed["choices"][0]["delta"]["content"], "test content");
  EXPECT_EQ(parsed["choices"][0]["finish_reason"], nullptr);
}

TEST_F(MockServerTest, StreamingFinalChunk) {
  auto final_chunk =
      OpenAIResponseBuilder::streaming_chunk("", "chatcmpl-123", true);

  std::string json_part = final_chunk.body.substr(5);
  json_part = json_part.substr(0, json_part.find("\n\n"));

  auto parsed = nlohmann::json::parse(json_part);
  EXPECT_EQ(parsed["choices"][0]["finish_reason"], "stop");
  EXPECT_TRUE(parsed["choices"][0]["delta"].empty() ||
              !parsed["choices"][0]["delta"].contains("content"));
}

// Network Condition Simulation Tests
TEST_F(MockServerTest, SlowNetworkCondition) {
  auto& server = server_scope_->server();

  NetworkConditionSimulator::apply_condition(
      server, NetworkConditionSimulator::Condition::kSlow);

  // In a real test, you'd measure response time
  // For now, just verify the condition was applied
  // (The delay would be observed when making actual HTTP requests)
}

TEST_F(MockServerTest, UnstableNetworkCondition) {
  auto& server = server_scope_->server();

  NetworkConditionSimulator::apply_condition(
      server, NetworkConditionSimulator::Condition::kUnstable);

  // With unstable conditions, some requests would fail randomly
  // In a real test, you'd make multiple requests and verify some fail
}

TEST_F(MockServerTest, PacketLossSimulation) {
  auto& server = server_scope_->server();

  NetworkConditionSimulator::simulate_packet_loss(server, 0.1);  // 10% loss

  // Would cause random failures in real HTTP requests
}

// Server Behavior Tests
TEST_F(MockServerTest, ResponseDelaySimulation) {
  auto& server = server_scope_->server();

  server.set_response_delay(1000);  // 1 second delay

  // In a real test, you'd measure that responses take at least 1 second
}

TEST_F(MockServerTest, RandomFailureSimulation) {
  auto& server = server_scope_->server();

  server.set_random_failures(0.5);  // 50% failure rate

  // In a real test, you'd make multiple requests and verify roughly half fail
}

TEST_F(MockServerTest, BehaviorReset) {
  auto& server = server_scope_->server();

  // Apply various behaviors
  server.set_response_delay(2000);
  server.set_random_failures(0.3);
  server.simulate_error(500, "Test error");

  // Reset should clear all behaviors
  server.reset_behavior();

  // In a real test, you'd verify that requests now behave normally
}

// Route Handling Tests
TEST_F(MockServerTest, CustomRouteHandling) {
  auto& server = server_scope_->server();

  server.add_route("GET", "/health", [](const HttpRequest& req) {
    return HttpResponse::json("{\"status\": \"healthy\"}");
  });

  // In a real test, you'd make a GET request to /health and verify the response
}

TEST_F(MockServerTest, DefaultHandlerFallback) {
  auto& server = server_scope_->server();

  server.set_default_handler([](const HttpRequest& req) {
    return HttpResponse::error(404, "Custom 404: " + req.path + " not found");
  });

  // Requests to non-existent routes would use the default handler
}

// Request History Tests
TEST_F(MockServerTest, RequestHistoryTracking) {
  auto& server = server_scope_->server();

  // Initially no requests
  EXPECT_EQ(server.get_request_count(), 0);

  // Clear history should work even when empty
  server.clear_request_history();
  EXPECT_EQ(server.get_request_count(), 0);

  // In a real test, you'd make requests and verify they're recorded
}

// HTTP Request/Response Helper Tests
class HttpHelpersTest : public ::testing::Test {};

TEST_F(HttpHelpersTest, HttpRequestHelpers) {
  HttpRequest req;
  req.headers["Content-Type"] = "application/json";
  req.headers["Authorization"] = "Bearer sk-test123";

  EXPECT_TRUE(req.has_header("Content-Type"));
  EXPECT_FALSE(req.has_header("X-Custom-Header"));
  EXPECT_EQ(req.get_header("Authorization"), "Bearer sk-test123");
  EXPECT_EQ(req.get_header("Missing"), "");
  EXPECT_TRUE(req.is_json());
}

TEST_F(HttpHelpersTest, HttpResponseHelpers) {
  auto response = HttpResponse::ok("Test body");
  EXPECT_EQ(response.status_code, 200);
  EXPECT_EQ(response.body, "Test body");

  auto json_response = HttpResponse::json("{\"test\": true}");
  EXPECT_EQ(json_response.status_code, 200);
  EXPECT_EQ(json_response.headers["Content-Type"], "application/json");

  auto error_response = HttpResponse::error(400, "Bad request");
  EXPECT_EQ(error_response.status_code, 400);
  EXPECT_EQ(error_response.body, "Bad request");
}

TEST_F(HttpHelpersTest, HttpResponseConvenienceMethods) {
  auto not_found = HttpResponse::not_found();
  EXPECT_EQ(not_found.status_code, 404);

  auto unauthorized = HttpResponse::unauthorized();
  EXPECT_EQ(unauthorized.status_code, 401);

  auto rate_limited = HttpResponse::rate_limited();
  EXPECT_EQ(rate_limited.status_code, 429);

  auto server_error = HttpResponse::server_error();
  EXPECT_EQ(server_error.status_code, 500);
}

}  // namespace test
}  // namespace ai