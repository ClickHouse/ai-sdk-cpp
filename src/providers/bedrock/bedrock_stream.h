#pragma once

#include "ai/types/stream_result.h"

#include <aws/bedrock-runtime/BedrockRuntimeClient.h>
#include <aws/bedrock-runtime/BedrockRuntimeErrors.h>
#include <aws/bedrock-runtime/model/ContentBlockDeltaEvent.h>
#include <aws/bedrock-runtime/model/ConverseStreamRequest.h>
#include <aws/bedrock-runtime/model/MessageStartEvent.h>
#include <aws/bedrock-runtime/model/MessageStopEvent.h>

#include <concurrentqueue.h>

#include <atomic>
#include <memory>
#include <semaphore>
#include <thread>

namespace ai {
namespace bedrock {

/// Implements streaming using AWS Bedrock ConverseStream API
/// Handles messageStart, contentBlockDelta, and messageStop events
/// and yields StreamEvents in order
class BedrockStreamImpl : public internal::StreamResultImpl {
 public:
  BedrockStreamImpl();
  ~BedrockStreamImpl();

  // Non-copyable, non-movable for thread safety
  BedrockStreamImpl(const BedrockStreamImpl&) = delete;
  BedrockStreamImpl& operator=(const BedrockStreamImpl&) = delete;
  BedrockStreamImpl(BedrockStreamImpl&&) = delete;
  BedrockStreamImpl& operator=(BedrockStreamImpl&&) = delete;

  /// Start streaming with the given client and request
  /// @param client AWS Bedrock runtime client
  /// @param request ConverseStream request
  /// @param semaphore Optional semaphore to release on completion (for concurrency control)
  void start_stream(
      std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
      Aws::BedrockRuntime::Model::ConverseStreamRequest request,
      std::counting_semaphore<>* semaphore = nullptr);

  // StreamResultImpl interface
  StreamEvent get_next_event() override;
  bool has_more_events() const override;
  void stop_stream() override;

 private:
  /// Handle messageStart event from stream
  void on_message_start(
      const Aws::BedrockRuntime::Model::MessageStartEvent& event);

  /// Handle contentBlockDelta event from stream
  void on_content_block_delta(
      const Aws::BedrockRuntime::Model::ContentBlockDeltaEvent& event);

  /// Handle messageStop event from stream
  void on_message_stop(
      const Aws::BedrockRuntime::Model::MessageStopEvent& event);

  /// Handle error from stream
  void on_error(const Aws::BedrockRuntime::BedrockRuntimeError& error);

  /// Push event to the queue (preserves ordering)
  void push_event(const StreamEvent& event);

  /// Mark stream as complete and release resources
  void mark_complete();

  moodycamel::ConcurrentQueue<StreamEvent> event_queue_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> stream_complete_{false};
  std::thread stream_thread_;
  std::counting_semaphore<>* semaphore_{nullptr};  // For concurrency control
};

}  // namespace bedrock
}  // namespace ai
