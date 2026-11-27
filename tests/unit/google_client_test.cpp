#include "providers/google/google_client.h"

#include "ai/errors.h"
#include "ai/google.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "providers/google/google_request_builder.h"
#include "providers/google/google_response_parser.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

namespace {

Tool make_weather_tool() {
  return Tool("Get the weather",
              {{"type", "object"},
               {"properties", {{"location", {{"type", "string"}}}}},
               {"required", {"location"}}},
              [](const JsonValue& args, const ToolExecutionContext&) {
                return JsonValue{{"location", args["location"]},
                                 {"temperature", 72}};
              });
}

nlohmann::json make_text_response(const std::string& text) {
  return {{"candidates",
           {{{"content", {{"role", "model"}, {"parts", {{{"text", text}}}}}},
             {"finishReason", "STOP"},
             {"index", 0}}}},
          {"usageMetadata",
           {{"promptTokenCount", 8},
            {"candidatesTokenCount", 5},
            {"thoughtsTokenCount", 20},
            {"totalTokenCount", 33}}},
          {"modelVersion", "gemini-4-hydrogen"},
          {"responseId", "resp-1"}};
}

nlohmann::json make_function_call_response() {
  return {{"candidates",
           {{{"content",
              {{"role", "model"},
               {"parts",
                {{{"functionCall",
                   {{"id", "fc-1"},
                    {"name", "weather"},
                    {"args", {{"location", "Paris"}}}}},
                  {"thoughtSignature", "sig-A"}},
                 {{"functionCall",
                   {{"id", "fc-2"},
                    {"name", "weather"},
                    {"args", {{"location", "London"}}}}}}}}}},
             {"finishReason", "STOP"}}}},
          {"usageMetadata",
           {{"promptTokenCount", 40},
            {"candidatesTokenCount", 12},
            {"totalTokenCount", 52}}}};
}

std::string env_or_empty(const char* name) {
  const char* value = std::getenv(name);
  return value ? value : "";
}

/// Set an environment variable for the lifetime of the object.
class ScopedEnv {
 public:
  ScopedEnv(const char* name, const char* value)
      : name_(name),
        previous_(std::getenv(name) != nullptr
                      ? std::optional<std::string>(env_or_empty(name))
                      : std::nullopt) {
    if (value) {
      setenv(name, value, 1);
    } else {
      unsetenv(name);
    }
  }
  ~ScopedEnv() {
    if (previous_) {
      setenv(name_, previous_->c_str(), 1);
    } else {
      unsetenv(name_);
    }
  }
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

 private:
  const char* name_;
  std::optional<std::string> previous_;
};

}  // namespace

// Request building

TEST(GoogleRequestTest, BuildsRequestFromPrompt) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(google::models::kGemini35FlashLite, "Be brief.",
                          "Hello");
  options.max_tokens = 64;
  options.temperature = 0.3;
  options.top_p = 0.9;
  options.seed = 7;

  const auto request = builder.build_request_json(options);

  EXPECT_FALSE(request.contains("model"));
  ASSERT_EQ(request["contents"].size(), 1u);
  EXPECT_EQ(request["contents"][0]["role"], "user");
  EXPECT_EQ(request["contents"][0]["parts"][0]["text"], "Hello");
  EXPECT_EQ(request["systemInstruction"]["parts"][0]["text"], "Be brief.");
  const auto& config = request["generationConfig"];
  EXPECT_EQ(config["maxOutputTokens"], 64);
  EXPECT_DOUBLE_EQ(config["temperature"].get<double>(), 0.3);
  EXPECT_DOUBLE_EQ(config["topP"].get<double>(), 0.9);
  EXPECT_EQ(config["seed"], 7);
  EXPECT_FALSE(request.contains("tools"));
}

TEST(GoogleRequestTest, OmitsSamplingParametersForRecentModels) {
  google::GoogleRequestBuilder builder;
  for (const auto* model :
       {google::models::kGemini4Carbon, google::models::kGemini4Neon,
        google::models::kGemini4Hydrogen, google::models::kGemini4Argon,
        google::models::kGemini38Flash, google::models::kGemini36Flash,
        "models/gemini-4-neon"}) {
    GenerateOptions options(model, "Hello");
    options.temperature = 0.7;
    options.top_p = 0.8;

    const auto request = builder.build_request_json(options);

    EXPECT_FALSE(request.contains("generationConfig")) << model;
  }

  // Earlier models still accept them.
  for (const auto* model :
       {google::models::kGemini35FlashLite, google::models::kGemini31FlashLite,
        "gemini-2.5-flash"}) {
    GenerateOptions options(model, "Hello");
    options.temperature = 0.7;
    const auto request = builder.build_request_json(options);
    EXPECT_TRUE(request["generationConfig"].contains("temperature")) << model;
  }
}

TEST(GoogleRequestTest, MapsRolesAndSystemMessages) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(
      google::models::kDefaultModel,
      Messages{Message::system("Be terse."), Message::user("What is 2 + 2?"),
               Message::assistant("4."), Message::user("And 4 + 4?")});
  options.system = "You are a math tutor.";

  const auto request = builder.build_request_json(options);

  const auto& system_parts = request["systemInstruction"]["parts"];
  ASSERT_EQ(system_parts.size(), 2u);
  EXPECT_EQ(system_parts[0]["text"], "You are a math tutor.");
  EXPECT_EQ(system_parts[1]["text"], "Be terse.");

  const auto& contents = request["contents"];
  ASSERT_EQ(contents.size(), 3u);
  EXPECT_EQ(contents[0]["role"], "user");
  EXPECT_EQ(contents[1]["role"], "model");
  EXPECT_EQ(contents[1]["parts"][0]["text"], "4.");
  EXPECT_EQ(contents[2]["role"], "user");
}

TEST(GoogleRequestTest, BuildsFunctionDeclarationsAndToolConfig) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(google::models::kDefaultModel, "Weather?");
  options.tools = {{"weather", make_weather_tool()}};

  auto request = builder.build_request_json(options);
  ASSERT_EQ(request["tools"].size(), 1u);
  const auto& declarations = request["tools"][0]["functionDeclarations"];
  ASSERT_EQ(declarations.size(), 1u);
  EXPECT_EQ(declarations[0]["name"], "weather");
  EXPECT_EQ(declarations[0]["description"], "Get the weather");
  EXPECT_EQ(declarations[0]["parametersJsonSchema"]["required"][0], "location");
  EXPECT_FALSE(declarations[0].contains("parameters"));
  EXPECT_EQ(request["toolConfig"]["functionCallingConfig"]["mode"], "AUTO");

  options.tool_choice = ToolChoice::required();
  request = builder.build_request_json(options);
  EXPECT_EQ(request["toolConfig"]["functionCallingConfig"]["mode"], "ANY");
  EXPECT_FALSE(request["toolConfig"]["functionCallingConfig"].contains(
      "allowedFunctionNames"));

  options.tool_choice = ToolChoice::none();
  request = builder.build_request_json(options);
  EXPECT_EQ(request["toolConfig"]["functionCallingConfig"]["mode"], "NONE");

  options.tool_choice = ToolChoice::specific("weather");
  request = builder.build_request_json(options);
  const auto& config = request["toolConfig"]["functionCallingConfig"];
  EXPECT_EQ(config["mode"], "ANY");
  EXPECT_EQ(config["allowedFunctionNames"], nlohmann::json::array({"weather"}));
}

TEST(GoogleRequestTest, MapsResponseFormat) {
  google::GoogleRequestBuilder builder;
  const nlohmann::json schema = {
      {"type", "object"},
      {"properties", {{"name", {{"type", "string"}}}}},
      {"required", {"name"}}};

  GenerateOptions chat_shape(google::models::kDefaultModel, "Name?");
  chat_shape.response_format = {
      {"type", "json_schema"},
      {"json_schema", {{"name", "person"}, {"schema", schema}}}};
  auto request = builder.build_request_json(chat_shape);
  EXPECT_EQ(request["generationConfig"]["responseMimeType"],
            "application/json");
  EXPECT_EQ(request["generationConfig"]["responseJsonSchema"], schema);

  GenerateOptions flat_shape(google::models::kDefaultModel, "Name?");
  flat_shape.response_format = {
      {"type", "json_schema"}, {"name", "person"}, {"schema", schema}};
  request = builder.build_request_json(flat_shape);
  EXPECT_EQ(request["generationConfig"]["responseJsonSchema"], schema);

  GenerateOptions json_object(google::models::kDefaultModel, "Name?");
  json_object.response_format =
      nlohmann::json::object({{"type", "json_object"}});
  request = builder.build_request_json(json_object);
  EXPECT_EQ(request["generationConfig"]["responseMimeType"],
            "application/json");
  EXPECT_FALSE(request["generationConfig"].contains("responseJsonSchema"));

  GenerateOptions unsupported(google::models::kDefaultModel, "Name?");
  unsupported.response_format = nlohmann::json::object({{"type", "xml"}});
  EXPECT_THROW(builder.build_request_json(unsupported), std::invalid_argument);
}

TEST(GoogleRequestTest, ReplaysParsedTurnWithThoughtSignatures) {
  google::GoogleResponseParser parser;
  google::GoogleRequestBuilder builder;
  auto step =
      parser.parse_success_completion_response(make_function_call_response());
  ASSERT_EQ(step.response_messages.size(), 1u);

  Messages messages = {Message::user("Weather in Paris and London?")};
  messages.push_back(step.response_messages[0]);
  messages.push_back(Message::tool_results(
      {ToolResultContentPart("fc-1", {{"temperature", 18}}),
       ToolResultContentPart("fc-2", "cloudy"),
       ToolResultContentPart("fc-2", {{"error", "timeout"}}, true)}));
  GenerateOptions options(google::models::kDefaultModel, std::move(messages));

  const auto request = builder.build_request_json(options);
  const auto& contents = request["contents"];
  ASSERT_EQ(contents.size(), 3u);

  // The model turn comes back exactly as received: the signature stays on
  // the first call only.
  const auto& model_parts = contents[1]["parts"];
  EXPECT_EQ(contents[1]["role"], "model");
  ASSERT_EQ(model_parts.size(), 2u);
  EXPECT_EQ(model_parts[0]["thoughtSignature"], "sig-A");
  EXPECT_EQ(model_parts[0]["functionCall"]["id"], "fc-1");
  EXPECT_EQ(model_parts[0]["functionCall"]["name"], "weather");
  EXPECT_EQ(model_parts[0]["functionCall"]["args"]["location"], "Paris");
  EXPECT_FALSE(model_parts[1].contains("thoughtSignature"));
  EXPECT_EQ(model_parts[1]["functionCall"]["id"], "fc-2");

  // The results take the function name from the call they answer.
  const auto& result_parts = contents[2]["parts"];
  EXPECT_EQ(contents[2]["role"], "user");
  ASSERT_EQ(result_parts.size(), 3u);
  EXPECT_EQ(result_parts[0]["functionResponse"]["name"], "weather");
  EXPECT_EQ(result_parts[0]["functionResponse"]["id"], "fc-1");
  EXPECT_EQ(result_parts[0]["functionResponse"]["response"]["temperature"], 18);
  EXPECT_EQ(result_parts[1]["functionResponse"]["response"]["output"],
            "cloudy");
  EXPECT_EQ(result_parts[2]["functionResponse"]["response"]["error"],
            "timeout");
}

TEST(GoogleRequestTest, UsesSkipSignatureForCallsWithoutSignature) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(
      google::models::kDefaultModel,
      Messages{Message::user("Weather?"),
               Message::assistant_with_tools(
                   "Checking.", {ToolCallContentPart("call_1", "weather",
                                                     {{"location", "Paris"}}),
                                 ToolCallContentPart("call_2", "weather",
                                                     {{"location", "Rome"}})}),
               Message::tool_results(
                   {ToolResultContentPart("call_1", {{"temperature", 18}}),
                    ToolResultContentPart("call_2", {{"temperature", 25}})})});

  const auto request = builder.build_request_json(options);
  const auto& model_parts = request["contents"][1]["parts"];
  ASSERT_EQ(model_parts.size(), 3u);
  EXPECT_EQ(model_parts[0]["text"], "Checking.");
  EXPECT_FALSE(model_parts[0].contains("thoughtSignature"));
  EXPECT_EQ(model_parts[1]["thoughtSignature"],
            google::kSkipThoughtSignatureValidator);
  EXPECT_FALSE(model_parts[2].contains("thoughtSignature"));
}

TEST(GoogleRequestTest, OmitsSyntheticCallIds) {
  google::GoogleResponseParser parser;
  google::GoogleRequestBuilder builder;
  // Gemini 2.5 returns function calls without an `id`.
  auto step = parser.parse_success_completion_response(
      {{"candidates",
        {{{"content",
           {{"role", "model"},
            {"parts",
             {{{"functionCall",
                {{"name", "weather"}, {"args", {{"location", "Oslo"}}}}}}}}}},
          {"finishReason", "STOP"}}}}});
  ASSERT_EQ(step.tool_calls.size(), 1u);
  const auto id = step.tool_calls[0].id;
  EXPECT_THAT(id, testing::StartsWith(google::kSyntheticCallIdPrefix));

  Messages messages = {
      Message::user("Weather in Oslo?"), step.response_messages[0],
      Message::tool_results({ToolResultContentPart(id, {{"temperature", 3}})})};
  const auto request = builder.build_request_json(
      GenerateOptions(google::models::kGemini35FlashLite, std::move(messages)));

  const auto& call = request["contents"][1]["parts"][0]["functionCall"];
  EXPECT_FALSE(call.contains("id"));
  const auto& response = request["contents"][2]["parts"][0]["functionResponse"];
  EXPECT_FALSE(response.contains("id"));
  EXPECT_EQ(response["name"], "weather");
}

TEST(GoogleRequestTest, RejectsToolResultWithoutMatchingCall) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(
      google::models::kDefaultModel,
      Messages{Message::user("Weather?"),
               Message::tool_results({ToolResultContentPart("call_x", {})})});
  EXPECT_THROW(builder.build_request_json(options), std::invalid_argument);
}

TEST(GoogleRequestTest, IgnoresOtherProvidersParts) {
  google::GoogleRequestBuilder builder;
  GenerateOptions options(
      google::models::kDefaultModel,
      Messages{Message::user("Hi"),
               Message(kMessageRoleAssistant,
                       {ProviderContentPart("openai.responses",
                                            {{"type", "reasoning"}}),
                        TextContentPart("Hello!")})});

  const auto request = builder.build_request_json(options);
  const auto& model_parts = request["contents"][1]["parts"];
  ASSERT_EQ(model_parts.size(), 1u);
  EXPECT_EQ(model_parts[0], nlohmann::json({{"text", "Hello!"}}));
}

// Response parsing

TEST(GoogleParserTest, ParsesTextResponse) {
  google::GoogleResponseParser parser;
  const auto result =
      parser.parse_success_completion_response(make_text_response("Hi!"));

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.text, "Hi!");
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_EQ(result.id, "resp-1");
  EXPECT_EQ(result.model, "gemini-4-hydrogen");
  EXPECT_EQ(result.usage.prompt_tokens, 8);
  EXPECT_EQ(result.usage.completion_tokens, 25);  // answer + thinking
  EXPECT_EQ(result.usage.total_tokens, 33);
  ASSERT_EQ(result.response_messages.size(), 1u);
  EXPECT_EQ(result.response_messages[0].get_text(), "Hi!");
}

TEST(GoogleParserTest, KeepsThoughtsAndSignaturesOutOfText) {
  google::GoogleResponseParser parser;
  const nlohmann::json thought = {{"text", "Let me think."}, {"thought", true}};
  const nlohmann::json signature_only = {{"text", ""},
                                         {"thoughtSignature", "sig-T"}};
  const auto result = parser.parse_success_completion_response(
      {{"candidates",
        {{{"content",
           {{"role", "model"},
            {"parts",
             {thought,
              {{"text", "The answer is 42."}, {"thoughtSignature", "sig-X"}},
              signature_only}}}},
          {"finishReason", "STOP"}}}}});

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.text, "The answer is 42.");
  ASSERT_EQ(result.response_messages.size(), 1u);

  // Replaying the turn restores every part as received.
  google::GoogleRequestBuilder builder;
  Messages messages = {Message::user("Question"), result.response_messages[0],
                       Message::user("Thanks")};
  const auto request = builder.build_request_json(
      GenerateOptions(google::models::kDefaultModel, std::move(messages)));
  const auto& parts = request["contents"][1]["parts"];
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0], thought);
  EXPECT_EQ(parts[1], nlohmann::json({{"text", "The answer is 42."},
                                      {"thoughtSignature", "sig-X"}}));
  EXPECT_EQ(parts[2], signature_only);
}

TEST(GoogleParserTest, ParsesFunctionCalls) {
  google::GoogleResponseParser parser;
  const auto result =
      parser.parse_success_completion_response(make_function_call_response());

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.finish_reason, kFinishReasonToolCalls);
  ASSERT_EQ(result.tool_calls.size(), 2u);
  EXPECT_EQ(result.tool_calls[0].id, "fc-1");
  EXPECT_EQ(result.tool_calls[0].tool_name, "weather");
  EXPECT_EQ(result.tool_calls[0].arguments["location"], "Paris");
  EXPECT_EQ(result.tool_calls[1].id, "fc-2");
  ASSERT_EQ(result.response_messages.size(), 1u);
  EXPECT_TRUE(result.response_messages[0].has_tool_calls());
}

TEST(GoogleParserTest, MapsFinishReasons) {
  EXPECT_EQ(google::parse_finish_reason("STOP", false), kFinishReasonStop);
  EXPECT_EQ(google::parse_finish_reason("STOP", true), kFinishReasonToolCalls);
  EXPECT_EQ(google::parse_finish_reason("MAX_TOKENS", false),
            kFinishReasonLength);
  EXPECT_EQ(google::parse_finish_reason("MAX_TOKENS", true),
            kFinishReasonToolCalls);
  EXPECT_EQ(google::parse_finish_reason("CONTINUATION", false),
            kFinishReasonLength);
  for (const auto* reason :
       {"SAFETY", "RECITATION", "BLOCKLIST", "PROHIBITED_CONTENT", "SPII"}) {
    EXPECT_EQ(google::parse_finish_reason(reason, false),
              kFinishReasonContentFilter)
        << reason;
  }
  for (const auto* reason :
       {"MALFORMED_FUNCTION_CALL", "MISSING_THOUGHT_SIGNATURE", "OTHER",
        "FINISH_REASON_UNSPECIFIED", ""}) {
    EXPECT_FALSE(google::parse_finish_reason(reason, false).has_value())
        << reason;
  }
}

TEST(GoogleParserTest, ReportsMaxTokensAndSafety) {
  google::GoogleResponseParser parser;
  auto response = make_text_response("Truncated");
  response["candidates"][0]["finishReason"] = "MAX_TOKENS";
  auto result = parser.parse_success_completion_response(response);
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonLength);

  result = parser.parse_success_completion_response(
      {{"candidates", {{{"finishReason", "SAFETY"}}}}});
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonContentFilter);
  EXPECT_TRUE(result.text.empty());
}

TEST(GoogleParserTest, ReportsFailedFinishReasonAsError) {
  google::GoogleResponseParser parser;
  const auto result = parser.parse_success_completion_response(
      {{"candidates",
        {{{"finishReason", "MALFORMED_FUNCTION_CALL"},
          {"finishMessage", "Malformed function call: weather("}}}}});
  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonError);
  EXPECT_THAT(result.error_message(),
              testing::HasSubstr("MALFORMED_FUNCTION_CALL"));
  EXPECT_THAT(result.error_message(), testing::HasSubstr("weather("));
}

TEST(GoogleParserTest, ReportsBlockedPrompt) {
  google::GoogleResponseParser parser;
  const auto result = parser.parse_success_completion_response(
      {{"promptFeedback", {{"blockReason", "SAFETY"}}},
       {"usageMetadata", {{"promptTokenCount", 5}, {"totalTokenCount", 5}}}});
  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonContentFilter);
  EXPECT_THAT(result.error_message(), testing::HasSubstr("SAFETY"));
}

TEST(GoogleParserTest, ParsesGoogleErrors) {
  google::GoogleResponseParser parser;
  const auto result = parser.parse_error_completion_response(
      400,
      R"({"error":{"code":400,"message":"API key not valid.","status":"INVALID_ARGUMENT"}})");
  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("Google API error"));
  EXPECT_THAT(result.error_message(), testing::HasSubstr("API key not valid."));
  EXPECT_THAT(result.error_message(), testing::HasSubstr("INVALID_ARGUMENT"));
  EXPECT_FALSE(result.is_retryable.value_or(true));

  EXPECT_TRUE(
      parser.parse_error_completion_response(503, "{}").is_retryable.value_or(
          false));
}

// Multi-step tool loop

TEST(GoogleMultiStepTest, FollowUpRequestReplaysThoughtSignature) {
  google::GoogleRequestBuilder builder;
  google::GoogleResponseParser parser;
  GenerateOptions options(google::models::kDefaultModel,
                          "Weather in Paris and London?");
  options.tools = {{"weather", make_weather_tool()}};
  options.max_steps = 3;

  std::vector<nlohmann::json> requests;
  auto result = MultiStepCoordinator::execute_multi_step(
      options, [&](const GenerateOptions& step_options) {
        requests.push_back(builder.build_request_json(step_options));
        if (requests.size() == 1) {
          auto step = parser.parse_success_completion_response(
              make_function_call_response());
          step.tool_results = ToolExecutor::execute_tools_with_options(
              step.tool_calls, step_options, false);
          return step;
        }
        return parser.parse_success_completion_response(
            make_text_response("Both are 72F."));
      });

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.text, "Both are 72F.");
  ASSERT_EQ(requests.size(), 2u);
  const auto& contents = requests[1]["contents"];
  ASSERT_EQ(contents.size(), 3u);
  EXPECT_EQ(contents[1]["parts"][0]["thoughtSignature"], "sig-A");
  EXPECT_EQ(contents[1]["parts"][1]["functionCall"]["args"]["location"],
            "London");
  ASSERT_EQ(contents[2]["parts"].size(), 2u);
  EXPECT_EQ(contents[2]["parts"][0]["functionResponse"]["id"], "fc-1");
  EXPECT_EQ(contents[2]["parts"][0]["functionResponse"]["response"]["location"],
            "Paris");
  EXPECT_EQ(contents[2]["parts"][1]["functionResponse"]["id"], "fc-2");

  ASSERT_EQ(result.response_messages.size(), 3u);
  EXPECT_EQ(result.response_messages.back().get_text(), "Both are 72F.");
}

// Client

TEST(GoogleClientTest, UsesModelPathAndDefaults) {
  google::GoogleClient client("test-key");
  EXPECT_EQ(client.provider_name(), "google");
  EXPECT_EQ(client.default_model(), google::models::kDefaultModel);
  EXPECT_EQ(client.get_base_url(), google::kDefaultBaseUrl);
  EXPECT_THAT(client.config_info(), testing::HasSubstr("Gemini"));
  EXPECT_TRUE(client.is_valid());
  EXPECT_TRUE(client.supports_model(google::models::kGemini4Neon));
  EXPECT_TRUE(client.supports_model("gemini-some-future-model"));
  EXPECT_FALSE(client.supports_model(""));
  EXPECT_FALSE(google::GoogleClient("").is_valid());

  EXPECT_EQ(
      google::GoogleClient::model_path("gemini-4-neon", "generateContent"),
      "/v1beta/models/gemini-4-neon:generateContent");
  EXPECT_EQ(google::GoogleClient::model_path("models/gemini-4-neon",
                                             "streamGenerateContent"),
            "/v1beta/models/gemini-4-neon:streamGenerateContent");
}

TEST(GoogleClientTest, AuthenticatesWithGoogApiKeyHeader) {
  google::GoogleRequestBuilder builder;
  providers::ProviderConfig config{.api_key = "test-key",
                                   .auth_header_name = "x-goog-api-key"};
  const auto headers = builder.build_headers(config);
  const auto it = headers.find("x-goog-api-key");
  ASSERT_NE(it, headers.end());
  EXPECT_EQ(it->second, "test-key");
}

TEST(GoogleClientTest, EmbeddingsAreNotSupported) {
  google::GoogleClient client("test-key");
  const auto result =
      client.embeddings(EmbeddingOptions("gemini-embedding-2", "text"));
  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("not supported"));
}

TEST(GoogleClientTest, FactoryReadsApiKeyFromEnvironment) {
  {
    ScopedEnv google_key("GOOGLE_API_KEY", nullptr);
    ScopedEnv gemini_key("GEMINI_API_KEY", nullptr);
    EXPECT_FALSE(google::try_create_client().has_value());
    EXPECT_THROW(google::create_client(), ConfigurationError);
  }
  {
    ScopedEnv google_key("GOOGLE_API_KEY", nullptr);
    ScopedEnv gemini_key("GEMINI_API_KEY", "gemini-key");
    auto client = google::try_create_client();
    ASSERT_TRUE(client.has_value());
    EXPECT_EQ(client->provider_name(), "google");
    EXPECT_TRUE(client->is_valid());
  }
  {
    ScopedEnv google_key("GOOGLE_API_KEY", "google-key");
    ScopedEnv gemini_key("GEMINI_API_KEY", nullptr);
    EXPECT_TRUE(google::create_client().is_valid());
  }
}

}  // namespace test
}  // namespace ai
