// Bedrock Request Mapper Unit Tests
// Tests message conversion, parameter mapping, and system prompt handling

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "ai/bedrock.h"
#include "ai/types/generate_options.h"
#include "ai/types/message.h"
#include "ai/types/tool.h"
#include "providers/bedrock/bedrock_request_mapper.h"

#include <aws/bedrock-runtime/model/ToolResultStatus.h>

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
  options.model = models::kNovaLite;
  options.prompt = "Test";
  options.temperature = 0.7;

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_FLOAT_EQ(request.GetInferenceConfig().GetTemperature(), 0.7f);
}

// Test top_p mapping
TEST_F(BedrockRequestMapperTest, MapsTopP) {
  GenerateOptions options;
  options.model = models::kNovaLite;
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

// Claude models that reject sampling parameters do not get them
TEST_F(BedrockRequestMapperTest, DropsSamplingParametersForClaude) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Test";
  options.temperature = 0.7;
  options.top_p = 0.9;

  auto request = mapper_.map_to_converse_request(options);

  EXPECT_FALSE(request.GetInferenceConfig().TemperatureHasBeenSet());
  EXPECT_FALSE(request.GetInferenceConfig().TopPHasBeenSet());
}

TEST_F(BedrockRequestMapperTest, RecognizesSamplingRejectingModels) {
  EXPECT_TRUE(BedrockRequestMapper::rejects_sampling_parameters(
      "anthropic.claude-opus-5-5"));
  EXPECT_TRUE(BedrockRequestMapper::rejects_sampling_parameters(
      "us.anthropic.claude-sonnet-5-5"));
  EXPECT_TRUE(BedrockRequestMapper::rejects_sampling_parameters(
      "global.anthropic.claude-fable-5-2"));
  EXPECT_TRUE(BedrockRequestMapper::rejects_sampling_parameters(
      "arn:aws:bedrock:us-east-1:123456789012:inference-profile/"
      "us.anthropic.claude-haiku-5-5"));
  EXPECT_FALSE(BedrockRequestMapper::rejects_sampling_parameters(
      "anthropic.claude-sonnet-4-6"));
  EXPECT_FALSE(
      BedrockRequestMapper::rejects_sampling_parameters(models::kNovaLite));
  EXPECT_FALSE(
      BedrockRequestMapper::rejects_sampling_parameters(models::kLlama3_8B));
}

// System messages inside `messages` become system content blocks
TEST_F(BedrockRequestMapperTest, MergesSystemMessagesIntoSystem) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.system = "Be brief.";
  options.messages = {
      Message::system("Answer in English."),
      Message::user("Hello"),
  };

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetSystem().size(), 2);
  EXPECT_EQ(request.GetSystem()[0].GetText(), "Be brief.");
  EXPECT_EQ(request.GetSystem()[1].GetText(), "Answer in English.");
  ASSERT_EQ(request.GetMessages().size(), 1);
  EXPECT_EQ(request.GetMessages()[0].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
}

// With only system messages, the prompt becomes the conversation
TEST_F(BedrockRequestMapperTest, UsesPromptWhenOnlySystemMessages) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.messages = {Message::system("Be brief.")};

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetSystem().size(), 1);
  ASSERT_EQ(request.GetMessages().size(), 1);
  EXPECT_EQ(request.GetMessages()[0].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
  EXPECT_EQ(request.GetMessages()[0].GetContent()[0].GetText(), "Hello");
}

// Dropping a system message between two user turns merges them, because
// Converse requires alternating roles
TEST_F(BedrockRequestMapperTest, MergesAdjacentSameRoleTurns) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.messages = {
      Message::user("First"),
      Message::system("Answer in English."),
      Message::user("Second"),
      Message::assistant("Reply"),
  };

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetMessages().size(), 2);
  const auto& user = request.GetMessages()[0];
  EXPECT_EQ(user.GetRole(), Aws::BedrockRuntime::Model::ConversationRole::user);
  ASSERT_EQ(user.GetContent().size(), 2);
  EXPECT_EQ(user.GetContent()[0].GetText(), "First");
  EXPECT_EQ(user.GetContent()[1].GetText(), "Second");
  EXPECT_EQ(request.GetMessages()[1].GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::assistant);
}

// When a text turn and a tool result turn are merged, the tool results come
// first, as Claude requires
TEST_F(BedrockRequestMapperTest, MergedTurnPutsToolResultsFirst) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.messages = {
      Message::user("Weather in Paris?"),
      Message::assistant_with_tools(
          "",
          {ToolCallContentPart("call_1", "get_weather", {{"city", "Paris"}})}),
      Message::user("Also, answer in French."),
      Message::tool_results({ToolResultContentPart(
          "call_1", JsonValue{{"temperature", 20}}, false)}),
      Message::user("Thanks!"),
  };

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetMessages().size(), 3);
  const auto& merged = request.GetMessages()[2];
  EXPECT_EQ(merged.GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
  ASSERT_EQ(merged.GetContent().size(), 3);
  EXPECT_TRUE(merged.GetContent()[0].ToolResultHasBeenSet());
  EXPECT_EQ(merged.GetContent()[0].GetToolResult().GetToolUseId(), "call_1");
  EXPECT_EQ(merged.GetContent()[1].GetText(), "Also, answer in French.");
  EXPECT_EQ(merged.GetContent()[2].GetText(), "Thanks!");
}

ToolSet make_weather_tools() {
  ToolSet tools;
  tools["get_weather"] =
      Tool("Get the weather for a city",
           {{"type", "object"},
            {"properties", {{"city", {{"type", "string"}}}}},
            {"required", {"city"}}},
           [](const JsonValue&, const ToolExecutionContext&) {
             return JsonValue{{"temperature", 20}};
           });
  return tools;
}

TEST_F(BedrockRequestMapperTest, MapsToolsAndToolChoice) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Weather in Paris?";
  options.tools = make_weather_tools();

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_TRUE(request.ToolConfigHasBeenSet());
  const auto& tools = request.GetToolConfig().GetTools();
  ASSERT_EQ(tools.size(), 1);
  EXPECT_EQ(tools[0].GetToolSpec().GetName(), "get_weather");
  EXPECT_EQ(tools[0].GetToolSpec().GetDescription(),
            "Get the weather for a city");
  EXPECT_EQ(BedrockRequestMapper::from_document(
                tools[0].GetToolSpec().GetInputSchema().GetJson())["required"],
            nlohmann::json::array({"city"}));
  EXPECT_TRUE(request.GetToolConfig().GetToolChoice().AutoHasBeenSet());

  options.tool_choice = ToolChoice::specific("get_weather");
  request = mapper_.map_to_converse_request(options);
  EXPECT_EQ(request.GetToolConfig().GetToolChoice().GetTool().GetName(),
            "get_weather");

  options.tool_choice = ToolChoice::none();
  request = mapper_.map_to_converse_request(options);
  EXPECT_FALSE(request.ToolConfigHasBeenSet());
}

TEST_F(BedrockRequestMapperTest, MapsToolCallsAndResults) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.tools = make_weather_tools();
  options.messages = {
      Message::user("Weather in Paris?"),
      Message::assistant_with_tools(
          "Let me check.",
          {ToolCallContentPart("call_1", "get_weather", {{"city", "Paris"}})}),
      Message::tool_results({ToolResultContentPart(
          "call_1", JsonValue{{"temperature", 20}}, false)}),
  };

  auto request = mapper_.map_to_converse_request(options);

  ASSERT_EQ(request.GetMessages().size(), 3);

  const auto& assistant = request.GetMessages()[1];
  EXPECT_EQ(assistant.GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::assistant);
  ASSERT_EQ(assistant.GetContent().size(), 2);
  EXPECT_EQ(assistant.GetContent()[0].GetText(), "Let me check.");
  const auto& tool_use = assistant.GetContent()[1].GetToolUse();
  EXPECT_EQ(tool_use.GetToolUseId(), "call_1");
  EXPECT_EQ(tool_use.GetName(), "get_weather");
  EXPECT_EQ(BedrockRequestMapper::from_document(tool_use.GetInput()),
            (nlohmann::json{{"city", "Paris"}}));

  const auto& results = request.GetMessages()[2];
  EXPECT_EQ(results.GetRole(),
            Aws::BedrockRuntime::Model::ConversationRole::user);
  ASSERT_EQ(results.GetContent().size(), 1);
  const auto& tool_result = results.GetContent()[0].GetToolResult();
  EXPECT_EQ(tool_result.GetToolUseId(), "call_1");
  EXPECT_EQ(tool_result.GetStatus(),
            Aws::BedrockRuntime::Model::ToolResultStatus::success);
  ASSERT_EQ(tool_result.GetContent().size(), 1);
  EXPECT_EQ(BedrockRequestMapper::from_document(
                tool_result.GetContent()[0].GetJson()),
            (nlohmann::json{{"temperature", 20}}));

  // With tool calls in the conversation the tools are sent even for "none"
  options.tool_choice = ToolChoice::none();
  request = mapper_.map_to_converse_request(options);
  EXPECT_TRUE(request.ToolConfigHasBeenSet());
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
