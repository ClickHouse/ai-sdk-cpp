// Bedrock Request Mapper Unit Tests
// Tests message conversion, parameter mapping, and system prompt handling

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/bedrock_request_mapper.h"

#include "ai/bedrock.h"
#include "ai/types/generate_options.h"
#include "ai/types/message.h"

namespace ai {
namespace bedrock {
namespace {

class BedrockRequestMapperTest : public ::testing::Test {
 protected:
  BedrockRequestMapper mapper_;
};

// Test basic prompt-to-message conversion
TEST_F(BedrockRequestMapperTest, ConvertsPromptToUserMessage) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello, world!";

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_EQ(request.GetModelId(), models::kClaudeSonnet);
  ASSERT_EQ(request.GetMessages().size(), 1);
  EXPECT_EQ(request.GetMessages()[0].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
}

// Test model ID mapping
TEST_F(BedrockRequestMapperTest, MapsModelId) {
  GenerateOptions options;
  options.model = "custom-model-id";
  options.prompt = "Test";

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_EQ(request.GetModelId(), "custom-model-id");
}

// Test max_tokens mapping
TEST_F(BedrockRequestMapperTest, MapsMaxTokens) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test";
  options.max_tokens = 1000;

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_EQ(request.GetInferenceConfig().GetMaxTokens(), 1000);
}

// Test temperature mapping
TEST_F(BedrockRequestMapperTest, MapsTemperature) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test";
  options.temperature = 0.7;

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_FLOAT_EQ(request.GetInferenceConfig().GetTemperature(), 0.7f);
}

// Test top_p mapping
TEST_F(BedrockRequestMapperTest, MapsTopP) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test";
  options.top_p = 0.9;

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_FLOAT_EQ(request.GetInferenceConfig().GetTopP(), 0.9f);
}

// Test system prompt mapping
TEST_F(BedrockRequestMapperTest, MapsSystemPrompt) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test";
  options.system = "You are a helpful assistant.";

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetSystem().size(), 1);
  EXPECT_EQ(request.GetSystem()[0].GetText(), "You are a helpful assistant.");
}

// Test messages conversion
TEST_F(BedrockRequestMapperTest, ConvertsMessages) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.messages = {
      Message::user("Hello"),
      Message::assistant("Hi there!"),
      Message::user("How are you?"),
  };

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetMessages().size(), 3);
  EXPECT_EQ(request.GetMessages()[0].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
  EXPECT_EQ(request.GetMessages()[1].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::assistant);
  EXPECT_EQ(request.GetMessages()[2].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
}

// Test stream request mapping
TEST_F(BedrockRequestMapperTest, MapsStreamRequest) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test streaming";
  options.max_tokens = 500;

  auto request = mapper_.map_to_converse_stream_request(options);

  EXPECT_EQ(request.GetModelId(), models::kClaudeSonnet);
  EXPECT_EQ(request.GetInferenceConfig().GetMaxTokens(), 500);
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

// Placeholder test when Bedrock is not enabled
TEST(BedrockRequestMapperTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
