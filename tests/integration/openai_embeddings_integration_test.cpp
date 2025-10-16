#include "../utils/test_fixtures.h"
#include "ai/openai.h"
#include "ai/types/embedding_options.h"

#include <cmath>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Note: These tests connect to the real OpenAI API when OPENAI_API_KEY is set
// Otherwise they are skipped

namespace ai {
namespace test {

class OpenAIEmbeddingsIntegrationTest : public AITestFixture {
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

  // Helper to calculate cosine similarity between two embeddings
  double cosine_similarity(const std::vector<double>& a,
                           const std::vector<double>& b) {
    if (a.size() != b.size()) {
      return 0.0;
    }

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
      dot_product += a[i] * b[i];
      norm_a += a[i] * a[i];
      norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0 || norm_b == 0.0) {
      return 0.0;
    }

    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
  }

  bool use_real_api_;
  std::optional<ai::Client> client_;
};

// Basic Embeddings Tests
TEST_F(OpenAIEmbeddingsIntegrationTest, BasicSingleStringEmbedding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Hello, world!";
  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_FALSE(result.data.is_null());
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);

  // Check that we got an embedding vector
  auto embedding = result.data[0]["embedding"];
  EXPECT_TRUE(embedding.is_array());
  EXPECT_GT(embedding.size(), 0);

  // text-embedding-3-small should have 1536 dimensions by default
  EXPECT_EQ(embedding.size(), 1536);

  // Check token usage
  EXPECT_GT(result.usage.total_tokens, 0);
  EXPECT_GT(result.usage.prompt_tokens, 0);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, MultipleStringsEmbedding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = nlohmann::json::array(
      {"sunny day at the beach", "rainy afternoon in the city",
       "snowy night in the mountains"});

  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 3);

  // Check each embedding
  for (size_t i = 0; i < 3; ++i) {
    auto embedding = result.data[i]["embedding"];
    EXPECT_TRUE(embedding.is_array());
    EXPECT_EQ(embedding.size(), 1536);
  }

  // Check token usage
  EXPECT_GT(result.usage.total_tokens, 0);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, EmbeddingWithCustomDimensions) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Test with custom dimensions";
  EmbeddingOptions options("text-embedding-3-small", input, 512);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);

  // Check that the embedding has the requested dimensions
  auto embedding = result.data[0]["embedding"];
  EXPECT_TRUE(embedding.is_array());
  EXPECT_EQ(embedding.size(), 512);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, EmbeddingWithLargeModel) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Testing large embedding model";
  EmbeddingOptions options("text-embedding-3-large", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);

  // text-embedding-3-large should have 3072 dimensions by default
  auto embedding = result.data[0]["embedding"];
  EXPECT_TRUE(embedding.is_array());
  EXPECT_EQ(embedding.size(), 3072);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, EmbeddingSimilarity) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input =
      nlohmann::json::array({"cat", "kitten", "dog", "puppy", "car"});

  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_EQ(result.data.size(), 5);

  // Convert embeddings to vectors
  std::vector<std::vector<double>> embeddings;
  for (const auto& item : result.data) {
    std::vector<double> embedding;
    for (const auto& val : item["embedding"]) {
      embedding.push_back(val.get<double>());
    }
    embeddings.push_back(embedding);
  }

  // Calculate similarities
  double cat_kitten_sim = cosine_similarity(embeddings[0], embeddings[1]);
  double dog_puppy_sim = cosine_similarity(embeddings[2], embeddings[3]);
  double cat_car_sim = cosine_similarity(embeddings[0], embeddings[4]);

  // Similar words should have higher similarity than unrelated words
  // Note: Single words have moderate similarity (~0.5-0.6), not as high as full
  // sentences
  EXPECT_GT(cat_kitten_sim, 0.5) << "cat and kitten should be similar";
  EXPECT_GT(dog_puppy_sim, 0.5) << "dog and puppy should be similar";
  EXPECT_LT(cat_car_sim, cat_kitten_sim)
      << "cat and car should be less similar than cat and kitten";
}

// Error Handling Tests
TEST_F(OpenAIEmbeddingsIntegrationTest, InvalidApiKey) {
  auto invalid_client = ai::openai::create_client("sk-invalid123");

  nlohmann::json input = "Test with invalid key";
  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = invalid_client.embeddings(options);

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(),
              testing::AnyOf(testing::HasSubstr("401"),
                             testing::HasSubstr("Unauthorized"),
                             testing::HasSubstr("API key"),
                             testing::HasSubstr("authentication")));
}

TEST_F(OpenAIEmbeddingsIntegrationTest, InvalidModel) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Test with invalid model";
  EmbeddingOptions options("invalid-embedding-model", input);
  auto result = client_->embeddings(options);

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(
      result.error_message(),
      testing::AnyOf(testing::HasSubstr("404"), testing::HasSubstr("model"),
                     testing::HasSubstr("not found"),
                     testing::HasSubstr("does not exist")));
}

// Edge Cases
TEST_F(OpenAIEmbeddingsIntegrationTest, EmptyStringEmbedding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "";
  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  // OpenAI API should handle empty strings
  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, LongTextEmbedding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Create a reasonably long text (but within token limits)
  std::string long_text = "This is a test sentence. ";
  for (int i = 0; i < 50; ++i) {
    long_text += "This is a test sentence. ";
  }

  nlohmann::json input = long_text;
  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);

  auto embedding = result.data[0]["embedding"];
  EXPECT_TRUE(embedding.is_array());
  EXPECT_EQ(embedding.size(), 1536);

  // Should use more tokens for longer text
  EXPECT_GT(result.usage.prompt_tokens, 50);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, SpecialCharactersEmbedding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input =
      nlohmann::json::array({"Hello! How are you?", "¡Hola! ¿Cómo estás?",
                             "你好！你好吗？", "🌟✨🎉"});

  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_EQ(result.data.size(), 4);

  // All embeddings should have the correct size
  for (const auto& item : result.data) {
    auto embedding = item["embedding"];
    EXPECT_TRUE(embedding.is_array());
    EXPECT_EQ(embedding.size(), 1536);
  }
}

// Configuration Tests
TEST_F(OpenAIEmbeddingsIntegrationTest, EmbeddingWithUserIdentifier) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Test with user identifier";
  EmbeddingOptions options("text-embedding-3-small", input);
  options.user = "test-user-123";

  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);
}

TEST_F(OpenAIEmbeddingsIntegrationTest, EmbeddingWithBase64Encoding) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Test with base64 encoding";
  EmbeddingOptions options("text-embedding-3-small", input);
  options.encoding_format = "base64";

  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
  EXPECT_TRUE(result.data.is_array());
  EXPECT_EQ(result.data.size(), 1);

  // With base64 encoding, the embedding should be a string
  auto embedding = result.data[0]["embedding"];
  EXPECT_TRUE(embedding.is_string());
}

// Token Usage Tests
TEST_F(OpenAIEmbeddingsIntegrationTest, TokenUsageTracking) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = nlohmann::json::array(
      {"Short text", "This is a slightly longer text with more words"});

  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = client_->embeddings(options);

  ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();

  // Check token usage is properly tracked
  EXPECT_GT(result.usage.prompt_tokens, 0);
  EXPECT_GT(result.usage.total_tokens, 0);
  EXPECT_EQ(result.usage.total_tokens, result.usage.prompt_tokens);

  // Longer text should use more tokens (approximately)
  EXPECT_GT(result.usage.prompt_tokens, 5);
}

// Network Error Tests
TEST_F(OpenAIEmbeddingsIntegrationTest, NetworkFailure) {
  const char* api_key = std::getenv("OPENAI_API_KEY");
  if (!api_key) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // Test with localhost on unused port to simulate connection refused
  auto failing_client =
      ai::openai::create_client(api_key, "http://localhost:59999");

  nlohmann::json input = "Test network failure";
  EmbeddingOptions options("text-embedding-3-small", input);
  auto result = failing_client.embeddings(options);

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(),
              testing::AnyOf(
                  testing::HasSubstr("Network"), testing::HasSubstr("network"),
                  testing::HasSubstr("connection"),
                  testing::HasSubstr("refused"), testing::HasSubstr("failed")));
}

// Different Models Test
TEST_F(OpenAIEmbeddingsIntegrationTest, DifferentEmbeddingModels) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  nlohmann::json input = "Test different models";

  // Test text-embedding-3-small
  {
    EmbeddingOptions options("text-embedding-3-small", input);
    auto result = client_->embeddings(options);
    ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
    EXPECT_EQ(result.data[0]["embedding"].size(), 1536);
  }

  // Test text-embedding-3-large
  {
    EmbeddingOptions options("text-embedding-3-large", input);
    auto result = client_->embeddings(options);
    ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
    EXPECT_EQ(result.data[0]["embedding"].size(), 3072);
  }

  // Test text-embedding-ada-002 (legacy model)
  {
    EmbeddingOptions options("text-embedding-ada-002", input);
    auto result = client_->embeddings(options);
    ASSERT_TRUE(result.is_success()) << "Error: " << result.error_message();
    EXPECT_EQ(result.data[0]["embedding"].size(), 1536);
  }
}

}  // namespace test
}  // namespace ai
