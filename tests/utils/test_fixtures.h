#pragma once

#include "ai/types/generate_options.h"
#include "ai/types/message.h"

#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace ai {
namespace test {

// Base test fixture with common setup
class AITestFixture : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Common test data
  static constexpr const char* kTestApiKey = "sk-test123456789";
  static constexpr const char* kTestModel = "gpt-4o";
  static constexpr const char* kTestPrompt = "Hello, world!";
  static constexpr const char* kTestBaseUrl = "https://api.openai.com";

  // Anthropic test data
  static constexpr const char* kTestAnthropicApiKey = "sk-ant-test123456789";
  static constexpr const char* kTestAnthropicModel =
      "claude-sonnet-4-5-20250929";
  static constexpr const char* kTestAnthropicBaseUrl =
      "https://api.anthropic.com";
};

// OpenAI specific test fixture
class OpenAITestFixture : public AITestFixture {
 protected:
  void SetUp() override;

  // Sample request/response data
  GenerateOptions createBasicOptions();
  GenerateOptions createAdvancedOptions();
  nlohmann::json createValidChatCompletionResponse();
  nlohmann::json createErrorResponse(int status_code,
                                     const std::string& message);
  nlohmann::json createStreamResponse();

  // Message builders
  Messages createSampleConversation();
  Message createUserMessage(const std::string& content);
  Message createAssistantMessage(const std::string& content);
  Message createSystemMessage(const std::string& content);
};

// Anthropic specific test fixture
class AnthropicTestFixture : public AITestFixture {
 protected:
  void SetUp() override;

  // Sample request/response data
  GenerateOptions createBasicAnthropicOptions();
  GenerateOptions createAdvancedAnthropicOptions();
  nlohmann::json createValidAnthropicResponse();
  nlohmann::json createAnthropicErrorResponse(int status_code,
                                              const std::string& message);
  nlohmann::json createAnthropicStreamResponse();

  // Message builders
  Messages createSampleAnthropicConversation();
  Message createAnthropicUserMessage(const std::string& content);
  Message createAnthropicAssistantMessage(const std::string& content);
  Message createAnthropicSystemMessage(const std::string& content);
};

// Test data generators
class TestDataGenerator {
 public:
  // Generate various GenerateOptions configurations
  static std::vector<GenerateOptions> generateOptionsVariations();

  // Generate OpenAI API responses
  static nlohmann::json createMinimalValidResponse();
  static nlohmann::json createFullValidResponse();
  static nlohmann::json createResponseWithUsage(int prompt_tokens,
                                                int completion_tokens);
  static nlohmann::json createResponseWithMetadata();

  // Generate error scenarios
  static std::vector<std::pair<int, std::string>> errorScenarios();

  // Generate streaming events
  static std::vector<std::string> createStreamingEvents();
  static std::vector<std::string> createAnthropicStreamingEvents();

  // Edge case data
  static GenerateOptions createEdgeCaseOptions();
  static std::string createLargePrompt(size_t size);
  static nlohmann::json createMalformedResponse();
};

// Assertion helpers
class TestAssertions {
 public:
  // Assert GenerateResult properties
  static void assertSuccess(const GenerateResult& result);
  static void assertError(const GenerateResult& result,
                          const std::string& expected_error = "");
  static void assertUsage(const Usage& usage,
                          int expected_prompt,
                          int expected_completion);

  // Assert JSON structure
  static void assertValidRequestJson(const nlohmann::json& request);
  static void assertHasRequiredFields(const nlohmann::json& json,
                                      const std::vector<std::string>& fields);

  // Assert OpenAI client state
  static void assertClientValid(const class OpenAIClient& client);
  static void assertModelSupported(const class OpenAIClient& client,
                                   const std::string& model);

  // Assert Anthropic client state
  static void assertAnthropicClientValid(const class AnthropicClient& client);
  static void assertAnthropicModelSupported(const class AnthropicClient& client,
                                            const std::string& model);
};

}  // namespace test
}  // namespace ai