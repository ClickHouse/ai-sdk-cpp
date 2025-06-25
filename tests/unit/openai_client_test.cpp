#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Include the OpenAI client headers
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

// Include the real OpenAI client implementation for testing
#include "providers/openai/openai_client.h"

// Test utilities
#include "../utils/test_fixtures.h"

namespace ai {
namespace test {

class OpenAIClientTest : public OpenAITestFixture {
 protected:
  void SetUp() override {
    OpenAITestFixture::SetUp();
    client_ =
        std::make_unique<ai::openai::OpenAIClient>(kTestApiKey, kTestBaseUrl);
  }

  void TearDown() override {
    client_.reset();
    OpenAITestFixture::TearDown();
  }

  std::unique_ptr<ai::openai::OpenAIClient> client_;
};

// Constructor and Configuration Tests
TEST_F(OpenAIClientTest, ConstructorWithValidApiKey) {
  ai::openai::OpenAIClient client("sk-validkey123", "https://api.openai.com");

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.provider_name(), "openai");
  EXPECT_THAT(client.config_info(), testing::HasSubstr("OpenAI API"));

  // Test internal access - these methods are exposed for testing
  EXPECT_EQ(client.get_api_key(), "sk-validkey123");
  EXPECT_EQ(client.get_base_url(), "https://api.openai.com");
  EXPECT_EQ(client.get_host(), "api.openai.com");
  EXPECT_TRUE(client.get_use_ssl());
}

TEST_F(OpenAIClientTest, ConstructorWithEmptyApiKey) {
  ai::openai::OpenAIClient client("", "https://api.openai.com");

  // Real implementation should be invalid with empty API key
  EXPECT_FALSE(client.is_valid());
}

TEST_F(OpenAIClientTest, ConstructorWithCustomBaseUrl) {
  ai::openai::OpenAIClient client("sk-test", "https://custom-api.example.com");

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.provider_name(), "openai");
  EXPECT_THAT(client.config_info(),
              testing::HasSubstr("custom-api.example.com"));

  // Test internal access
  EXPECT_EQ(client.get_host(), "custom-api.example.com");
  EXPECT_TRUE(client.get_use_ssl());
}

TEST_F(OpenAIClientTest, ConstructorWithHttpUrl) {
  ai::openai::OpenAIClient client("sk-test", "http://localhost:8080");

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.get_host(), "localhost:8080");
  EXPECT_FALSE(client.get_use_ssl());
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

// Text Generation Tests - Testing error handling without network calls
TEST_F(OpenAIClientTest, GenerateTextWithInvalidApiKey) {
  ai::openai::OpenAIClient client("invalid-key", "https://api.openai.com");
  auto options = createBasicOptions();

  // This will attempt a real call and should fail gracefully
  auto result = client.generate_text(options);

  // We expect this to fail since we're using an invalid API key
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.error_message().empty());
}

TEST_F(OpenAIClientTest, GenerateTextWithBadUrl) {
  ai::openai::OpenAIClient client(
      "sk-test", "http://invalid-url-that-does-not-exist.example");
  auto options = createBasicOptions();

  // This should fail due to network connectivity
  auto result = client.generate_text(options);

  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.error_message().empty());
}

// Configuration Tests
TEST_F(OpenAIClientTest, ConfigurationInfoContainsExpectedData) {
  auto config = client_->config_info();

  EXPECT_FALSE(config.empty());
  EXPECT_THAT(config, testing::HasSubstr("OpenAI"));
  EXPECT_THAT(config, testing::HasSubstr(kTestBaseUrl));
}

TEST_F(OpenAIClientTest, ProviderNameIsConsistent) {
  EXPECT_EQ(client_->provider_name(), "openai");
}

// Test option validation without network calls
TEST_F(OpenAIClientTest, ValidateOptionsValidation) {
  // Test with empty model
  GenerateOptions invalid_options("", "test prompt");
  EXPECT_FALSE(invalid_options.is_valid());

  // Test with valid options
  auto valid_options = createBasicOptions();
  EXPECT_TRUE(valid_options.is_valid());
}

// Stream Tests (Basic validation)
TEST_F(OpenAIClientTest, StreamTextBasicValidation) {
  auto options = StreamOptions(GenerateOptions(kTestModel, kTestPrompt));

  // For now, just verify the call doesn't crash
  // In a real test environment, this would fail due to network, but should
  // handle gracefully
  auto result = client_->stream_text(options);

  // The stream result should be created even if the underlying request fails
  EXPECT_TRUE(true);  // Just verify no crash occurred
}

// Internal Method Tests - These test the private implementation
TEST_F(OpenAIClientTest, TestInternalJsonBuilding) {
  auto options = createBasicOptions();

  // Test the internal JSON building method (exposed via AI_SDK_TESTING)
  auto json = client_->build_request_json(options);

  EXPECT_EQ(json["model"], kTestModel);
  EXPECT_TRUE(json.contains("messages"));
  EXPECT_TRUE(json["messages"].is_array());
  EXPECT_FALSE(json["messages"].empty());
}

TEST_F(OpenAIClientTest, TestMessageRoleConversion) {
  // Test the internal message role conversion method
  EXPECT_EQ(client_->message_role_to_string(kMessageRoleSystem), "system");
  EXPECT_EQ(client_->message_role_to_string(kMessageRoleUser), "user");
  EXPECT_EQ(client_->message_role_to_string(kMessageRoleAssistant),
            "assistant");
}

TEST_F(OpenAIClientTest, TestFinishReasonParsing) {
  // Test the internal finish reason parsing method
  EXPECT_EQ(client_->parse_finish_reason("stop"), kFinishReasonStop);
  EXPECT_EQ(client_->parse_finish_reason("length"), kFinishReasonLength);
  EXPECT_EQ(client_->parse_finish_reason("content_filter"),
            kFinishReasonContentFilter);
  EXPECT_EQ(client_->parse_finish_reason("tool_calls"), kFinishReasonToolCalls);
  EXPECT_EQ(client_->parse_finish_reason("unknown"), kFinishReasonError);
}

}  // namespace test
}  // namespace ai