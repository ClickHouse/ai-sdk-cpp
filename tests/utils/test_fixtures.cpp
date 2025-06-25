#include "test_fixtures.h"

#include "ai/types/enums.h"

#include <gmock/gmock.h>

namespace ai {
namespace test {

// AITestFixture implementation
void AITestFixture::SetUp() {
  // Common setup for all tests
}

void AITestFixture::TearDown() {
  // Common cleanup
}

// OpenAITestFixture implementation
void OpenAITestFixture::SetUp() {
  AITestFixture::SetUp();
}

GenerateOptions OpenAITestFixture::createBasicOptions() {
  return GenerateOptions(kTestModel, kTestPrompt);
}

GenerateOptions OpenAITestFixture::createAdvancedOptions() {
  GenerateOptions options(kTestModel, "System prompt", kTestPrompt);
  options.temperature = 0.7;
  options.max_tokens = 100;
  options.top_p = 0.9;
  options.frequency_penalty = 0.1;
  options.presence_penalty = 0.2;
  options.seed = 42;
  return options;
}

nlohmann::json OpenAITestFixture::createValidChatCompletionResponse() {
  return nlohmann::json{
      {"id", "chatcmpl-test123"},
      {"object", "chat.completion"},
      {"created", 1234567890},
      {"model", kTestModel},
      {"system_fingerprint", "fp_test123"},
      {"choices", nlohmann::json::array(
                      {{{"index", 0},
                        {"message",
                         {{"role", "assistant"},
                          {"content", "Hello! How can I help you today?"}}},
                        {"finish_reason", "stop"}}})},
      {"usage",
       {{"prompt_tokens", 10},
        {"completion_tokens", 20},
        {"total_tokens", 30}}}};
}

nlohmann::json OpenAITestFixture::createErrorResponse(
    int status_code,
    const std::string& message) {
  return nlohmann::json{{"error",
                         {{"message", message},
                          {"type", "invalid_request_error"},
                          {"param", nullptr},
                          {"code", nullptr}}}};
}

nlohmann::json OpenAITestFixture::createStreamResponse() {
  return nlohmann::json{
      {"id", "chatcmpl-stream123"},
      {"object", "chat.completion.chunk"},
      {"created", 1234567890},
      {"model", kTestModel},
      {"choices", nlohmann::json::array({{{"index", 0},
                                          {"delta", {{"content", "Hello"}}},
                                          {"finish_reason", nullptr}}})}};
}

Messages OpenAITestFixture::createSampleConversation() {
  return {createSystemMessage("You are a helpful assistant."),
          createUserMessage("Hello!"),
          createAssistantMessage("Hi there! How can I help you?"),
          createUserMessage("What's the weather like?")};
}

Message OpenAITestFixture::createUserMessage(const std::string& content) {
  return Message(kMessageRoleUser, content);
}

Message OpenAITestFixture::createAssistantMessage(const std::string& content) {
  return Message(kMessageRoleAssistant, content);
}

Message OpenAITestFixture::createSystemMessage(const std::string& content) {
  return Message(kMessageRoleSystem, content);
}

// TestDataGenerator implementation
std::vector<GenerateOptions> TestDataGenerator::generateOptionsVariations() {
  std::vector<GenerateOptions> variations;

  // Basic prompt
  variations.emplace_back("gpt-4o", "Simple test");

  // With system prompt
  variations.emplace_back("gpt-4o", "You are helpful", "User question");

  // With messages
  Messages msgs = {Message(kMessageRoleUser, "Hello")};
  variations.emplace_back("gpt-4o", std::move(msgs));

  // With all parameters
  GenerateOptions full("gpt-4o", "Test prompt");
  full.temperature = 0.5;
  full.max_tokens = 50;
  full.top_p = 0.8;
  full.frequency_penalty = -0.5;
  full.presence_penalty = 0.3;
  full.seed = 123;
  variations.push_back(std::move(full));

  return variations;
}

nlohmann::json TestDataGenerator::createMinimalValidResponse() {
  return nlohmann::json{
      {"id", "test"},
      {"choices",
       nlohmann::json::array({{{"message", {{"content", "Response"}}},
                               {"finish_reason", "stop"}}})}};
}

nlohmann::json TestDataGenerator::createFullValidResponse() {
  return nlohmann::json{
      {"id", "chatcmpl-full123"},
      {"object", "chat.completion"},
      {"created", 1234567890},
      {"model", "gpt-4o"},
      {"system_fingerprint", "fp_full123"},
      {"choices",
       nlohmann::json::array(
           {{{"index", 0},
             {"message",
              {{"role", "assistant"},
               {"content", "This is a complete response with all fields."}}},
             {"finish_reason", "stop"},
             {"logprobs", nullptr}}})},
      {"usage",
       {{"prompt_tokens", 25},
        {"completion_tokens", 45},
        {"total_tokens", 70}}}};
}

nlohmann::json TestDataGenerator::createResponseWithUsage(
    int prompt_tokens,
    int completion_tokens) {
  auto response = createMinimalValidResponse();
  response["usage"] = {{"prompt_tokens", prompt_tokens},
                       {"completion_tokens", completion_tokens},
                       {"total_tokens", prompt_tokens + completion_tokens}};
  return response;
}

nlohmann::json TestDataGenerator::createResponseWithMetadata() {
  auto response = createFullValidResponse();
  response["custom_field"] = "custom_value";
  response["choices"][0]["custom_choice_field"] = true;
  return response;
}

std::vector<std::pair<int, std::string>> TestDataGenerator::errorScenarios() {
  return {{400, "Bad request - invalid parameters"},
          {401, "Unauthorized - invalid API key"},
          {403, "Forbidden - insufficient permissions"},
          {404, "Not found - invalid endpoint"},
          {429, "Rate limit exceeded"},
          {500, "Internal server error"},
          {502, "Bad gateway"},
          {503, "Service unavailable"}};
}

std::vector<std::string> TestDataGenerator::createStreamingEvents() {
  return {
      "data: "
      "{\"id\":\"chatcmpl-stream1\",\"object\":\"chat.completion.chunk\","
      "\"created\":1234567890,\"model\":\"gpt-4o\",\"choices\":[{\"index\":0,"
      "\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}\n\n",
      "data: "
      "{\"id\":\"chatcmpl-stream1\",\"object\":\"chat.completion.chunk\","
      "\"created\":1234567890,\"model\":\"gpt-4o\",\"choices\":[{\"index\":0,"
      "\"delta\":{\"content\":\" world\"},\"finish_reason\":null}]}\n\n",
      "data: "
      "{\"id\":\"chatcmpl-stream1\",\"object\":\"chat.completion.chunk\","
      "\"created\":1234567890,\"model\":\"gpt-4o\",\"choices\":[{\"index\":0,"
      "\"delta\":{\"content\":\"!\"},\"finish_reason\":\"stop\"}]}\n\n",
      "data: [DONE]\n\n"};
}

GenerateOptions TestDataGenerator::createEdgeCaseOptions() {
  GenerateOptions options("gpt-4o", "");  // Empty prompt
  options.temperature = 2.0;              // Maximum temperature
  options.max_tokens = 1;                 // Minimum tokens
  options.top_p = 1.0;                    // Maximum top_p
  options.frequency_penalty = 2.0;        // Maximum penalty
  options.presence_penalty = -2.0;        // Minimum penalty
  return options;
}

std::string TestDataGenerator::createLargePrompt(size_t size) {
  std::string large_prompt;
  large_prompt.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    large_prompt += (i % 100 == 0) ? '\n' : 'a';
  }
  return large_prompt;
}

nlohmann::json TestDataGenerator::createMalformedResponse() {
  return nlohmann::json{
      {"id", 123},                  // Should be string
      {"choices", "not_an_array"},  // Should be array
      {"usage",
       {
           {"prompt_tokens", "not_a_number"}  // Should be number
       }}};
}

// TestAssertions implementation
void TestAssertions::assertSuccess(const GenerateResult& result) {
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.text.empty())
      << "Expected non-empty text in successful result";
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";
}

void TestAssertions::assertError(const GenerateResult& result,
                                 const std::string& expected_error) {
  EXPECT_FALSE(result.is_success()) << "Expected error result but got success";
  EXPECT_TRUE(result.error.has_value())
      << "Expected error message in error result";
  if (!expected_error.empty()) {
    EXPECT_THAT(result.error_message(), testing::HasSubstr(expected_error))
        << "Error message doesn't contain expected text";
  }
}

void TestAssertions::assertUsage(const Usage& usage,
                                 int expected_prompt,
                                 int expected_completion) {
  EXPECT_EQ(usage.prompt_tokens, expected_prompt)
      << "Unexpected prompt token count";
  EXPECT_EQ(usage.completion_tokens, expected_completion)
      << "Unexpected completion token count";
  EXPECT_EQ(usage.total_tokens, expected_prompt + expected_completion)
      << "Total tokens should equal sum";
}

void TestAssertions::assertValidRequestJson(const nlohmann::json& request) {
  EXPECT_TRUE(request.contains("model")) << "Request missing model field";
  EXPECT_TRUE(request.contains("messages")) << "Request missing messages field";
  EXPECT_TRUE(request["messages"].is_array()) << "Messages should be array";
  EXPECT_FALSE(request["messages"].empty())
      << "Messages array should not be empty";
}

void TestAssertions::assertHasRequiredFields(
    const nlohmann::json& json,
    const std::vector<std::string>& fields) {
  for (const auto& field : fields) {
    EXPECT_TRUE(json.contains(field))
        << "JSON missing required field: " << field;
  }
}

}  // namespace test
}  // namespace ai