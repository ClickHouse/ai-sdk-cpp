#include "../utils/test_fixtures.h"
#include "ai/openai.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <future>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Note: These tests connect to the real OpenAI API when OPENAI_API_KEY is set
// Otherwise they are skipped

namespace ai {
namespace test {

class OpenAIIntegrationTest : public AITestFixture {
 protected:
  void SetUp() override {
    AITestFixture::SetUp();

    // Check if we should run real API tests
    const char* api_key = std::getenv("OPENAI_API_KEY");

    if (api_key != nullptr) {
      use_real_api_ = true;
      client_ = ai::openai::create_client(api_key);
    } else {
      use_real_api_ = false;
      // Skip tests if no API key is available
    }
  }

  void TearDown() override { AITestFixture::TearDown(); }

  bool use_real_api_;
  std::optional<ai::Client> client_;
};

// Basic API Connectivity Tests
TEST_F(OpenAIIntegrationTest, BasicTextGeneration) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          "Hello, how are you?");
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_GT(result.usage.total_tokens, 0);

  if (result.id.has_value()) {
    EXPECT_FALSE(result.id->empty());
  }
}

TEST_F(OpenAIIntegrationTest, TextGenerationWithSystemPrompt) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(
      ai::openai::models::kGpt4oMini,
      "You are a helpful assistant that responds in French.",
      "Hello, how are you?");

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // With real API, expect French response
  EXPECT_THAT(
      result.text,
      testing::AnyOf(testing::HasSubstr("Bonjour"), testing::HasSubstr("salut"),
                     testing::HasSubstr("ça va"), testing::HasSubstr("bien"),
                     testing::HasSubstr("merci")));
}

TEST_F(OpenAIIntegrationTest, TextGenerationWithParameters) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          "Write a very short story about a cat.");
  options.max_tokens = 50;
  options.temperature = 0.7;
  options.top_p = 0.9;

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // With real API and max_tokens=50, should be relatively short
  EXPECT_LE(result.usage.completion_tokens, 50);
  EXPECT_LE(result.text.length(), 400);  // Rough estimate
}

TEST_F(OpenAIIntegrationTest, ConversationWithMessages) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  Messages conversation = {
      Message::system("You are a helpful weather assistant."),
      Message::user("Hello!"),
      Message::assistant("Hello! I can help you with weather information."),
      Message::user("What's the weather like today?")};

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          std::move(conversation));
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // Should respond as a weather assistant (though actual weather won't be real)
  EXPECT_THAT(result.text, testing::AnyOf(testing::HasSubstr("weather"),
                                          testing::HasSubstr("information"),
                                          testing::HasSubstr("location"),
                                          testing::HasSubstr("current"),
                                          testing::HasSubstr("help")));
}

// Model Testing
TEST_F(OpenAIIntegrationTest, DifferentModelSupport) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  std::vector<std::string> models_to_test = {ai::openai::models::kGpt4o,
                                             ai::openai::models::kGpt4oMini};

  for (const auto& model : models_to_test) {
    if (!client_->supports_model(model)) {
      GTEST_SKIP() << "Model " << model << " not supported by client";
    }

    GenerateOptions options(model, "Say hello");
    auto result = client_->generate_text(options);

    TestAssertions::assertSuccess(result);
    EXPECT_FALSE(result.text.empty());

    if (result.model.has_value()) {
      EXPECT_TRUE(result.model.value().find(model) == 0)
          << "Expected model to start with: " << model
          << ", but got: " << result.model.value();
    }
  }
}

// Error Handling Tests
TEST_F(OpenAIIntegrationTest, InvalidApiKey) {
  // Test with invalid API key
  auto invalid_client = ai::openai::create_client("sk-invalid123");

  GenerateOptions options(ai::openai::models::kGpt4oMini, "Test prompt");
  auto result = invalid_client.generate_text(options);

  TestAssertions::assertError(result);
  EXPECT_THAT(result.error_message(),
              testing::AnyOf(testing::HasSubstr("401"),
                             testing::HasSubstr("Unauthorized"),
                             testing::HasSubstr("API key"),
                             testing::HasSubstr("authentication")));
}

TEST_F(OpenAIIntegrationTest, InvalidModel) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options("invalid-model-name", "Test prompt");
  auto result = client_->generate_text(options);

  TestAssertions::assertError(result);
  EXPECT_THAT(
      result.error_message(),
      testing::AnyOf(testing::HasSubstr("404"), testing::HasSubstr("model"),
                     testing::HasSubstr("not found"),
                     testing::HasSubstr("does not exist")));
}

TEST_F(OpenAIIntegrationTest, RateLimitHandling) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Note: This test may not trigger rate limiting in normal usage
  // It's here to document the expected behavior when rate limits are hit
  GenerateOptions options(ai::openai::models::kGpt4oMini, "Test prompt");
  auto result = client_->generate_text(options);

  // If we hit rate limits, the error should be handled gracefully
  if (!result.is_success()) {
    EXPECT_THAT(result.error_message(),
                testing::AnyOf(testing::HasSubstr("429"),
                               testing::HasSubstr("rate limit"),
                               testing::HasSubstr("quota")));
  } else {
    // Normal case - request succeeded
    TestAssertions::assertSuccess(result);
  }
}

// Streaming Tests
TEST_F(OpenAIIntegrationTest, BasicStreaming) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions gen_options(ai::openai::models::kGpt4oMini,
                              "Count from 1 to 3");
  StreamOptions options(gen_options);
  auto stream = client_->stream_text(options);

  bool received_text = false;
  bool stream_finished = false;
  std::string accumulated_text;

  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      received_text = true;
      accumulated_text += event.text_delta;
    } else if (event.is_finish()) {
      stream_finished = true;
      break;
    } else if (event.is_error()) {
      FAIL() << "Streaming error: " << event.error.value_or("unknown");
    }
  }

  EXPECT_TRUE(received_text) << "Should have received text deltas";
  EXPECT_TRUE(stream_finished) << "Stream should have finished";
  EXPECT_FALSE(accumulated_text.empty()) << "Should have accumulated some text";
}

// Performance Tests
TEST_F(OpenAIIntegrationTest, ConcurrentRequests) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test smaller number of concurrent requests to respect rate limits
  const int num_requests = 2;
  std::vector<std::future<GenerateResult>> futures;
  futures.reserve(num_requests);

  for (int i = 0; i < num_requests; ++i) {
    futures.push_back(std::async(std::launch::async, [this, i]() {
      GenerateOptions options(ai::openai::models::kGpt4oMini,
                              "Say hello " + std::to_string(i));
      return client_->generate_text(options);
    }));
  }

  // Wait for all requests to complete
  for (auto& future : futures) {
    auto result = future.get();
    TestAssertions::assertSuccess(result);
    EXPECT_FALSE(result.text.empty());
  }
}

TEST_F(OpenAIIntegrationTest, LargePromptHandling) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Use smaller prompt to avoid hitting token limits
  auto large_prompt =
      TestDataGenerator::createLargePrompt(2000);  // ~2KB prompt

  GenerateOptions options(ai::openai::models::kGpt4oMini, large_prompt);
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // Large prompts should use more tokens
  EXPECT_GT(result.usage.prompt_tokens,
            250);  // Adjusted for actual tokenization
}

// Configuration Tests
TEST_F(OpenAIIntegrationTest, CustomBaseUrl) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test that custom base URL can be set (though we'll use OpenAI's URL)
  const char* api_key = std::getenv("OPENAI_API_KEY");
  auto custom_client =
      ai::openai::create_client(api_key, "https://api.openai.com");

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          "Test custom base URL");
  auto result = custom_client.generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
}

// Edge Cases
TEST_F(OpenAIIntegrationTest, EmptyPrompt) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt4oMini, "");

  // Empty prompt should be caught by validation
  EXPECT_FALSE(options.is_valid());

  // The client should handle this gracefully
  auto result = client_->generate_text(options);
  TestAssertions::assertError(result);
}

TEST_F(OpenAIIntegrationTest, VeryLongResponse) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          "Write a detailed explanation of quantum physics");
  options.max_tokens = 500;  // Reasonable limit for testing

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // Real API should respect max_tokens
  EXPECT_LE(result.usage.completion_tokens, 500);
}

// Timeout and Network Tests
TEST_F(OpenAIIntegrationTest, NetworkTimeout) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Note: This test documents timeout behavior but cannot reliably trigger it
  // with the real API under normal conditions
  GenerateOptions options(ai::openai::models::kGpt4oMini, "Simple test");
  auto result = client_->generate_text(options);

  // Under normal conditions, this should succeed
  // If it fails due to network issues, the error should be handled gracefully
  if (!result.is_success()) {
    EXPECT_THAT(result.error_message(),
                testing::AnyOf(testing::HasSubstr("timeout"),
                               testing::HasSubstr("network"),
                               testing::HasSubstr("connection")));
  } else {
    TestAssertions::assertSuccess(result);
  }
}

TEST_F(OpenAIIntegrationTest, NetworkFailure) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test with localhost on unused port to simulate connection refused quickly
  const char* api_key = std::getenv("OPENAI_API_KEY");
  auto failing_client = ai::openai::create_client(
      api_key, "http://localhost:59999");  // Very unlikely port to be in use

  GenerateOptions options(ai::openai::models::kGpt4oMini,
                          "Test network failure");
  auto result = failing_client.generate_text(options);

  TestAssertions::assertError(result);
  EXPECT_THAT(result.error_message(),
              testing::AnyOf(
                  testing::HasSubstr("Network"), testing::HasSubstr("network"),
                  testing::HasSubstr("connection"),
                  testing::HasSubstr("refused"), testing::HasSubstr("failed")));
}

// Environment Configuration Test
class EnvironmentConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Save original environment
    original_api_key_ = std::getenv("OPENAI_API_KEY");
    original_base_url_ = std::getenv("OPENAI_BASE_URL");
  }

  void TearDown() override {
    // Restore environment (if needed)
  }

 private:
  const char* original_api_key_;
  const char* original_base_url_;
};

TEST_F(EnvironmentConfigTest, ConfigurationFromEnvironment) {
  const char* api_key = std::getenv("OPENAI_API_KEY");
  const char* base_url = std::getenv("OPENAI_BASE_URL");

  if (api_key) {
    auto client = base_url ? ai::openai::create_client(api_key, base_url)
                           : ai::openai::create_client(api_key);
    EXPECT_TRUE(client.is_valid());
    EXPECT_EQ(client.provider_name(), "openai");
  } else {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }
}

TEST_F(EnvironmentConfigTest, DefaultClientCreation) {
  const char* api_key = std::getenv("OPENAI_API_KEY");

  if (api_key) {
    // Test creating client with default configuration (reads from environment)
    auto client = ai::openai::create_client();
    EXPECT_TRUE(client.is_valid());
    EXPECT_EQ(client.provider_name(), "openai");

    // Test that supported models include expected ones
    auto models = client.supported_models();
    EXPECT_TRUE(client.supports_model(ai::openai::models::kGpt4oMini));
    EXPECT_TRUE(client.supports_model(ai::openai::models::kGpt35Turbo));
  } else {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }
}

TEST_F(OpenAIIntegrationTest, DefaultModelGeneration) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test generate_text using default_model()
  GenerateOptions options(client_->default_model(), "Count to 3");
  options.max_tokens = 50;

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // Verify we're using the expected default model
  EXPECT_EQ(client_->default_model(), ai::openai::models::kDefaultModel);
  if (result.model.has_value()) {
    EXPECT_TRUE(result.model.value().find(client_->default_model()) !=
                std::string::npos);
  }
}

TEST_F(OpenAIIntegrationTest, DefaultModelStreaming) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test stream_text using default_model()
  GenerateOptions gen_options(client_->default_model(), "Count from 1 to 3");
  gen_options.max_tokens = 50;
  StreamOptions options(gen_options);

  auto stream = client_->stream_text(options);

  bool received_text = false;
  bool stream_finished = false;
  std::string accumulated_text;

  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      received_text = true;
      accumulated_text += event.text_delta;
    } else if (event.is_finish()) {
      stream_finished = true;
      break;
    } else if (event.is_error()) {
      FAIL() << "Streaming error: " << event.error.value_or("unknown");
    }
  }

  EXPECT_TRUE(received_text) << "Should have received text deltas";
  EXPECT_TRUE(stream_finished) << "Stream should have finished";
  EXPECT_FALSE(accumulated_text.empty()) << "Should have accumulated some text";
}

}  // namespace test
}  // namespace ai