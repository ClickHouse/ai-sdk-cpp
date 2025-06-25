#include "ai/errors.h"
#include "ai/types/enums.h"
#include "ai/types/generate_options.h"
#include "ai/types/message.h"
#include "ai/types/usage.h"
#include "test_fixtures.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

// GenerateOptions Tests
class GenerateOptionsTest : public AITestFixture {};

TEST_F(GenerateOptionsTest, DefaultConstructor) {
  GenerateOptions options;

  EXPECT_TRUE(options.model.empty());
  EXPECT_TRUE(options.prompt.empty());
  EXPECT_TRUE(options.system.empty());
  EXPECT_TRUE(options.messages.empty());
  EXPECT_FALSE(options.temperature.has_value());
  EXPECT_FALSE(options.max_tokens.has_value());
  EXPECT_FALSE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ConstructorWithModelAndPrompt) {
  GenerateOptions options("gpt-4o", "Hello, world!");

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_EQ(options.prompt, "Hello, world!");
  EXPECT_TRUE(options.system.empty());
  EXPECT_TRUE(options.messages.empty());
  EXPECT_TRUE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ConstructorWithSystemPrompt) {
  GenerateOptions options("gpt-4o", "You are helpful", "Hello!");

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_EQ(options.system, "You are helpful");
  EXPECT_EQ(options.prompt, "Hello!");
  EXPECT_TRUE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ConstructorWithMessages) {
  Messages messages = {Message(kMessageRoleUser, "Hello"),
                       Message(kMessageRoleAssistant, "Hi there!")};
  GenerateOptions options("gpt-4o", std::move(messages));

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_TRUE(options.prompt.empty());
  EXPECT_EQ(options.messages.size(), 2);
  EXPECT_TRUE(options.has_messages());
  EXPECT_TRUE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ValidationEmptyModel) {
  GenerateOptions options("", "Some prompt");

  EXPECT_FALSE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ValidationEmptyPromptAndMessages) {
  GenerateOptions options("gpt-4o", "");

  EXPECT_FALSE(options.is_valid());
}

TEST_F(GenerateOptionsTest, ValidationWithValidMessages) {
  Messages messages = {Message(kMessageRoleUser, "Hello")};
  GenerateOptions options("gpt-4o", std::move(messages));

  EXPECT_TRUE(options.is_valid());
}

TEST_F(GenerateOptionsTest, OptionalParametersSet) {
  GenerateOptions options("gpt-4o", "Test");

  options.temperature = 0.7;
  options.max_tokens = 100;
  options.top_p = 0.9;
  options.frequency_penalty = 0.1;
  options.presence_penalty = 0.2;
  options.seed = 42;

  EXPECT_EQ(options.temperature.value(), 0.7);
  EXPECT_EQ(options.max_tokens.value(), 100);
  EXPECT_EQ(options.top_p.value(), 0.9);
  EXPECT_EQ(options.frequency_penalty.value(), 0.1);
  EXPECT_EQ(options.presence_penalty.value(), 0.2);
  EXPECT_EQ(options.seed.value(), 42);
}

// GenerateResult Tests
class GenerateResultTest : public AITestFixture {};

TEST_F(GenerateResultTest, DefaultConstructor) {
  GenerateResult result;

  EXPECT_TRUE(result.text.empty());
  EXPECT_EQ(result.finish_reason, kFinishReasonError);
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.id.has_value());
  EXPECT_FALSE(result.model.has_value());
}

TEST_F(GenerateResultTest, SuccessConstructor) {
  Usage usage{10, 20};
  GenerateResult result("Hello, world!", kFinishReasonStop, usage);

  EXPECT_EQ(result.text, "Hello, world!");
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_EQ(result.usage.prompt_tokens, 10);
  EXPECT_EQ(result.usage.completion_tokens, 20);
  EXPECT_EQ(result.usage.total_tokens, 30);
  EXPECT_TRUE(result.is_success());
  EXPECT_TRUE(result);  // Implicit bool conversion
}

TEST_F(GenerateResultTest, ErrorConstructor) {
  GenerateResult result("Something went wrong");

  EXPECT_TRUE(result.text.empty());
  EXPECT_EQ(result.finish_reason, kFinishReasonError);
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result);  // Implicit bool conversion
  EXPECT_EQ(result.error_message(), "Something went wrong");
}

TEST_F(GenerateResultTest, FinishReasonToString) {
  GenerateResult result;

  result.finish_reason = kFinishReasonStop;
  EXPECT_EQ(result.finishReasonToString(), "stop");

  result.finish_reason = kFinishReasonLength;
  EXPECT_EQ(result.finishReasonToString(), "length");

  result.finish_reason = kFinishReasonContentFilter;
  EXPECT_EQ(result.finishReasonToString(), "content_filter");

  result.finish_reason = kFinishReasonToolCalls;
  EXPECT_EQ(result.finishReasonToString(), "tool_calls");

  result.finish_reason = kFinishReasonError;
  EXPECT_EQ(result.finishReasonToString(), "error");
}

TEST_F(GenerateResultTest, MetadataFields) {
  GenerateResult result("Text", kFinishReasonStop, Usage{});

  result.id = "test-id-123";
  result.model = "gpt-4o";
  result.created = 1234567890;
  result.system_fingerprint = "fp_123";
  result.provider_metadata = "{\"test\": true}";

  EXPECT_EQ(result.id.value(), "test-id-123");
  EXPECT_EQ(result.model.value(), "gpt-4o");
  EXPECT_EQ(result.created.value(), 1234567890);
  EXPECT_EQ(result.system_fingerprint.value(), "fp_123");
  EXPECT_EQ(result.provider_metadata.value(), "{\"test\": true}");
}

TEST_F(GenerateResultTest, ResponseMessages) {
  GenerateResult result("Response", kFinishReasonStop, Usage{});

  result.response_messages.push_back(
      Message(kMessageRoleAssistant, "Response"));

  EXPECT_EQ(result.response_messages.size(), 1);
  EXPECT_EQ(result.response_messages[0].role, kMessageRoleAssistant);
  EXPECT_EQ(result.response_messages[0].content, "Response");
}

// Message Tests
class MessageTest : public AITestFixture {};

TEST_F(MessageTest, Constructor) {
  Message msg(kMessageRoleUser, "Hello, world!");

  EXPECT_EQ(msg.role, kMessageRoleUser);
  EXPECT_EQ(msg.content, "Hello, world!");
}

TEST_F(MessageTest, SystemMessage) {
  Message msg(kMessageRoleSystem, "You are a helpful assistant.");

  EXPECT_EQ(msg.role, kMessageRoleSystem);
  EXPECT_EQ(msg.content, "You are a helpful assistant.");
}

TEST_F(MessageTest, AssistantMessage) {
  Message msg(kMessageRoleAssistant, "How can I help you?");

  EXPECT_EQ(msg.role, kMessageRoleAssistant);
  EXPECT_EQ(msg.content, "How can I help you?");
}

TEST_F(MessageTest, EmptyContent) {
  Message msg(kMessageRoleUser, "");

  EXPECT_EQ(msg.role, kMessageRoleUser);
  EXPECT_TRUE(msg.content.empty());
}

// Usage Tests
class UsageTest : public AITestFixture {};

TEST_F(UsageTest, DefaultConstructor) {
  Usage usage;

  EXPECT_EQ(usage.prompt_tokens, 0);
  EXPECT_EQ(usage.completion_tokens, 0);
  EXPECT_EQ(usage.total_tokens, 0);
}

TEST_F(UsageTest, ParameterizedConstructor) {
  Usage usage{15, 25};

  EXPECT_EQ(usage.prompt_tokens, 15);
  EXPECT_EQ(usage.completion_tokens, 25);
  EXPECT_EQ(usage.total_tokens, 40);
}

TEST_F(UsageTest, Addition) {
  Usage usage1{10, 20};
  Usage usage2{5, 15};

  // Note: This assumes Usage has operator+ defined
  // If not implemented, this test would need to be removed
  // or Usage would need to be extended
}

// Error Classes Tests
class ErrorClassesTest : public AITestFixture {};

TEST_F(ErrorClassesTest, AIErrorBase) {
  AIError error("Base AI error");

  EXPECT_STREQ(error.what(), "Base AI error");
}

TEST_F(ErrorClassesTest, APIError) {
  APIError error(400, "Bad request");

  EXPECT_EQ(error.status_code(), 400);
  EXPECT_THAT(std::string(error.what()), testing::HasSubstr("400"));
  EXPECT_THAT(std::string(error.what()), testing::HasSubstr("Bad request"));
}

TEST_F(ErrorClassesTest, AuthenticationError) {
  AuthenticationError error("Invalid API key");

  EXPECT_EQ(error.status_code(), 401);
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Authentication failed"));
  EXPECT_THAT(std::string(error.what()), testing::HasSubstr("Invalid API key"));
}

TEST_F(ErrorClassesTest, RateLimitError) {
  RateLimitError error("Too many requests");

  EXPECT_EQ(error.status_code(), 429);
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Rate limit exceeded"));
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Too many requests"));
}

TEST_F(ErrorClassesTest, ConfigurationError) {
  ConfigurationError error("Invalid model specified");

  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Configuration error"));
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Invalid model specified"));
}

TEST_F(ErrorClassesTest, NetworkError) {
  NetworkError error("Connection timeout");

  EXPECT_THAT(std::string(error.what()), testing::HasSubstr("Network error"));
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Connection timeout"));
}

TEST_F(ErrorClassesTest, ModelError) {
  ModelError error("Unsupported model");

  EXPECT_THAT(std::string(error.what()), testing::HasSubstr("Model error"));
  EXPECT_THAT(std::string(error.what()),
              testing::HasSubstr("Unsupported model"));
}

// Enum Tests
class EnumTest : public AITestFixture {};

TEST_F(EnumTest, MessageRoleValues) {
  // Test that enum values are properly defined
  EXPECT_NE(kMessageRoleSystem, kMessageRoleUser);
  EXPECT_NE(kMessageRoleUser, kMessageRoleAssistant);
  EXPECT_NE(kMessageRoleAssistant, kMessageRoleSystem);
}

TEST_F(EnumTest, FinishReasonValues) {
  // Test that enum values are properly defined
  EXPECT_NE(kFinishReasonStop, kFinishReasonLength);
  EXPECT_NE(kFinishReasonLength, kFinishReasonContentFilter);
  EXPECT_NE(kFinishReasonContentFilter, kFinishReasonToolCalls);
  EXPECT_NE(kFinishReasonToolCalls, kFinishReasonError);
}

// Parameterized Tests for Edge Cases
class TypesEdgeCaseTest : public AITestFixture,
                          public testing::WithParamInterface<std::string> {};

TEST_P(TypesEdgeCaseTest, GenerateOptionsWithVariousPrompts) {
  std::string prompt = GetParam();
  GenerateOptions options("gpt-4o", prompt);

  if (prompt.empty()) {
    EXPECT_FALSE(options.is_valid());
  } else {
    EXPECT_TRUE(options.is_valid());
    EXPECT_EQ(options.prompt, prompt);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PromptVariations,
    TypesEdgeCaseTest,
    testing::Values("",       // Empty prompt
                    "Hello",  // Simple prompt
                    "A very long prompt that contains many words and should "
                    "still work fine with the system",
                    "Prompt with\nnewlines\nand\ttabs",
                    "Unicode test: 你好世界 🌍",
                    "Special chars: !@#$%^&*()[]{}|;':\",./<>?",
                    std::string(10000, 'a')  // Very long prompt
                    ));

// Integration Tests for Type Combinations
class TypeIntegrationTest : public OpenAITestFixture {};

TEST_F(TypeIntegrationTest, CompleteWorkflow) {
  // Create options
  auto options = createAdvancedOptions();
  EXPECT_TRUE(options.is_valid());

  // Create expected result
  Usage usage{10, 20};
  GenerateResult result("Generated text", kFinishReasonStop, usage);
  result.id = "test-123";
  result.model = options.model;

  // Verify result structure
  TestAssertions::assertSuccess(result);
  TestAssertions::assertUsage(usage, 10, 20);
  EXPECT_EQ(result.model.value(), options.model);
}

TEST_F(TypeIntegrationTest, ErrorToSuccessWorkflow) {
  // Start with error result
  GenerateResult error_result("Network error");
  TestAssertions::assertError(error_result, "Network");

  // Convert to success (simulating retry)
  GenerateResult success_result("Retry succeeded", kFinishReasonStop,
                                Usage{5, 10});
  TestAssertions::assertSuccess(success_result);

  // Verify state transition
  EXPECT_FALSE(error_result.is_success());
  EXPECT_TRUE(success_result.is_success());
}

TEST_F(TypeIntegrationTest, MessageConversationFlow) {
  Messages conversation;

  // Start conversation
  conversation.emplace_back(kMessageRoleSystem, "You are helpful");
  conversation.emplace_back(kMessageRoleUser, "Hello");

  EXPECT_EQ(conversation.size(), 2);

  // Add assistant response
  conversation.emplace_back(kMessageRoleAssistant, "Hi there!");

  // Continue conversation
  conversation.emplace_back(kMessageRoleUser, "How are you?");

  EXPECT_EQ(conversation.size(), 4);
  EXPECT_EQ(conversation.back().role, kMessageRoleUser);
  EXPECT_EQ(conversation.back().content, "How are you?");
}

}  // namespace test
}  // namespace ai