// Input Validator Unit Tests
// Tests input validation for GenerateOptions

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/input_validator.h"

#include "ai/bedrock.h"
#include "ai/types/generate_options.h"
#include "ai/types/message.h"

namespace ai {
namespace bedrock {
namespace {

class InputValidatorTest : public ::testing::Test {
 protected:
  InputValidator validator_;
};

// Test valid model ID patterns
TEST_F(InputValidatorTest, ValidStandardModelIds) {
  EXPECT_TRUE(validator_.is_valid_model_id("anthropic.claude-3-5-sonnet-20241022-v2:0"));
  EXPECT_TRUE(validator_.is_valid_model_id("anthropic.claude-3-5-haiku-20241022-v1:0"));
  EXPECT_TRUE(validator_.is_valid_model_id("amazon.titan-text-express-v1"));
  EXPECT_TRUE(validator_.is_valid_model_id("amazon.titan-text-lite-v1"));
  EXPECT_TRUE(validator_.is_valid_model_id("meta.llama3-8b-instruct-v1:0"));
  EXPECT_TRUE(validator_.is_valid_model_id("meta.llama3-70b-instruct-v1:0"));
}

// Test valid ARN model IDs
TEST_F(InputValidatorTest, ValidArnModelIds) {
  EXPECT_TRUE(validator_.is_valid_model_id(
      "arn:aws:bedrock:us-east-1:123456789012:custom-model/my-model"));
  EXPECT_TRUE(validator_.is_valid_model_id(
      "arn:aws:bedrock:us-west-2:123456789012:provisioned-model/my-model"));
  EXPECT_TRUE(validator_.is_valid_model_id(
      "arn:aws:bedrock:eu-west-1:123456789012:foundation-model/anthropic/claude"));
}

// Test invalid model IDs
TEST_F(InputValidatorTest, InvalidModelIds) {
  EXPECT_FALSE(validator_.is_valid_model_id(""));
  EXPECT_FALSE(validator_.is_valid_model_id("invalid"));
  EXPECT_FALSE(validator_.is_valid_model_id("no-dot-separator"));
  EXPECT_FALSE(validator_.is_valid_model_id("has spaces.model"));
  EXPECT_FALSE(validator_.is_valid_model_id("special!chars.model"));
}

// Test temperature validation
TEST_F(InputValidatorTest, ValidTemperature) {
  EXPECT_TRUE(validator_.is_valid_temperature(0.0));
  EXPECT_TRUE(validator_.is_valid_temperature(0.5));
  EXPECT_TRUE(validator_.is_valid_temperature(1.0));
  EXPECT_TRUE(validator_.is_valid_temperature(0.7));
}

TEST_F(InputValidatorTest, InvalidTemperature) {
  EXPECT_FALSE(validator_.is_valid_temperature(-0.1));
  EXPECT_FALSE(validator_.is_valid_temperature(1.1));
  EXPECT_FALSE(validator_.is_valid_temperature(-1.0));
  EXPECT_FALSE(validator_.is_valid_temperature(2.0));
}

// Test top_p validation
TEST_F(InputValidatorTest, ValidTopP) {
  EXPECT_TRUE(validator_.is_valid_top_p(0.0));
  EXPECT_TRUE(validator_.is_valid_top_p(0.5));
  EXPECT_TRUE(validator_.is_valid_top_p(1.0));
  EXPECT_TRUE(validator_.is_valid_top_p(0.9));
}

TEST_F(InputValidatorTest, InvalidTopP) {
  EXPECT_FALSE(validator_.is_valid_top_p(-0.1));
  EXPECT_FALSE(validator_.is_valid_top_p(1.1));
  EXPECT_FALSE(validator_.is_valid_top_p(-1.0));
  EXPECT_FALSE(validator_.is_valid_top_p(2.0));
}

// Test max_tokens validation
TEST_F(InputValidatorTest, ValidMaxTokens) {
  EXPECT_TRUE(validator_.is_valid_max_tokens(1));
  EXPECT_TRUE(validator_.is_valid_max_tokens(100));
  EXPECT_TRUE(validator_.is_valid_max_tokens(1000));
  EXPECT_TRUE(validator_.is_valid_max_tokens(200000));
}

TEST_F(InputValidatorTest, InvalidMaxTokens) {
  EXPECT_FALSE(validator_.is_valid_max_tokens(0));
  EXPECT_FALSE(validator_.is_valid_max_tokens(-1));
  EXPECT_FALSE(validator_.is_valid_max_tokens(-100));
  EXPECT_FALSE(validator_.is_valid_max_tokens(200001));
}

// Test prompt length validation
TEST_F(InputValidatorTest, ValidPromptLength) {
  std::string short_prompt = "Hello, world!";
  std::string medium_prompt(1000, 'a');
  
  EXPECT_TRUE(validator_.is_valid_prompt_length(short_prompt));
  EXPECT_TRUE(validator_.is_valid_prompt_length(medium_prompt));
  EXPECT_TRUE(validator_.is_valid_prompt_length(""));
}

TEST_F(InputValidatorTest, InvalidPromptLength) {
  SecurityConfig config;
  config.max_prompt_length = 100;
  InputValidator strict_validator(config);
  
  std::string long_prompt(101, 'a');
  EXPECT_FALSE(strict_validator.is_valid_prompt_length(long_prompt));
}

// Test full validation
TEST_F(InputValidatorTest, ValidGenerateOptions) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello, world!";
  options.temperature = 0.7;
  options.top_p = 0.9;
  options.max_tokens = 100;

  auto error = validator_.validate(options);
  EXPECT_FALSE(error.has_value()) << "Error: " << error.value_or("");
}

TEST_F(InputValidatorTest, ValidationFailsOnEmptyModel) {
  GenerateOptions options;
  options.model = "";
  options.prompt = "Hello";

  auto error = validator_.validate(options);
  EXPECT_TRUE(error.has_value());
  EXPECT_NE(error->find("Model ID"), std::string::npos);
}

TEST_F(InputValidatorTest, ValidationFailsOnInvalidTemperature) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.temperature = 1.5;

  auto error = validator_.validate(options);
  EXPECT_TRUE(error.has_value());
  EXPECT_NE(error->find("Temperature"), std::string::npos);
}

TEST_F(InputValidatorTest, ValidationFailsOnInvalidTopP) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.top_p = -0.5;

  auto error = validator_.validate(options);
  EXPECT_TRUE(error.has_value());
  EXPECT_NE(error->find("Top_p"), std::string::npos);
}

TEST_F(InputValidatorTest, ValidationFailsOnInvalidMaxTokens) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.max_tokens = 0;

  auto error = validator_.validate(options);
  EXPECT_TRUE(error.has_value());
  EXPECT_NE(error->find("Max tokens"), std::string::npos);
}

TEST_F(InputValidatorTest, ValidationFailsOnMissingPromptAndMessages) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  // No prompt or messages

  auto error = validator_.validate(options);
  EXPECT_TRUE(error.has_value());
  EXPECT_NE(error->find("prompt or messages"), std::string::npos);
}

TEST_F(InputValidatorTest, ValidationPassesWithMessages) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.messages = {Message::user("Hello")};

  auto error = validator_.validate(options);
  EXPECT_FALSE(error.has_value()) << "Error: " << error.value_or("");
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

TEST(InputValidatorTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
