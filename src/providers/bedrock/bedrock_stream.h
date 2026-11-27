#pragma once

#include "ai/types/stream_result.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>
#include <thread>

#include <aws/bedrock-runtime/BedrockRuntimeClient.h>
#include <aws/bedrock-runtime/BedrockRuntimeErrors.h>
#include <aws/bedrock-runtime/model/ContentBlockDeltaEvent.h>
#include <aws/bedrock-runtime/model/ContentBlockStartEvent.h>
#include <aws/bedrock-runtime/model/ContentBlockStopEvent.h>
#include <aws/bedrock-runtime/model/ConverseStreamMetadataEvent.h>
#include <aws/bedrock-runtime/model/ConverseStreamRequest.h>
#include <aws/bedrock-runtime/model/MessageStopEvent.h>

namespace ai {
namespace bedrock {

/// Implements streaming using the AWS Bedrock ConverseStream API.
///
/// The stream owns (shares) everything its background thread uses: the AWS
/// runtime client and the concurrency semaphore, so it may outlive the
/// `Client` that created it.
class BedrockStreamImpl : public internal::StreamResultImpl {
 public:
  using Semaphore = std::counting_semaphore<>;

  BedrockStreamImpl() = default;
  ~BedrockStreamImpl() override;

  BedrockStreamImpl(const BedrockStreamImpl&) = delete;
  BedrockStreamImpl& operator=(const BedrockStreamImpl&) = delete;
  BedrockStreamImpl(BedrockStreamImpl&&) = delete;
  BedrockStreamImpl& operator=(BedrockStreamImpl&&) = delete;

  /// Start streaming on a background thread.
  /// @param semaphore Optional semaphore, already acquired by the caller,
  ///                  released when the stream completes
  void start_stream(
      std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
      Aws::BedrockRuntime::Model::ConverseStreamRequest request,
      std::shared_ptr<Semaphore> semaphore = nullptr);

  /// Finish the stream immediately with an error event, without calling the
  /// API (used when the request is rejected before it is sent)
  void fail(const std::string& error_message);

  // StreamResultImpl interface
  StreamEvent get_next_event() override;
  bool has_more_events() const override;
  void stop_stream() override;

 private:
  struct PendingToolCall {
    std::string id;
    std::string name;
    std::string input_json;
  };

  void run_stream(
      std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
      Aws::BedrockRuntime::Model::ConverseStreamRequest request);

  void on_content_block_start(
      const Aws::BedrockRuntime::Model::ContentBlockStartEvent& event);
  void on_content_block_delta(
      const Aws::BedrockRuntime::Model::ContentBlockDeltaEvent& event);
  void on_content_block_stop(
      const Aws::BedrockRuntime::Model::ContentBlockStopEvent& event);
  void on_message_stop(
      const Aws::BedrockRuntime::Model::MessageStopEvent& event);
  void on_metadata(
      const Aws::BedrockRuntime::Model::ConverseStreamMetadataEvent& event);
  void on_error(const Aws::BedrockRuntime::BedrockRuntimeError& error);

  void push_event(StreamEvent event);
  void push_error(const std::string& message);
  void flush_tool_call(int content_block_index);

  /// Push the finish event, release the semaphore, and wake up readers
  void mark_complete();

  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<StreamEvent> event_queue_;
  bool complete_ = false;

  std::atomic<bool> stop_requested_{false};
  std::thread stream_thread_;
  std::shared_ptr<Semaphore> semaphore_;

  // Accessed only by the stream thread (or before it starts)
  std::map<int, PendingToolCall> pending_tool_calls_;
  FinishReason finish_reason_ = kFinishReasonStop;
  std::optional<Usage> usage_;
  bool errored_ = false;
};

}  // namespace bedrock
}  // namespace ai
