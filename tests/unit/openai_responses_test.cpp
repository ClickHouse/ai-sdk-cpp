#include "../utils/test_fixtures.h"
#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "providers/openai/openai_responses_client.h"
#include "providers/openai/openai_responses_request_builder.h"
#include "providers/openai/openai_responses_response_parser.h"
#include "providers/openai/openai_responses_stream.h"

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

const nlohmann::json kReasoningItem = {{"id", "rs_1"},
                                       {"type", "reasoning"},
                                       {"summary", nlohmann::json::array()},
                                       {"encrypted_content", "gAAAA-secret"}};

nlohmann::json make_tool_call_response() {
  return {{"id", "resp_1"},
          {"object", "response"},
          {"created_at", 1741487325},
          {"model", "gpt-6.2-sol"},
          {"status", "completed"},
          {"output",
           {kReasoningItem,
            {{"id", "fc_1"},
             {"type", "function_call"},
             {"status", "completed"},
             {"call_id", "call_1"},
             {"name", "weather"},
             {"arguments", R"({"location":"Paris"})"}}}},
          {"usage",
           {{"input_tokens", 75},
            {"output_tokens", 30},
            {"output_tokens_details", {{"reasoning_tokens", 12}}},
            {"total_tokens", 105}}}};
}

}  // namespace

// Request building

TEST(OpenAIResponsesRequestTest, BuildsStatelessRequestFromPrompt) {
  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt62Sol, "Be brief.", "Hello");
  options.max_tokens = 64;

  const auto request = builder.build_request_json(options);

  EXPECT_EQ(request["model"], "gpt-6.2-sol");
  EXPECT_EQ(request["store"], false);
  EXPECT_EQ(request["instructions"], "Be brief.");
  EXPECT_EQ(request["max_output_tokens"], 64);
  EXPECT_FALSE(request.contains("messages"));
  EXPECT_FALSE(request.contains("max_completion_tokens"));
  ASSERT_EQ(request["input"].size(), 1u);
  EXPECT_EQ(request["input"][0]["role"], "user");
  EXPECT_EQ(request["input"][0]["content"], "Hello");
}

TEST(OpenAIResponsesRequestTest, UsesLowestAcceptedReasoningEffort) {
  openai::OpenAIResponsesRequestBuilder builder;

  for (const auto* model :
       {openai::models::kGpt62Astra, openai::models::kGpt62Sol}) {
    const auto request = builder.build_request_json(GenerateOptions(model, "Hi"));
    EXPECT_EQ(request["reasoning"]["effort"], "low") << model;
  }
  for (const auto* model :
       {openai::models::kGpt61Terra, openai::models::kGpt6Luna,
        openai::models::kGpt56}) {
    const auto request = builder.build_request_json(GenerateOptions(model, "Hi"));
    EXPECT_EQ(request["reasoning"]["effort"], "none") << model;
  }
  for (const auto* model : {openai::models::kGpt54Pro, openai::models::kGpt41}) {
    const auto request = builder.build_request_json(GenerateOptions(model, "Hi"));
    EXPECT_FALSE(request.contains("reasoning")) << model;
  }
}

TEST(OpenAIResponsesRequestTest, OmitsUnsupportedParameters) {
  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt6Luna, "Hi");
  options.temperature = 0.2;
  options.top_p = 0.9;
  options.seed = 7;
  options.frequency_penalty = 0.1;
  options.presence_penalty = 0.1;

  const auto request = builder.build_request_json(options);
  for (const auto* key :
       {"temperature", "top_p", "seed", "frequency_penalty",
        "presence_penalty"}) {
    EXPECT_FALSE(request.contains(key)) << key;
  }

  GenerateOptions non_reasoning(openai::models::kGpt41, "Hi");
  non_reasoning.temperature = 0.2;
  non_reasoning.top_p = 0.9;
  const auto kept = builder.build_request_json(non_reasoning);
  EXPECT_EQ(kept["temperature"], 0.2);
  EXPECT_EQ(kept["top_p"], 0.9);
}

TEST(OpenAIResponsesRequestTest, BuildsFlatFunctionTools) {
  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt62Sol, "Weather in Paris?");
  options.tools["weather"] = make_weather_tool();

  auto request = builder.build_request_json(options);
  ASSERT_EQ(request["tools"].size(), 1u);
  const auto& tool = request["tools"][0];
  EXPECT_EQ(tool["type"], "function");
  EXPECT_EQ(tool["name"], "weather");
  EXPECT_EQ(tool["description"], "Get the weather");
  EXPECT_EQ(tool["parameters"]["required"][0], "location");
  EXPECT_EQ(tool["strict"], false);
  EXPECT_FALSE(tool.contains("function"));
  EXPECT_EQ(request["tool_choice"], "auto");

  options.tool_choice = ToolChoice::specific("weather");
  request = builder.build_request_json(options);
  EXPECT_EQ(request["tool_choice"],
            nlohmann::json({{"type", "function"}, {"name", "weather"}}));

  options.tool_choice = ToolChoice::required();
  EXPECT_EQ(builder.build_request_json(options)["tool_choice"], "required");
  options.tool_choice = ToolChoice::none();
  EXPECT_EQ(builder.build_request_json(options)["tool_choice"], "none");
}

TEST(OpenAIResponsesRequestTest, MapsResponseFormatToTextFormat) {
  openai::OpenAIResponsesRequestBuilder builder;
  const nlohmann::json schema = {
      {"type", "object"},
      {"properties", {{"name", {{"type", "string"}}}}},
      {"required", {"name"}},
      {"additionalProperties", false}};

  // The Chat Completions shape is flattened.
  GenerateOptions chat_shape(openai::models::kGpt62Sol, "", "Name a city",
                             nlohmann::json{{"type", "json_schema"},
                                            {"json_schema",
                                             {{"name", "city"},
                                              {"schema", schema},
                                              {"strict", true}}}});
  EXPECT_EQ(builder.build_request_json(chat_shape)["text"]["format"],
            nlohmann::json({{"type", "json_schema"},
                            {"name", "city"},
                            {"schema", schema},
                            {"strict", true}}));
  EXPECT_FALSE(builder.build_request_json(chat_shape).contains(
      "response_format"));

  GenerateOptions json_object(openai::models::kGpt62Sol, "", "Name a city",
                              nlohmann::json{{"type", "json_object"}});
  EXPECT_EQ(builder.build_request_json(json_object)["text"]["format"],
            nlohmann::json({{"type", "json_object"}}));
}

TEST(OpenAIResponsesRequestTest, ReplaysToolRoundTripWithReasoningInOrder) {
  openai::OpenAIResponsesResponseParser parser;
  auto step = parser.parse_success_completion_response(make_tool_call_response());
  ASSERT_EQ(step.response_messages.size(), 1u);

  Messages conversation = {Message::system("Use the tools."),
                           Message::user("Weather in Paris?")};
  conversation.push_back(step.response_messages[0]);
  conversation.push_back(Message::tool_results(
      {ToolResultContentPart("call_1", {{"temperature", 72}})}));

  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt62Sol, std::move(conversation));
  const auto input = builder.build_request_json(options)["input"];

  ASSERT_EQ(input.size(), 5u);
  EXPECT_EQ(input[0]["role"], "system");
  EXPECT_EQ(input[1]["role"], "user");
  // The reasoning item is replayed verbatim, before the call it led to.
  EXPECT_EQ(input[2], kReasoningItem);
  EXPECT_EQ(input[3], nlohmann::json({{"type", "function_call"},
                                      {"id", "fc_1"},
                                      {"call_id", "call_1"},
                                      {"name", "weather"},
                                      {"arguments", R"({"location":"Paris"})"}}));
  EXPECT_EQ(input[4], nlohmann::json({{"type", "function_call_output"},
                                      {"call_id", "call_1"},
                                      {"output", R"({"temperature":72})"}}));
}

TEST(OpenAIResponsesRequestTest, BuildsFunctionCallsFromPlainMessages) {
  // Messages built by hand (or by another provider) carry no provider parts.
  Messages conversation = {
      Message::user("Weather in Paris?"),
      Message::assistant_with_tools(
          "Checking.", {ToolCallContentPart("call_9", "weather",
                                            {{"location", "Paris"}})}),
      Message::tool_results({ToolResultContentPart(
          "call_9", {{"error", "unavailable"}}, /*err=*/true)})};

  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt6Luna, std::move(conversation));
  const auto input = builder.build_request_json(options)["input"];

  ASSERT_EQ(input.size(), 4u);
  EXPECT_EQ(input[1]["role"], "assistant");
  EXPECT_EQ(input[1]["content"], "Checking.");
  EXPECT_EQ(input[2]["type"], "function_call");
  EXPECT_FALSE(input[2].contains("id"));
  EXPECT_EQ(input[2]["call_id"], "call_9");
  EXPECT_EQ(input[3]["output"], R"(Error: {"error":"unavailable"})");
}

TEST(OpenAIResponsesRequestTest, IgnoresOtherProvidersParts) {
  Messages conversation = {
      Message::user("Hi"),
      Message(kMessageRoleAssistant,
              {ProviderContentPart("anthropic", {{"type", "thinking"}}),
               TextContentPart("Hello")}),
      Message::user("Bye")};

  openai::OpenAIResponsesRequestBuilder builder;
  GenerateOptions options(openai::models::kGpt6Luna, std::move(conversation));
  const auto input = builder.build_request_json(options)["input"];

  ASSERT_EQ(input.size(), 3u);
  EXPECT_EQ(input[1]["content"], "Hello");
}

// Response parsing

TEST(OpenAIResponsesParserTest, ParsesTextResponse) {
  const nlohmann::json response = {
      {"id", "resp_2"},
      {"created_at", 1740855869},
      {"model", "gpt-6-luna"},
      {"status", "completed"},
      {"output",
       {{{"id", "msg_1"},
         {"type", "message"},
         {"role", "assistant"},
         {"status", "completed"},
         {"content",
          {{{"type", "output_text"},
            {"text", "Hello"},
            {"annotations", nlohmann::json::array()}},
           {{"type", "output_text"}, {"text", " there"}}}}}}},
      {"usage",
       {{"input_tokens", 5}, {"output_tokens", 2}, {"total_tokens", 7}}}};

  openai::OpenAIResponsesResponseParser parser;
  const auto result = parser.parse_success_completion_response(response);

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.text, "Hello there");
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_EQ(result.id, "resp_2");
  EXPECT_EQ(result.model, "gpt-6-luna");
  EXPECT_EQ(result.created, 1740855869);
  EXPECT_EQ(result.usage.prompt_tokens, 5);
  EXPECT_EQ(result.usage.completion_tokens, 2);
  EXPECT_EQ(result.usage.total_tokens, 7);
  ASSERT_EQ(result.response_messages.size(), 1u);
  EXPECT_EQ(result.response_messages[0].get_text(), "Hello there");
}

TEST(OpenAIResponsesParserTest, ParsesFunctionCallsAndKeepsReasoning) {
  openai::OpenAIResponsesResponseParser parser;
  const auto result =
      parser.parse_success_completion_response(make_tool_call_response());

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.finish_reason, kFinishReasonToolCalls);
  ASSERT_EQ(result.tool_calls.size(), 1u);
  EXPECT_EQ(result.tool_calls[0].id, "call_1");
  EXPECT_EQ(result.tool_calls[0].tool_name, "weather");
  EXPECT_EQ(result.tool_calls[0].arguments["location"], "Paris");
  EXPECT_EQ(result.usage.total_tokens, 105);

  ASSERT_EQ(result.response_messages.size(), 1u);
  const auto& turn = result.response_messages[0];
  EXPECT_EQ(turn.role, kMessageRoleAssistant);
  ASSERT_EQ(turn.content.size(), 3u);
  const auto* reasoning = std::get_if<ProviderContentPart>(&turn.content[0]);
  ASSERT_NE(reasoning, nullptr);
  EXPECT_EQ(reasoning->provider, openai::kResponsesProviderName);
  EXPECT_EQ(reasoning->data, kReasoningItem);
  ASSERT_EQ(turn.get_tool_calls().size(), 1u);
  EXPECT_EQ(turn.get_tool_calls()[0].id, "call_1");
}

TEST(OpenAIResponsesParserTest, MapsIncompleteAndFailedStatuses) {
  openai::OpenAIResponsesResponseParser parser;

  nlohmann::json response = {
      {"status", "incomplete"},
      {"incomplete_details", {{"reason", "max_output_tokens"}}},
      {"output", {kReasoningItem}}};
  auto result = parser.parse_success_completion_response(response);
  EXPECT_TRUE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonLength);
  // Reasoning alone cannot be replayed without the item that follows it.
  EXPECT_TRUE(result.response_messages.empty());

  response["incomplete_details"]["reason"] = "content_filter";
  result = parser.parse_success_completion_response(response);
  EXPECT_EQ(result.finish_reason, kFinishReasonContentFilter);

  response["incomplete_details"]["reason"] = "max_messages";
  result = parser.parse_success_completion_response(response);
  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("max_messages"));

  result = parser.parse_success_completion_response(
      {{"status", "failed"},
       {"error",
        {{"code", "server_error"},
         {"message", "The model failed to generate a response."}}}});
  EXPECT_FALSE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonError);
  EXPECT_THAT(result.error_message(), testing::HasSubstr("server_error"));
  EXPECT_THAT(result.error_message(),
              testing::HasSubstr("The model failed to generate a response."));
}

TEST(OpenAIResponsesParserTest, RejectsMalformedFunctionCallArguments) {
  auto response = make_tool_call_response();
  response["output"][1]["arguments"] = "{not json";

  openai::OpenAIResponsesResponseParser parser;
  const auto result = parser.parse_success_completion_response(response);
  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("weather"));
}

TEST(OpenAIResponsesParserTest, ParsesStandardErrors) {
  openai::OpenAIResponsesResponseParser parser;
  const auto result = parser.parse_error_completion_response(
      400,
      R"({"error":{"message":"Unsupported value: 'reasoning.effort' does not support 'none' with this model.","type":"invalid_request_error","param":"reasoning.effort","code":"unsupported_value"}})");
  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("reasoning.effort"));
}

// Multi-step tool loop: the reasoning items reach the follow-up request.

TEST(OpenAIResponsesMultiStepTest, FollowUpRequestReplaysReasoning) {
  GenerateOptions options(openai::models::kGpt62Sol, "Weather in Paris?");
  options.tools["weather"] = make_weather_tool();
  options.max_steps = 3;

  openai::OpenAIResponsesResponseParser parser;
  openai::OpenAIResponsesRequestBuilder builder;
  std::vector<nlohmann::json> requests;

  auto result = MultiStepCoordinator::execute_multi_step(
      options, [&](const GenerateOptions& step_options) {
        requests.push_back(builder.build_request_json(step_options));
        if (requests.size() == 1) {
          auto step =
              parser.parse_success_completion_response(make_tool_call_response());
          step.tool_results = ToolExecutor::execute_tools_with_options(
              step.tool_calls, step_options, false);
          return step;
        }
        return parser.parse_success_completion_response(
            {{"status", "completed"},
             {"output",
              {{{"type", "message"},
                {"role", "assistant"},
                {"content",
                 {{{"type", "output_text"}, {"text", "It is 72F."}}}}}}}});
      });

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_EQ(result.text, "It is 72F.");
  ASSERT_EQ(requests.size(), 2u);
  const auto& input = requests[1]["input"];
  ASSERT_EQ(input.size(), 4u);
  EXPECT_EQ(input[0]["role"], "user");
  EXPECT_EQ(input[1], kReasoningItem);
  EXPECT_EQ(input[2]["type"], "function_call");
  EXPECT_EQ(input[2]["id"], "fc_1");
  EXPECT_EQ(input[3]["type"], "function_call_output");
  EXPECT_EQ(input[3]["call_id"], "call_1");
  EXPECT_THAT(input[3]["output"].get<std::string>(),
              testing::HasSubstr("72"));

  // The continuation holds the replayable turn, the results, and the answer.
  ASSERT_EQ(result.response_messages.size(), 3u);
  EXPECT_EQ(result.response_messages.back().get_text(), "It is 72F.");
}

// Streaming

TEST(OpenAIResponsesStreamTest, StreamsTextAndUsage) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_line_for_testing("event: response.created");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.created","response":{"id":"resp_1","status":"in_progress","output":[]},"sequence_number":0})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_item.added","output_index":0,"item":{"id":"msg_1","type":"message","status":"in_progress","role":"assistant","content":[]},"sequence_number":1})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_text.delta","item_id":"msg_1","output_index":0,"content_index":0,"delta":"Hel","sequence_number":2})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_text.delta","item_id":"msg_1","output_index":0,"content_index":0,"delta":"lo","sequence_number":3})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.completed","response":{"id":"resp_1","status":"completed","output":[],"usage":{"input_tokens":4,"output_tokens":3,"total_tokens":7}},"sequence_number":4})");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 3u);
  EXPECT_EQ(stream.get_next_event().text_delta, "Hel");
  EXPECT_EQ(stream.get_next_event().text_delta, "lo");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonStop);
  ASSERT_TRUE(finish.usage.has_value());
  EXPECT_EQ(finish.usage->prompt_tokens, 4);
  EXPECT_EQ(finish.usage->total_tokens, 7);
  EXPECT_FALSE(stream.has_more_events());
}

TEST(OpenAIResponsesStreamTest, StreamsFunctionCall) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_item.added","output_index":1,"item":{"type":"function_call","id":"fc_1","call_id":"call_1","name":"weather","arguments":""}})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.function_call_arguments.delta","item_id":"fc_1","output_index":1,"delta":"{\"location\":"})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.function_call_arguments.delta","item_id":"fc_1","output_index":1,"delta":"\"Paris\"}"})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.function_call_arguments.done","item_id":"fc_1","output_index":1,"arguments":"{\"location\":\"Paris\"}"})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_item.done","output_index":1,"item":{"type":"function_call","id":"fc_1","call_id":"call_1","name":"weather","arguments":"{\"location\":\"Paris\"}","status":"completed"}})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.completed","response":{"status":"completed","usage":{"input_tokens":10,"output_tokens":5,"total_tokens":15}}})");

  const auto tool_event = stream.get_next_event();
  ASSERT_TRUE(tool_event.is_tool_call());
  EXPECT_EQ(tool_event.tool_call->id, "call_1");
  EXPECT_EQ(tool_event.tool_call->tool_name, "weather");
  EXPECT_EQ(tool_event.tool_call->arguments["location"], "Paris");

  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonToolCalls);
  EXPECT_EQ(finish.usage->total_tokens, 15);
}

TEST(OpenAIResponsesStreamTest, IncompleteResponseFinishesWithLength) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_text.delta","output_index":0,"delta":"Once"})");
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.incomplete","response":{"status":"incomplete","incomplete_details":{"reason":"max_output_tokens"},"usage":{"input_tokens":3,"output_tokens":16,"total_tokens":19}}})");

  EXPECT_EQ(stream.get_next_event().text_delta, "Once");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonLength);
  EXPECT_EQ(finish.usage->completion_tokens, 16);
}

TEST(OpenAIResponsesStreamTest, FailedResponseEmitsError) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.failed","response":{"status":"failed","error":{"code":"server_error","message":"The model failed to generate a response."}}})");

  const auto error = stream.get_next_event();
  ASSERT_TRUE(error.is_error());
  EXPECT_THAT(*error.error, testing::HasSubstr("server_error"));
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonError);
  EXPECT_FALSE(finish.usage.has_value());
}

TEST(OpenAIResponsesStreamTest, ErrorEventEmitsError) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_line_for_testing(
      R"(data: {"type":"error","code":"rate_limit_exceeded","message":"Slow down","param":null,"sequence_number":1})");
  // Nothing after the terminal event is emitted.
  stream.process_sse_line_for_testing(
      R"(data: {"type":"response.output_text.delta","output_index":0,"delta":"late"})");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 2u);
  const auto error = stream.get_next_event();
  ASSERT_TRUE(error.is_error());
  EXPECT_THAT(*error.error, testing::HasSubstr("rate_limit_exceeded"));
  EXPECT_THAT(*error.error, testing::HasSubstr("Slow down"));
  EXPECT_EQ(stream.get_next_event().finish_reason, kFinishReasonError);
}

TEST(OpenAIResponsesStreamTest, ChunkedEventsAreReassembled) {
  openai::OpenAIResponsesStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      "event: response.output_text.delta\ndata: {\"type\":\"response.output_"
      "text.delta\",\"output_index\":0,\"del");
  stream.process_sse_chunk_for_testing(
      "ta\":\"Hi\"}\n\nevent: response.completed\ndata: {\"type\":\"response."
      "completed\",\"response\":{\"status\":\"completed\"}}\n\n");

  EXPECT_EQ(stream.get_next_event().text_delta, "Hi");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonStop);
}

// Client

TEST(OpenAIResponsesClientTest, UsesResponsesEndpointAndDefaults) {
  openai::OpenAIResponsesClient client("sk-test", "https://api.openai.com");
  EXPECT_EQ(client.provider_name(), "openai");
  EXPECT_EQ(client.default_model(), openai::models::kDefaultResponsesModel);
  EXPECT_THAT(client.config_info(), testing::HasSubstr("Responses API"));
  EXPECT_TRUE(client.supports_model(openai::models::kGpt62Sol));
  EXPECT_TRUE(client.is_valid());
}

}  // namespace test
}  // namespace ai
