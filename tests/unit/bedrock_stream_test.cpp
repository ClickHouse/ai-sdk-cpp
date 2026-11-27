// Bedrock Stream Unit Tests
// Tests streaming event handling and ordering

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "ai/bedrock.h"
#include "ai/types/stream_event.h"
#include "providers/bedrock/bedrock_stream.h"

#include <cstdlib>
#include <optional>
#include <string>

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

// Test a rejected stream yields its error immediately and then ends
TEST_F(BedrockStreamTest, FailedStreamYieldsErrorAndCompletes) {
  BedrockStreamImpl impl;
  impl.fail("Invalid model ID format: bad model");

  auto event = impl.get_next_event();
  EXPECT_EQ(event.type, kStreamEventTypeError);
  ASSERT_TRUE(event.error.has_value());
  EXPECT_EQ(event.error.value(), "Invalid model ID format: bad model");

  auto finish = impl.get_next_event();
  EXPECT_EQ(finish.type, kStreamEventTypeFinish);
  EXPECT_EQ(finish.finish_reason, kFinishReasonError);
  EXPECT_FALSE(impl.has_more_events());
}

// Sets (or unsets) an environment variable and restores its previous value
class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::optional<std::string>& value)
      : name_(name) {
    if (const char* previous = std::getenv(name)) {
      previous_ = previous;
    }
    set(value);
  }
  ~ScopedEnvVar() { set(previous_); }

  ScopedEnvVar(const ScopedEnvVar&) = delete;
  ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

 private:
  void set(const std::optional<std::string>& value) {
    if (value) {
      ::setenv(name_.c_str(), value->c_str(), 1);
    } else {
      ::unsetenv(name_.c_str());
    }
  }

  std::string name_;
  std::optional<std::string> previous_;
};

// A stream may outlive the client that created it: it shares ownership of
// everything its background thread uses
TEST_F(BedrockStreamTest, StreamOutlivesClient) {
  // Static credentials and an endpoint nothing listens on, so that the request
  // fails fast without network access or credential discovery
  // (restored when the test ends)
  ScopedEnvVar access_key("AWS_ACCESS_KEY_ID", "AKIDEXAMPLE");
  ScopedEnvVar secret_key("AWS_SECRET_ACCESS_KEY", "secret");
  ScopedEnvVar session_token("AWS_SESSION_TOKEN", std::nullopt);

  BedrockConfig config;
  config.region = "us-east-1";
  config.endpoint_override = "http://127.0.0.1:1";

  std::optional<StreamResult> stream;
  {
    auto client = create_client(config);
    GenerateOptions options(models::kClaudeSonnet, "Hello");
    stream.emplace(client.stream_text(StreamOptions(options)));
  }  // The client is destroyed while the stream may still be running

  bool saw_error = false;
  bool saw_finish = false;
  int events_after_finish = 0;
  for (const auto& event : *stream) {
    if (saw_finish) {
      // The finish event is the last one, even though it is pushed by
      // another thread while this one iterates
      ++events_after_finish;
    } else if (event.is_error()) {
      saw_error = true;
    } else if (event.type == kStreamEventTypeFinish) {
      saw_finish = true;
      EXPECT_EQ(event.finish_reason, kFinishReasonError);
    }
  }
  EXPECT_TRUE(saw_error);
  EXPECT_TRUE(saw_finish);
  EXPECT_EQ(events_after_finish, 0);
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
