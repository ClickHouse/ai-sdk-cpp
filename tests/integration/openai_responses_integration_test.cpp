#include "../utils/test_fixtures.h"
#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Note: These tests connect to the real OpenAI Responses API when
// OPENAI_API_KEY is set. Otherwise they are skipped.

namespace ai {
namespace test {

namespace {

Tool make_weather_tool(std::atomic<int>* calls = nullptr) {
  return create_simple_tool(
      "weather", "Get the current weather for a city", {{"location", "string"}},
      [calls](const JsonValue& args, const ToolExecutionContext&) {
        if (calls != nullptr) {
          ++*calls;
        }
        return JsonValue{{"location", args["location"]},
                         {"temperature_fahrenheit", 72},
                         {"condition", "Sunny"}};
      });
}

Tool make_population_tool(std::atomic<int>* calls) {
  return create_simple_tool(
      "population", "Get the population of a city", {{"city", "string"}},
      [calls](const JsonValue& args, const ToolExecutionContext&) {
        ++*calls;
        return JsonValue{{"city", args["city"]}, {"population", 2100000}};
      });
}

bool has_reasoning_part(const Messages& messages) {
  return std::any_of(
      messages.begin(), messages.end(), [](const Message& message) {
        return std::any_of(
            message.content.begin(), message.content.end(),
            [](const ContentPart& part) {
              return std::holds_alternative<ProviderContentPart>(part);
            });
      });
}

}  // namespace

class OpenAIResponsesIntegrationTest : public AITestFixture {
 protected:
  void SetUp() override {
    AITestFixture::SetUp();
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (api_key && *api_key != '\0') {
      client_ = ai::openai::create_responses_client(api_key);
    }
  }

  std::optional<ai::Client> client_;
};

TEST_F(OpenAIResponsesIntegrationTest, BasicTextGeneration) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt6Luna,
                          "Reply with a short greeting.");
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_GT(result.usage.total_tokens, 0);
  ASSERT_TRUE(result.id.has_value());
  EXPECT_THAT(*result.id, testing::StartsWith("resp_"));
}

TEST_F(OpenAIResponsesIntegrationTest, InstructionsAndSamplingParameters) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  // GPT-6.2 rejects sampling parameters and the "none" effort; the request
  // must still succeed.
  GenerateOptions options(ai::openai::models::kGpt62Sol,
                          "You always answer in French.",
                          "Hello, how are you?");
  options.temperature = 0.3;
  options.max_tokens = 400;
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
}

TEST_F(OpenAIResponsesIntegrationTest, MaxOutputTokensFinishesWithLength) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt6Luna,
                          "Write a 500-word essay about the sea.");
  options.max_tokens = 16;
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_EQ(result.finish_reason, kFinishReasonLength);
}

TEST_F(OpenAIResponsesIntegrationTest, StructuredOutput) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  const JsonValue format = {
      {"type", "json_schema"},
      {"json_schema",
       {{"name", "city"},
        {"strict", true},
        {"schema",
         {{"type", "object"},
          {"properties",
           {{"name", {{"type", "string"}}}, {"country", {{"type", "string"}}}}},
          {"required", {"name", "country"}},
          {"additionalProperties", false}}}}}};
  GenerateOptions options(ai::openai::models::kGpt6Luna, "",
                          "Name the capital of France.",
                          std::optional<JsonValue>(std::in_place, format));
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  const auto json = JsonValue::parse(result.text);
  EXPECT_TRUE(json.contains("name"));
  EXPECT_TRUE(json.contains("country"));
}

TEST_F(OpenAIResponsesIntegrationTest, ToolCallingWithGpt62Sol) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(ai::openai::models::kGpt62Sol,
                          "What's the weather like in Paris?");
  options.tools["weather"] = make_weather_tool();
  options.tool_choice = ToolChoice::required();
  options.max_tokens = 1000;
  auto result = client_->generate_text(options);

  // A single step stops at the tool calls, so there is no text to expect.
  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.finish_reason, kFinishReasonToolCalls);
  ASSERT_FALSE(result.tool_calls.empty());
  EXPECT_EQ(result.tool_calls[0].tool_name, "weather");
  EXPECT_TRUE(result.tool_calls[0].arguments.contains("location"));
  ASSERT_EQ(result.tool_results.size(), result.tool_calls.size());
  EXPECT_TRUE(result.tool_results[0].is_success());
}

TEST_F(OpenAIResponsesIntegrationTest, MultiStepToolLoopWithReasoning) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  std::atomic<int> weather_calls{0};
  std::atomic<int> population_calls{0};
  GenerateOptions options(
      ai::openai::models::kGpt62Sol,
      "Use the tools to find the weather in Paris and the population of "
      "Paris, then answer in one sentence that includes both the "
      "temperature and the population.");
  options.tools["weather"] = make_weather_tool(&weather_calls);
  options.tools["population"] = make_population_tool(&population_calls);
  options.max_steps = 5;
  options.max_tokens = 2000;
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_GE(result.steps.size(), 2u);
  EXPECT_GE(weather_calls.load(), 1);
  EXPECT_GE(population_calls.load(), 1);
  EXPECT_THAT(result.text, testing::HasSubstr("72"));
  EXPECT_FALSE(result.response_messages.empty());
  if (!has_reasoning_part(result.response_messages)) {
    // Not a failure: the model may skip reasoning at the lowest effort.
    std::cout << "No reasoning items were returned to replay\n";
  }

  // The continuation, reasoning items included, is accepted as the input of
  // the next request.
  Messages conversation = {Message::user(options.prompt)};
  conversation.insert(conversation.end(), result.response_messages.begin(),
                      result.response_messages.end());
  conversation.push_back(Message::user("And what is the weather in Paris?"));
  GenerateOptions follow_up(ai::openai::models::kGpt62Sol,
                            std::move(conversation));
  follow_up.tools = options.tools;
  follow_up.max_steps = 3;
  follow_up.max_tokens = 2000;
  auto follow_up_result = client_->generate_text(follow_up);
  TestAssertions::assertSuccess(follow_up_result);
  EXPECT_FALSE(follow_up_result.text.empty());
}

TEST_F(OpenAIResponsesIntegrationTest, StreamingText) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions generate_options(ai::openai::models::kGpt6Luna,
                                   "Count from 1 to 5.");
  auto stream = client_->stream_text(StreamOptions(generate_options));

  std::string text;
  std::optional<StreamEvent> finish;
  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      text += event.text_delta;
    } else if (event.is_error()) {
      FAIL() << "Stream error: " << event.error.value_or("");
    } else if (event.is_finish()) {
      finish = event;
      break;
    }
  }

  EXPECT_THAT(text, testing::HasSubstr("5"));
  ASSERT_TRUE(finish.has_value());
  EXPECT_EQ(finish->finish_reason, kFinishReasonStop);
  ASSERT_TRUE(finish->usage.has_value());
  EXPECT_GT(finish->usage->total_tokens, 0);
}

TEST_F(OpenAIResponsesIntegrationTest, StreamingToolCall) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions generate_options(ai::openai::models::kGpt6Luna,
                                   "What's the weather like in Tokyo?");
  generate_options.tools["weather"] = make_weather_tool();
  generate_options.tool_choice = ToolChoice::required();
  auto stream = client_->stream_text(StreamOptions(generate_options));

  std::optional<ToolCall> tool_call;
  std::optional<StreamEvent> finish;
  for (const auto& event : stream) {
    if (event.is_tool_call()) {
      tool_call = event.tool_call;
    } else if (event.is_error()) {
      FAIL() << "Stream error: " << event.error.value_or("");
    } else if (event.is_finish()) {
      finish = event;
      break;
    }
  }

  ASSERT_TRUE(tool_call.has_value());
  EXPECT_EQ(tool_call->tool_name, "weather");
  EXPECT_TRUE(tool_call->arguments.contains("location"));
  ASSERT_TRUE(finish.has_value());
  EXPECT_EQ(finish->finish_reason, kFinishReasonToolCalls);
}

TEST_F(OpenAIResponsesIntegrationTest, InvalidApiKey) {
  if (!client_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  auto client = ai::openai::create_responses_client("sk-invalid");
  auto result = client.generate_text(
      GenerateOptions(ai::openai::models::kGpt6Luna, "Hello"));
  EXPECT_FALSE(result.is_success());
  EXPECT_FALSE(result.error_message().empty());
}

}  // namespace test
}  // namespace ai
