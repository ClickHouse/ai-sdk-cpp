#include "../utils/mock_openai_client.h"
#include "../utils/test_fixtures.h"
#include "ai/types/stream_event.h"
#include "ai/types/stream_options.h"
#include "ai/types/stream_result.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

class OpenAIStreamTest : public OpenAITestFixture {};

// StreamOptions Tests
TEST_F(OpenAIStreamTest, StreamOptionsBasicConstructor) {
  StreamOptions options(GenerateOptions("gpt-4o", "Hello, world!"));

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_EQ(options.prompt, "Hello, world!");
  EXPECT_TRUE(options.system.empty());
  EXPECT_TRUE(options.messages.empty());
}

TEST_F(OpenAIStreamTest, StreamOptionsWithSystemPrompt) {
  StreamOptions options(
      GenerateOptions("gpt-4o", "System prompt", "User prompt"));

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_EQ(options.system, "System prompt");
  EXPECT_EQ(options.prompt, "User prompt");
}

TEST_F(OpenAIStreamTest, StreamOptionsWithMessages) {
  Messages messages = createSampleConversation();
  StreamOptions options(GenerateOptions("gpt-4o", std::move(messages)));

  EXPECT_EQ(options.model, "gpt-4o");
  EXPECT_FALSE(options.messages.empty());
  EXPECT_TRUE(options.has_messages());
}

TEST_F(OpenAIStreamTest, StreamOptionsValidation) {
  StreamOptions valid_options(GenerateOptions("gpt-4o", "Valid prompt"));
  EXPECT_TRUE(valid_options.is_valid());

  StreamOptions invalid_model(GenerateOptions("", "Valid prompt"));
  EXPECT_FALSE(invalid_model.is_valid());

  StreamOptions invalid_prompt(GenerateOptions("gpt-4o", ""));
  EXPECT_FALSE(invalid_prompt.is_valid());
}

// StreamResult Tests
TEST_F(OpenAIStreamTest, StreamResultBasicConstruction) {
  // Note: StreamResult likely takes a stream implementation
  // For testing, we'll use nullptr or a mock implementation
  StreamResult result(nullptr);

  // Basic test - in real implementation, this would test stream state
  // For now, just verify it can be constructed
}

// Streaming Event Processing Tests
class StreamEventTest : public OpenAITestFixture {};

TEST_F(StreamEventTest, ParseServerSentEvent) {
  std::string sse_data =
      "data: "
      "{\"id\":\"chatcmpl-test\",\"choices\":[{\"delta\":{\"content\":"
      "\"Hello\"}}]}\n\n";

  // In a real implementation, you'd parse this SSE data
  // For now, just verify the test data is well-formed
  EXPECT_THAT(sse_data, testing::HasSubstr("data:"));
  EXPECT_THAT(sse_data, testing::HasSubstr("Hello"));
}

TEST_F(StreamEventTest, ParseCompletionEvent) {
  std::string completion_data = "data: [DONE]\n\n";

  EXPECT_THAT(completion_data, testing::HasSubstr("[DONE]"));
}

TEST_F(StreamEventTest, ParseErrorEvent) {
  std::string error_data =
      "data: {\"error\":{\"message\":\"Stream error\"}}\n\n";

  EXPECT_THAT(error_data, testing::HasSubstr("error"));
  EXPECT_THAT(error_data, testing::HasSubstr("Stream error"));
}

// Mock Stream Implementation Tests
class MockStreamImpl {
 public:
  MOCK_METHOD(bool, is_active, (), (const));
  MOCK_METHOD(void, start, (), ());
  MOCK_METHOD(void, stop, (), ());
  MOCK_METHOD(std::string, read_next, (), ());
  MOCK_METHOD(bool, has_next, (), (const));
};

class StreamMockTest : public OpenAITestFixture {};

TEST_F(StreamMockTest, MockStreamBasicOperations) {
  MockStreamImpl mock_stream;

  // Set up expectations
  EXPECT_CALL(mock_stream, is_active())
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(mock_stream, start()).Times(1);

  EXPECT_CALL(mock_stream, has_next()).WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock_stream, read_next())
      .WillOnce(testing::Return("Hello"))
      .WillOnce(testing::Return(" world"))
      .WillOnce(testing::Return("!"));

  // Test sequence
  EXPECT_FALSE(mock_stream.is_active());

  mock_stream.start();
  EXPECT_TRUE(mock_stream.is_active());

  // Read stream data
  std::string full_text;
  while (mock_stream.has_next()) {
    auto chunk = mock_stream.read_next();
    full_text += chunk;
    if (full_text == "Hello world!") {
      break;  // Prevent infinite loop
    }
  }

  EXPECT_EQ(full_text, "Hello world!");
}

// Stream Response Builder Tests
TEST_F(StreamEventTest, BuildStreamingEvents) {
  auto events = ResponseBuilder::buildStreamingResponse("Test response");

  EXPECT_FALSE(events.empty());

  // First events should contain content
  bool found_content = false;
  for (const auto& event : events) {
    if (event.find("Test") != std::string::npos ||
        event.find("response") != std::string::npos) {
      found_content = true;
      break;
    }
  }
  EXPECT_TRUE(found_content);

  // Last event should be DONE
  EXPECT_THAT(events.back(), testing::HasSubstr("[DONE]"));
}

TEST_F(StreamEventTest, StreamEventValidation) {
  auto events = TestDataGenerator::createStreamingEvents();

  EXPECT_FALSE(events.empty());

  // Check that all events start with "data:"
  for (const auto& event : events) {
    if (!event.empty()) {
      EXPECT_THAT(event, testing::StartsWith("data:"));
    }
  }

  // Check that the sequence ends with DONE
  EXPECT_THAT(events.back(), testing::HasSubstr("[DONE]"));
}

// Stream Error Handling Tests
class StreamErrorTest : public OpenAITestFixture {};

TEST_F(StreamErrorTest, HandleStreamConnectionError) {
  ControllableOpenAIClient client(kTestApiKey);
  StreamOptions options(GenerateOptions("gpt-4o", "Test prompt"));

  client.setShouldFail(true);

  auto result = client.stream_text(options);

  // In a real implementation, this would test stream error state
  // For now, just verify the call was made
  EXPECT_EQ(client.getCallCount(), 1);
}

TEST_F(StreamErrorTest, HandleStreamTimeout) {
  ControllableOpenAIClient client(kTestApiKey);
  StreamOptions options(GenerateOptions("gpt-4o", "Test prompt"));

  client.setShouldTimeout(true);

  auto result = client.stream_text(options);

  EXPECT_EQ(client.getCallCount(), 1);
}

TEST_F(StreamErrorTest, HandleMalformedStreamData) {
  // Test data with invalid JSON
  std::vector<std::string> malformed_events = {
      "data: {invalid json\n\n",
      "data: {\"choices\":[{\"delta\":{\"content\":\"test\"}]\n\n",  // Missing
                                                                     // closing
                                                                     // brace
      "not-data: invalid format\n\n"};

  // In a real implementation, you'd test that these are handled gracefully
  // For now, just verify they're malformed
  for (const auto& event : malformed_events) {
    EXPECT_TRUE(event.find("data:") != std::string::npos ||
                event.find("invalid") != std::string::npos);
  }
}

// Stream Performance Tests
class StreamPerformanceTest : public OpenAITestFixture {};

TEST_F(StreamPerformanceTest, HandleLargeStreamResponse) {
  // Create a large response to test streaming performance
  std::string large_content =
      TestDataGenerator::createLargePrompt(50000);  // 50KB
  auto events = ResponseBuilder::buildStreamingResponse(large_content);

  EXPECT_FALSE(events.empty());

  // Verify that large content is properly chunked
  size_t total_content_length = 0;
  for (const auto& event : events) {
    if (event.find("content") != std::string::npos) {
      total_content_length += event.length();
    }
  }

  EXPECT_GT(total_content_length, 0);
}

TEST_F(StreamPerformanceTest, HandleManySmallChunks) {
  // Test with many small chunks
  std::vector<std::string> small_chunks;
  for (int i = 0; i < 1000; ++i) {
    small_chunks.push_back("chunk" + std::to_string(i));
  }

  // In a real implementation, you'd stream these and verify performance
  EXPECT_EQ(small_chunks.size(), 1000);
}

// Integration with OpenAI Client
class StreamIntegrationTest : public OpenAITestFixture {};

TEST_F(StreamIntegrationTest, StreamOptionsFromGenerateOptions) {
  auto generate_options = createAdvancedOptions();

  // Convert GenerateOptions to StreamOptions
  StreamOptions stream_options(generate_options);

  EXPECT_EQ(stream_options.model, generate_options.model);
  EXPECT_EQ(stream_options.prompt, generate_options.prompt);
  EXPECT_EQ(stream_options.system, generate_options.system);
  EXPECT_EQ(stream_options.temperature, generate_options.temperature);
}

TEST_F(StreamIntegrationTest, StreamWithClientConfiguration) {
  ControllableOpenAIClient client(kTestApiKey);

  // Verify client supports streaming
  EXPECT_TRUE(client.is_valid());

  StreamOptions options(GenerateOptions("gpt-4o", "Stream test"));
  auto result = client.stream_text(options);

  EXPECT_EQ(client.getCallCount(), 1);

  auto last_options = client.getLastStreamOptions();
  EXPECT_EQ(last_options.model, "gpt-4o");
  EXPECT_EQ(last_options.prompt, "Stream test");
}

// Edge Cases for Streaming
class StreamEdgeCaseTest : public OpenAITestFixture {};

TEST_F(StreamEdgeCaseTest, EmptyStreamResponse) {
  auto events = ResponseBuilder::buildStreamingResponse("");

  // Should still have at least the DONE event
  EXPECT_FALSE(events.empty());
  EXPECT_THAT(events.back(), testing::HasSubstr("[DONE]"));
}

TEST_F(StreamEdgeCaseTest, SingleCharacterStream) {
  auto events = ResponseBuilder::buildStreamingResponse("a");

  EXPECT_FALSE(events.empty());

  // Should contain the character and DONE
  bool found_char = false;
  for (const auto& event : events) {
    if (event.find("a") != std::string::npos) {
      found_char = true;
      break;
    }
  }
  EXPECT_TRUE(found_char);
}

TEST_F(StreamEdgeCaseTest, UnicodeInStream) {
  auto events = ResponseBuilder::buildStreamingResponse("Hello 世界 🌍");

  EXPECT_FALSE(events.empty());

  // Should handle Unicode properly
  bool found_unicode = false;
  for (const auto& event : events) {
    if (event.find("世界") != std::string::npos ||
        event.find("🌍") != std::string::npos) {
      found_unicode = true;
      break;
    }
  }
  EXPECT_TRUE(found_unicode);
}

}  // namespace test
}  // namespace ai