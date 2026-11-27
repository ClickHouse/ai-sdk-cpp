#include "../utils/test_fixtures.h"
#include "ai/google.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <optional>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Note: These tests connect to the real Gemini API when GEMINI_API_KEY (or
// GOOGLE_API_KEY) is set. Otherwise they are skipped.

namespace ai {
namespace test {

namespace {

// Gemini 3.5 Flash-Lite thinks at the `minimal` level by default, so small
// `max_tokens` values are not used up by thinking.
constexpr const char* kTestModel = ai::google::models::kGemini35FlashLite;

}  // namespace

class GoogleIntegrationTest : public AITestFixture {
 protected:
  void SetUp() override {
    AITestFixture::SetUp();
    client_ = ai::google::try_create_client();
  }

  bool use_real_api() const { return client_.has_value(); }

  std::optional<ai::Client> client_;
};

#define SKIP_WITHOUT_GEMINI_KEY()                                 \
  if (!use_real_api()) {                                          \
    GTEST_SKIP() << "No GEMINI_API_KEY environment variable set"; \
  }

TEST_F(GoogleIntegrationTest, ListModels) {
  SKIP_WITHOUT_GEMINI_KEY();

  const auto result = client_->list_models();
  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_THAT(result.models, testing::Contains(testing::StartsWith("gemini-")));
}

TEST_F(GoogleIntegrationTest, BasicTextGeneration) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions options(kTestModel, "Hello, how are you?");
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_GT(result.usage.total_tokens, 0);
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
}

TEST_F(GoogleIntegrationTest, TextGenerationWithSystemPrompt) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions options(
      kTestModel, "You are a helpful assistant that responds in French.",
      "Hello, how are you?");
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_THAT(
      result.text,
      testing::AnyOf(testing::HasSubstr("Bonjour"), testing::HasSubstr("salut"),
                     testing::HasSubstr("ça va"), testing::HasSubstr("bien"),
                     testing::HasSubstr("merci")));
}

TEST_F(GoogleIntegrationTest, TextGenerationWithParameters) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions options(kTestModel, "Write a very short story about a cat.");
  options.max_tokens = 50;
  options.temperature = 0.7;

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_THAT(result.finish_reason,
              testing::AnyOf(kFinishReasonStop, kFinishReasonLength));
  EXPECT_LE(result.text.length(), 400);
}

TEST_F(GoogleIntegrationTest, ConversationWithMessages) {
  SKIP_WITHOUT_GEMINI_KEY();

  Messages conversation = {
      Message::user("My name is Ada."),
      Message::assistant("Nice to meet you, Ada!"),
      Message::user("What is my name? Answer with the name only.")};

  GenerateOptions options(kTestModel, std::move(conversation));
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_THAT(result.text, testing::HasSubstr("Ada"));
}

TEST_F(GoogleIntegrationTest, StructuredOutput) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions options(kTestModel, "Name the capital of France.");
  options.response_format = {
      {"type", "json_schema"},
      {"json_schema",
       {{"name", "capital"},
        {"schema",
         {{"type", "object"},
          {"properties", {{"city", {{"type", "string"}}}}},
          {"required", {"city"}}}}}}};

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  const auto json = nlohmann::json::parse(result.text);
  EXPECT_THAT(json["city"].get<std::string>(), testing::HasSubstr("Paris"));
}

TEST_F(GoogleIntegrationTest, MultiStepToolCallingReplaysSignatures) {
  SKIP_WITHOUT_GEMINI_KEY();

  int calls = 0;
  GenerateOptions options(
      ai::google::models::kDefaultModel,
      "What is the weather in Paris? Use the weather tool, then answer in "
      "one sentence.");
  options.tools = {
      {"weather",
       create_simple_tool(
           "weather", "Get the current weather for a city",
           {{"location", "string"}},
           [&calls](const JsonValue& args, const ToolExecutionContext&) {
             ++calls;
             return JsonValue{{"location", args["location"]},
                              {"temperature_celsius", 21},
                              {"condition", "sunny"}};
           })}};
  options.max_steps = 4;

  auto result = client_->generate_text(options);

  // Gemini 4 rejects the second step with HTTP 400 if the thought signature
  // of the function call is not sent back.
  TestAssertions::assertSuccess(result);
  EXPECT_GE(calls, 1);
  EXPECT_GE(result.steps.size(), 2u);
  EXPECT_FALSE(result.text.empty());
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
}

TEST_F(GoogleIntegrationTest, InvalidApiKey) {
  auto invalid_client = ai::google::create_client("invalid-gemini-key");

  GenerateOptions options(kTestModel, "Test prompt");
  auto result = invalid_client.generate_text(options);

  TestAssertions::assertError(result);
  EXPECT_THAT(
      result.error_message(),
      testing::AnyOf(testing::HasSubstr("400"), testing::HasSubstr("401"),
                     testing::HasSubstr("403"), testing::HasSubstr("API key")));
}

TEST_F(GoogleIntegrationTest, InvalidModel) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions options("invalid-model-name", "Test prompt");
  auto result = client_->generate_text(options);

  TestAssertions::assertError(result);
  EXPECT_THAT(
      result.error_message(),
      testing::AnyOf(testing::HasSubstr("404"), testing::HasSubstr("400"),
                     testing::HasSubstr("not found")));
}

TEST_F(GoogleIntegrationTest, BasicStreaming) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions gen_options(kTestModel, "Count from 1 to 3");
  auto stream = client_->stream_text(StreamOptions(gen_options));

  bool stream_finished = false;
  std::string accumulated_text;
  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      accumulated_text += event.text_delta;
    } else if (event.is_finish()) {
      stream_finished = true;
      EXPECT_EQ(event.finish_reason, kFinishReasonStop);
      ASSERT_TRUE(event.usage.has_value());
      EXPECT_GT(event.usage->total_tokens, 0);
      break;
    } else if (event.is_error()) {
      FAIL() << "Streaming error: " << event.error.value_or("unknown");
    }
  }

  EXPECT_TRUE(stream_finished) << "Stream should have finished";
  EXPECT_THAT(accumulated_text, testing::HasSubstr("2"));
}

TEST_F(GoogleIntegrationTest, DefaultModelGeneration) {
  SKIP_WITHOUT_GEMINI_KEY();

  EXPECT_EQ(client_->default_model(), ai::google::models::kDefaultModel);
  GenerateOptions options(client_->default_model(), "Count to 3");
  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  if (result.model.has_value()) {
    EXPECT_THAT(*result.model,
                testing::HasSubstr(ai::google::models::kDefaultModel));
  }
}

TEST_F(GoogleIntegrationTest, DefaultModelStreaming) {
  SKIP_WITHOUT_GEMINI_KEY();

  GenerateOptions gen_options(client_->default_model(), "Count from 1 to 3");
  auto stream = client_->stream_text(StreamOptions(gen_options));

  bool stream_finished = false;
  std::string accumulated_text;
  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      accumulated_text += event.text_delta;
    } else if (event.is_finish()) {
      stream_finished = true;
      break;
    } else if (event.is_error()) {
      FAIL() << "Streaming error: " << event.error.value_or("unknown");
    }
  }

  EXPECT_TRUE(stream_finished) << "Stream should have finished";
  EXPECT_FALSE(accumulated_text.empty());
}

}  // namespace test
}  // namespace ai
