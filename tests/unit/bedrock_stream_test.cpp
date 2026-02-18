// Bedrock Stream Unit Tests
// Tests streaming event handling and ordering

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/bedrock_stream.h"

#include "ai/bedrock.h"
#include "ai/types/stream_event.h"

namespace ai {
namespace bedrock {
namespace {

class BedrockStreamTest : public ::testing::Test {
 protected:
  // Note: Full streaming tests require mocking the AWS SDK client
  // These tests verify the basic structure and interface
};

// Test stream implementation can be created
TEST_F(BedrockStreamTest, CanCreateStreamImpl) {
  BedrockStreamImpl impl;
  // A newly created stream that hasn't been started yet
  // returns true for has_more_events because it's waiting for events
  // The stream is not complete until start_stream is called and finishes
  EXPECT_TRUE(impl.has_more_events());
}

// Test stream can be stopped
TEST_F(BedrockStreamTest, CanStopStream) {
  BedrockStreamImpl impl;
  impl.stop_stream();
  // After stopping, the stream should still report has_more_events as true
  // because stream_complete_ is only set when the stream thread finishes
  // For a stream that was never started, stop_stream is a no-op
  EXPECT_TRUE(impl.has_more_events());
}

// Note: More comprehensive streaming tests would require mocking
// the AWS BedrockRuntimeClient, which is complex due to the
// event stream handler pattern. Integration tests provide
// better coverage for streaming functionality.

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

// Placeholder test when Bedrock is not enabled
TEST(BedrockStreamTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
