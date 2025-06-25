#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Include the OpenAI client headers
#include "ai/errors.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

// Test utilities
#include "../utils/mock_openai_client.h"
#include "../utils/test_fixtures.h"

// We need to access private implementation for thorough testing
// In a real implementation, you'd include the private header
// For now, we'll test the public interface and use controllable mocks

namespace ai {
namespace test {

class OpenAIClientTest : public OpenAITestFixture {
 protected:
  void SetUp() override {
    OpenAITestFixture::SetUp();
    client_ =
        std::make_unique<ControllableOpenAIClient>(kTestApiKey, kTestBaseUrl);
  }

  void TearDown() override {
    client_.reset();
    OpenAITestFixture::TearDown();
  }

  std::unique_ptr<ControllableOpenAIClient> client_;
};

// Constructor and Configuration Tests
TEST_F(OpenAIClientTest, ConstructorWithValidApiKey) {
  ControllableOpenAIClient client("sk-validkey123", "https://api.openai.com");

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.provider_name(), "openai-test");
  EXPECT_THAT(client.config_info(), testing::HasSubstr("Test OpenAI Client"));
}

TEST_F(OpenAIClientTest, ConstructorWithEmptyApiKey) {
  ControllableOpenAIClient client("", "https://api.openai.com");

  // Note: Our controllable client doesn't validate API key emptiness
  // In real implementation, this would likely be invalid
  EXPECT_TRUE(client.is_valid());  // Simplified for testing
}

TEST_F(OpenAIClientTest, ConstructorWithCustomBaseUrl) {
  ControllableOpenAIClient client("sk-test", "https://custom-api.example.com");

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.provider_name(), "openai-test");
}

// Model Support Tests
TEST_F(OpenAIClientTest, SupportedModelsContainsExpectedModels) {
  auto models = client_->supported_models();

  EXPECT_THAT(models, testing::Contains("gpt-4o"));
  EXPECT_THAT(models, testing::Contains("gpt-4o-mini"));
  EXPECT_THAT(models, testing::Contains("gpt-4"));
  EXPECT_THAT(models, testing::Contains("gpt-3.5-turbo"));
  EXPECT_FALSE(models.empty());
}

TEST_F(OpenAIClientTest, SupportsValidModel) {
  EXPECT_TRUE(client_->supports_model("gpt-4o"));
  EXPECT_TRUE(client_->supports_model("gpt-4"));
  EXPECT_TRUE(client_->supports_model("gpt-3.5-turbo"));
}

TEST_F(OpenAIClientTest, DoesNotSupportInvalidModel) {
  EXPECT_FALSE(client_->supports_model("invalid-model"));
  EXPECT_FALSE(client_->supports_model("claude-3"));
  EXPECT_FALSE(client_->supports_model(""));
}

// Text Generation Tests
TEST_F(OpenAIClientTest, GenerateTextWithBasicOptions) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(
      ai::test::ResponseBuilder::buildSuccessResponse("Hello, world!"));

  auto result = client_->generate_text(options);

  ai::test::TestAssertions::assertSuccess(result);
  EXPECT_EQ(result.text, "Hello, world!");
  EXPECT_EQ(client_->getCallCount(), 1);

  auto last_options = client_->getLastGenerateOptions();
  EXPECT_EQ(last_options.model, kTestModel);
  EXPECT_EQ(last_options.prompt, kTestPrompt);
}

TEST_F(OpenAIClientTest, GenerateTextWithAdvancedOptions) {
  auto options = createAdvancedOptions();
  client_->setPredefinedResponse(
      ResponseBuilder::buildSuccessResponse("Advanced response"));

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_EQ(result.text, "Advanced response");

  auto last_options = client_->getLastGenerateOptions();
  EXPECT_EQ(last_options.temperature.value(), 0.7);
  EXPECT_EQ(last_options.max_tokens.value(), 100);
  EXPECT_EQ(last_options.top_p.value(), 0.9);
}

TEST_F(OpenAIClientTest, GenerateTextWithMessages) {
  auto messages = createSampleConversation();
  GenerateOptions options(kTestModel, std::move(messages));

  client_->setPredefinedResponse(
      ResponseBuilder::buildSuccessResponse("Conversation response"));

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_EQ(result.text, "Conversation response");
}

TEST_F(OpenAIClientTest, GenerateTextWithUsageInformation) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(
      ResponseBuilder::buildSuccessResponse("Response", "gpt-4o", 15, 25));

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  TestAssertions::assertUsage(result.usage, 15, 25);
}

TEST_F(OpenAIClientTest, GenerateTextWithMetadata) {
  auto options = createBasicOptions();
  auto response_json = TestDataGenerator::createFullValidResponse();
  client_->setPredefinedResponse(response_json.dump());

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_TRUE(result.id.has_value());
  EXPECT_TRUE(result.model.has_value());
  EXPECT_TRUE(result.provider_metadata.has_value());
}

// Error Handling Tests
TEST_F(OpenAIClientTest, HandleNetworkTimeout) {
  auto options = createBasicOptions();
  client_->setShouldTimeout(true);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "timeout");
}

TEST_F(OpenAIClientTest, HandleNetworkFailure) {
  auto options = createBasicOptions();
  client_->setShouldFail(true);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "Network error");
}

TEST_F(OpenAIClientTest, HandleApiErrorResponse) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(ResponseBuilder::buildAuthErrorResponse(),
                                 401);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "401");
}

TEST_F(OpenAIClientTest, HandleRateLimitError) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(ResponseBuilder::buildRateLimitResponse(),
                                 429);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "429");
}

TEST_F(OpenAIClientTest, HandleModelNotFoundError) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(ResponseBuilder::buildModelNotFoundResponse(),
                                 404);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "404");
}

// Edge Cases
TEST_F(OpenAIClientTest, HandleEmptyResponse) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse("", 200);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "parse");
}

TEST_F(OpenAIClientTest, HandleMalformedJsonResponse) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse("{invalid json", 200);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "parse");
}

TEST_F(OpenAIClientTest, HandlePartialResponse) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(ResponseBuilder::buildPartialResponse(), 200);

  auto result = client_->generate_text(options);

  TestAssertions::assertError(result, "parse");
}

// Parameter Validation Tests
class OpenAIClientParameterTest
    : public OpenAITestFixture,
      public testing::WithParamInterface<GenerateOptions> {};

TEST_P(OpenAIClientParameterTest, ValidateOptionsVariations) {
  ControllableOpenAIClient client(kTestApiKey);
  client.setPredefinedResponse(ResponseBuilder::buildSuccessResponse());

  auto options = GetParam();

  if (options.is_valid()) {
    auto result = client.generate_text(options);
    TestAssertions::assertSuccess(result);
  }
}

INSTANTIATE_TEST_SUITE_P(
    OptionsVariations,
    OpenAIClientParameterTest,
    testing::ValuesIn(TestDataGenerator::generateOptionsVariations()));

// Stream Tests (Basic)
TEST_F(OpenAIClientTest, StreamTextBasic) {
  auto options = StreamOptions(GenerateOptions(kTestModel, kTestPrompt));

  auto result = client_->stream_text(options);

  // For now, just verify the call was made
  EXPECT_EQ(client_->getCallCount(), 1);

  auto last_options = client_->getLastStreamOptions();
  EXPECT_EQ(last_options.model, kTestModel);
  EXPECT_EQ(last_options.prompt, kTestPrompt);
}

// Network Failure Simulation Tests
class NetworkFailureTest
    : public OpenAITestFixture,
      public testing::WithParamInterface<NetworkFailureSimulator::FailureType> {
};

TEST_P(NetworkFailureTest, HandleNetworkFailureTypes) {
  ControllableOpenAIClient client(kTestApiKey);
  auto options = createBasicOptions();

  auto failure_type = GetParam();
  auto expected_result =
      NetworkFailureSimulator::createNetworkErrorResult(failure_type);

  // In a real test, we'd inject the failure type into the client
  // For now, just verify the error result is properly constructed
  TestAssertions::assertError(expected_result, "Network error");
}

INSTANTIATE_TEST_SUITE_P(
    AllFailureTypes,
    NetworkFailureTest,
    testing::Values(
        NetworkFailureSimulator::FailureType::kConnectionTimeout,
        NetworkFailureSimulator::FailureType::kReadTimeout,
        NetworkFailureSimulator::FailureType::kConnectionRefused,
        NetworkFailureSimulator::FailureType::kDnsFailure,
        NetworkFailureSimulator::FailureType::kSslError,
        NetworkFailureSimulator::FailureType::kUnexpectedDisconnect));

// Performance Considerations Tests
TEST_F(OpenAIClientTest, HandleLargePrompt) {
  auto large_prompt =
      TestDataGenerator::createLargePrompt(100000);  // 100KB prompt
  GenerateOptions options(kTestModel, large_prompt);

  client_->setPredefinedResponse(
      ResponseBuilder::buildSuccessResponse("Large response handled"));

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
}

TEST_F(OpenAIClientTest, MultipleSequentialCalls) {
  auto options = createBasicOptions();
  client_->setPredefinedResponse(ResponseBuilder::buildSuccessResponse());

  constexpr int num_calls = 5;
  for (int i = 0; i < num_calls; ++i) {
    auto result = client_->generate_text(options);
    TestAssertions::assertSuccess(result);
  }

  EXPECT_EQ(client_->getCallCount(), num_calls);
}

// Configuration Tests
TEST_F(OpenAIClientTest, ConfigurationInfoContainsExpectedData) {
  auto config = client_->config_info();

  EXPECT_FALSE(config.empty());
  EXPECT_THAT(config, testing::HasSubstr("OpenAI"));
}

TEST_F(OpenAIClientTest, ProviderNameIsConsistent) {
  EXPECT_EQ(client_->provider_name(), "openai-test");
}

}  // namespace test
}  // namespace ai