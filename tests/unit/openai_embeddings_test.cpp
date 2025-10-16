#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Include the OpenAI client headers
#include "ai/types/embedding_options.h"

// Include the real OpenAI client implementation for testing
#include "providers/openai/openai_client.h"

// Test utilities
#include "../utils/test_fixtures.h"

namespace ai {
namespace test {

class OpenAIEmbeddingsTest : public OpenAITestFixture {
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

// EmbeddingOptions Constructor and Validation Tests
TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsDefaultConstructor) {
  EmbeddingOptions options;

  EXPECT_FALSE(options.is_valid());
  EXPECT_FALSE(options.has_input());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsBasicConstructor) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input);

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(options.has_input());
  EXPECT_EQ(options.model, "text-embedding-3-small");
  EXPECT_EQ(options.input, input);
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsWithDimensions) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input, 512);

  EXPECT_TRUE(options.is_valid());
  EXPECT_EQ(options.dimensions, 512);
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsWithEncodingFormat) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input, 512, "float");

  EXPECT_TRUE(options.is_valid());
  EXPECT_EQ(options.dimensions, 512);
  EXPECT_EQ(options.encoding_format, "float");
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsWithArrayInput) {
  nlohmann::json input =
      nlohmann::json::array({"first text", "second text", "third text"});
  EmbeddingOptions options("text-embedding-3-small", input);

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(options.has_input());
  EXPECT_TRUE(options.input.is_array());
  EXPECT_EQ(options.input.size(), 3);
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsInvalidEmptyModel) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("", input);

  EXPECT_FALSE(options.is_valid());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingOptionsInvalidNullInput) {
  nlohmann::json input = nlohmann::json();
  EmbeddingOptions options("text-embedding-3-small", input);

  EXPECT_FALSE(options.is_valid());
  EXPECT_FALSE(options.has_input());
}

// EmbeddingResult Tests
TEST_F(OpenAIEmbeddingsTest, EmbeddingResultDefaultConstructor) {
  EmbeddingResult result;

  EXPECT_TRUE(result.is_success());
  EXPECT_TRUE(result.data.is_null());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingResultWithError) {
  EmbeddingResult result("Test error message");

  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.error_message(), "Test error message");
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingResultBoolConversion) {
  EmbeddingResult success_result;
  EXPECT_TRUE(static_cast<bool>(success_result));

  EmbeddingResult error_result("error");
  EXPECT_FALSE(static_cast<bool>(error_result));
}

// Client Tests - Testing error handling without network calls
TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithInvalidApiKey) {
  ai::openai::OpenAIClient client("invalid-key", "https://api.openai.com");

  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input);

  // This will attempt a real call and should fail gracefully
  auto result = client.embeddings(options);

  // We expect this to fail since we're using an invalid API key
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.error_message().empty());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithBadUrl) {
  ai::openai::OpenAIClient client(
      "sk-test", "http://invalid-url-that-does-not-exist.example");

  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input);

  // This should fail due to network connectivity
  auto result = client.embeddings(options);

  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.error_message().empty());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithInvalidOptions) {
  nlohmann::json input = nlohmann::json();  // null input
  EmbeddingOptions invalid_options("", input);

  EXPECT_FALSE(invalid_options.is_valid());
}

// Test option validation
TEST_F(OpenAIEmbeddingsTest, ValidateEmbeddingOptionsValidation) {
  // Test with empty model
  nlohmann::json input = "test";
  EmbeddingOptions invalid_options("", input);
  EXPECT_FALSE(invalid_options.is_valid());

  // Test with null input
  EmbeddingOptions invalid_input_options("text-embedding-3-small",
                                         nlohmann::json());
  EXPECT_FALSE(invalid_input_options.is_valid());

  // Test with valid options
  EmbeddingOptions valid_options("text-embedding-3-small", input);
  EXPECT_TRUE(valid_options.is_valid());
}

// Test different input formats
TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithSingleString) {
  nlohmann::json input = "This is a test string for embeddings";
  EmbeddingOptions options("text-embedding-3-small", input);

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(input.is_string());
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithMultipleStrings) {
  nlohmann::json input =
      nlohmann::json::array({"First embedding text", "Second embedding text",
                             "Third embedding text"});
  EmbeddingOptions options("text-embedding-3-small", input);

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(input.is_array());
  EXPECT_EQ(input.size(), 3);
}

// Test optional parameters
TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithOptionalUser) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input);
  options.user = "test-user-123";

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(options.user.has_value());
  EXPECT_EQ(options.user.value(), "test-user-123");
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithCustomDimensions) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-large", input);
  options.dimensions = 1024;

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(options.dimensions.has_value());
  EXPECT_EQ(options.dimensions.value(), 1024);
}

TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithBase64Encoding) {
  nlohmann::json input = "test text";
  EmbeddingOptions options("text-embedding-3-small", input);
  options.encoding_format = "base64";

  EXPECT_TRUE(options.is_valid());
  EXPECT_TRUE(options.encoding_format.has_value());
  EXPECT_EQ(options.encoding_format.value(), "base64");
}

// Test different embedding models
TEST_F(OpenAIEmbeddingsTest, EmbeddingsWithDifferentModels) {
  nlohmann::json input = "test text";

  // Test with text-embedding-3-small
  EmbeddingOptions small_options("text-embedding-3-small", input);
  EXPECT_TRUE(small_options.is_valid());
  EXPECT_EQ(small_options.model, "text-embedding-3-small");

  // Test with text-embedding-3-large
  EmbeddingOptions large_options("text-embedding-3-large", input);
  EXPECT_TRUE(large_options.is_valid());
  EXPECT_EQ(large_options.model, "text-embedding-3-large");

  // Test with text-embedding-ada-002
  EmbeddingOptions ada_options("text-embedding-ada-002", input);
  EXPECT_TRUE(ada_options.is_valid());
  EXPECT_EQ(ada_options.model, "text-embedding-ada-002");
}

}  // namespace test
}  // namespace ai
