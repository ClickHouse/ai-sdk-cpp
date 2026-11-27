#include "providers/google/google_stream.h"

#include "ai/types/stream_event.h"
#include "providers/google/google_request_builder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

TEST(GoogleStreamTest, StreamsTextAndUsage) {
  google::GoogleStreamImpl stream;
  stream.process_sse_line_for_testing(
      R"(data: {"candidates":[{"content":{"role":"model","parts":[{"text":"Hel"}]},"index":0}],"usageMetadata":{"promptTokenCount":4,"totalTokenCount":4},"modelVersion":"gemini-4-hydrogen","responseId":"r1"})");
  stream.process_sse_line_for_testing("");
  stream.process_sse_line_for_testing(
      R"(data: {"candidates":[{"content":{"role":"model","parts":[{"text":"lo"}]},"finishReason":"STOP","index":0}],"usageMetadata":{"promptTokenCount":4,"candidatesTokenCount":2,"thoughtsTokenCount":3,"totalTokenCount":9},"modelVersion":"gemini-4-hydrogen","responseId":"r1"})");
  stream.process_sse_line_for_testing("");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 3u);
  EXPECT_EQ(stream.get_next_event().text_delta, "Hel");
  EXPECT_EQ(stream.get_next_event().text_delta, "lo");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonStop);
  ASSERT_TRUE(finish.usage.has_value());
  EXPECT_EQ(finish.usage->prompt_tokens, 4);
  EXPECT_EQ(finish.usage->completion_tokens, 5);
  EXPECT_EQ(finish.usage->total_tokens, 9);
  EXPECT_FALSE(stream.has_more_events());
}

TEST(GoogleStreamTest, ChunkedEventsAreReassembled) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      "data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi");
  stream.process_sse_chunk_for_testing(
      " there\"}]},\"finishReason\":\"STOP\"}]}\r\n\r\n");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 2u);
  EXPECT_EQ(stream.get_next_event().text_delta, "Hi there");
  EXPECT_TRUE(stream.get_next_event().is_finish());
}

TEST(GoogleStreamTest, SkipsThoughtsAndSignatureOnlyParts) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"candidates":[{"content":{"parts":[{"text":"Thinking...","thought":true}]}}]}

data: {"candidates":[{"content":{"parts":[{"text":"Answer"}]}}]}

data: {"candidates":[{"content":{"parts":[{"text":"","thoughtSignature":"sig"}]},"finishReason":"STOP"}]}

)");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 2u);
  EXPECT_EQ(stream.get_next_event().text_delta, "Answer");
  EXPECT_TRUE(stream.get_next_event().is_finish());
}

TEST(GoogleStreamTest, StreamsFunctionCalls) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"candidates":[{"content":{"role":"model","parts":[{"functionCall":{"id":"fc-1","name":"weather","args":{"location":"Paris"}},"thoughtSignature":"sig"},{"functionCall":{"name":"weather","args":{"location":"Rome"}}}]},"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":10,"candidatesTokenCount":6,"totalTokenCount":16}}

)");

  ASSERT_EQ(stream.queued_event_count_for_testing(), 3u);
  auto first = stream.get_next_event();
  ASSERT_TRUE(first.is_tool_call());
  EXPECT_EQ(first.tool_call->id, "fc-1");
  EXPECT_EQ(first.tool_call->tool_name, "weather");
  EXPECT_EQ(first.tool_call->arguments["location"], "Paris");
  auto second = stream.get_next_event();
  ASSERT_TRUE(second.is_tool_call());
  EXPECT_THAT(second.tool_call->id,
              testing::StartsWith(google::kSyntheticCallIdPrefix));
  EXPECT_EQ(second.tool_call->arguments["location"], "Rome");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonToolCalls);
}

TEST(GoogleStreamTest, MaxTokensFinishesWithLength) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"candidates":[{"content":{"parts":[{"text":"Once"}]},"finishReason":"MAX_TOKENS"}]}

)");
  EXPECT_EQ(stream.get_next_event().text_delta, "Once");
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonLength);
}

TEST(GoogleStreamTest, FailedFinishReasonEmitsError) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"candidates":[{"finishReason":"MALFORMED_FUNCTION_CALL","finishMessage":"bad call"}]}

)");
  const auto error = stream.get_next_event();
  ASSERT_TRUE(error.is_error());
  EXPECT_THAT(error.error.value_or(""),
              testing::HasSubstr("MALFORMED_FUNCTION_CALL"));
  const auto finish = stream.get_next_event();
  ASSERT_TRUE(finish.is_finish());
  EXPECT_EQ(finish.finish_reason, kFinishReasonError);
}

TEST(GoogleStreamTest, BlockedPromptEmitsError) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"promptFeedback":{"blockReason":"PROHIBITED_CONTENT"},"usageMetadata":{"promptTokenCount":3,"totalTokenCount":3}}

)");
  const auto error = stream.get_next_event();
  ASSERT_TRUE(error.is_error());
  EXPECT_THAT(error.error.value_or(""),
              testing::HasSubstr("PROHIBITED_CONTENT"));
  EXPECT_EQ(stream.get_next_event().finish_reason, kFinishReasonError);
}

TEST(GoogleStreamTest, ErrorChunkEmitsError) {
  google::GoogleStreamImpl stream;
  stream.process_sse_chunk_for_testing(
      R"(data: {"error":{"code":500,"message":"Internal error encountered.","status":"INTERNAL"}}

)");
  const auto error = stream.get_next_event();
  ASSERT_TRUE(error.is_error());
  EXPECT_THAT(error.error.value_or(""),
              testing::HasSubstr("Internal error encountered."));
  EXPECT_THAT(error.error.value_or(""), testing::HasSubstr("INTERNAL"));
  EXPECT_EQ(stream.get_next_event().finish_reason, kFinishReasonError);
}

}  // namespace test
}  // namespace ai
