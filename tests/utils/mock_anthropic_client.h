#pragma once

#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"
#include "ai/types/stream_result.h"

#include <functional>
#include <map>
#include <string>

#include <gmock/gmock.h>

namespace ai {
namespace test {

// Mock HTTP client for testing Anthropic network interactions
class MockAnthropicHTTPClient {
 public:
  virtual ~MockAnthropicHTTPClient() = default;
  virtual bool post(const std::string& url,
                    const std::map<std::string, std::string>& headers,
                    const std::string& body,
                    std::string& response_body,
                    int& status_code) = 0;

  virtual bool stream_post(
      const std::string& url,
      const std::map<std::string, std::string>& headers,
      const std::string& body,
      const std::function<void(const std::string&)>& callback) = 0;
};

// Controllable Anthropic client for testing
class ControllableAnthropicClient {
 private:
  std::string predefined_response_;
  int predefined_status_code_;
  bool should_fail_;
  bool should_timeout_;

 public:
  explicit ControllableAnthropicClient(
      const std::string& api_key = "sk-ant-test-key",
      const std::string& base_url = "https://api.anthropic.com");

  // Control behavior
  void setPredefinedResponse(const std::string& response,
                             int status_code = 200);
  void setShouldFail(bool fail);
  void setShouldTimeout(bool timeout);

  // Reset to default behavior
  void reset();

  // Simulate Anthropic client methods
  GenerateResult generate_text(const GenerateOptions& options);
  StreamResult stream_text(const StreamOptions& options);

  // Configuration methods (same interface as real client)
  bool is_valid() const;
  std::string provider_name() const;
  std::vector<std::string> supported_models() const;
  bool supports_model(const std::string& model_name) const;
  std::string config_info() const;

  // Test inspection methods
  GenerateOptions getLastGenerateOptions() const;
  GenerateOptions getLastStreamOptions() const;
  int getCallCount() const;

 private:
  mutable GenerateOptions last_generate_options_;
  mutable int call_count_;
};

// Test doubles for Anthropic network failures
class AnthropicNetworkFailureSimulator {
 public:
  enum class FailureType {
    kConnectionTimeout,
    kReadTimeout,
    kConnectionRefused,
    kDnsFailure,
    kSslError,
    kUnexpectedDisconnect
  };

  static std::string simulateFailure(FailureType type);
  static GenerateResult createNetworkErrorResult(FailureType type);
};

// Response builders for Anthropic test scenarios
class AnthropicResponseBuilder {
 public:
  static std::string buildSuccessResponse(
      const std::string& content = "Test response",
      const std::string& model = "claude-3-5-sonnet-20241022",
      int input_tokens = 10,
      int output_tokens = 20);

  static std::string buildErrorResponse(
      int status_code,
      const std::string& error_type = "invalid_request_error",
      const std::string& message = "Test error");

  static std::string buildPartialResponse();  // For testing malformed responses

  static std::vector<std::string> buildStreamingResponse(
      const std::string& content = "Hello world!");

  static std::string buildRateLimitResponse();
  static std::string buildAuthErrorResponse();
  static std::string buildModelNotFoundResponse();
  static std::string buildMaxTokensRequiredResponse();
};

}  // namespace test
}  // namespace ai