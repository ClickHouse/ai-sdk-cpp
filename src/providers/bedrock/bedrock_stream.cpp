#include "bedrock_stream.h"

#include "ai/logger.h"

#include <aws/bedrock-runtime/BedrockRuntimeClient.h>
#include <aws/bedrock-runtime/model/ContentBlockDelta.h>
#include <aws/bedrock-runtime/model/ContentBlockDeltaEvent.h>
#include <aws/bedrock-runtime/model/ConverseStreamHandler.h>
#include <aws/bedrock-runtime/model/ConverseStreamRequest.h>
#include <aws/bedrock-runtime/model/MessageStartEvent.h>
#include <aws/bedrock-runtime/model/MessageStopEvent.h>
#include <aws/bedrock-runtime/model/StopReason.h>

#include <chrono>
#include <semaphore>
#include <thread>

namespace {
constexpr auto kEventTimeout = std::chrono::seconds(60);
constexpr auto kSleepInterval = std::chrono::milliseconds(1);
}  // namespace

namespace ai {
namespace bedrock {

BedrockStreamImpl::BedrockStreamImpl() = default;

BedrockStreamImpl::~BedrockStreamImpl() {
  stop_stream();
  
  // Ensure semaphore is released if stream was destroyed before completion
  if (semaphore_ != nullptr) {
    semaphore_->release();
    semaphore_ = nullptr;
  }
}

void BedrockStreamImpl::start_stream(
    std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
    Aws::BedrockRuntime::Model::ConverseStreamRequest request,
    std::counting_semaphore<>* semaphore) {
  ai::logger::log_debug("Starting Bedrock ConverseStream");

  // Store semaphore for release on completion
  semaphore_ = semaphore;

  // Start streaming in a separate thread
  stream_thread_ = std::thread([this, client = std::move(client),
                                request = std::move(request)]() mutable {
    try {
      // Set up the stream handler with callbacks
      Aws::BedrockRuntime::Model::ConverseStreamHandler handler;

      // Handle messageStart event
      handler.SetMessageStartEventCallback(
          [this](const Aws::BedrockRuntime::Model::MessageStartEvent& event) {
            on_message_start(event);
          });

      // Handle contentBlockDelta event
      handler.SetContentBlockDeltaEventCallback(
          [this](
              const Aws::BedrockRuntime::Model::ContentBlockDeltaEvent& event) {
            on_content_block_delta(event);
          });

      // Handle messageStop event
      handler.SetMessageStopEventCallback(
          [this](const Aws::BedrockRuntime::Model::MessageStopEvent& event) {
            on_message_stop(event);
          });

      // Handle errors
      handler.SetOnErrorCallback(
          [this](const Aws::Client::AWSError<
                 Aws::BedrockRuntime::BedrockRuntimeErrors>& error) {
            on_error(error);
          });

      // Set the handler on the request
      request.SetEventStreamHandler(handler);

      // Make the streaming call
      ai::logger::log_debug("Calling ConverseStream API");
      auto outcome = client->ConverseStream(request);

      if (!outcome.IsSuccess()) {
        on_error(outcome.GetError());
      }

    } catch (const std::exception& e) {
      ai::logger::log_error("Stream thread exception: {}", e.what());
      StreamEvent error_event(kStreamEventTypeError,
                              std::string("Stream error: ") + e.what());
      push_event(error_event);
    }

    mark_complete();
  });
}

StreamEvent BedrockStreamImpl::get_next_event() {
  StreamEvent event("");
  auto start_time = std::chrono::steady_clock::now();

  while (!event_queue_.try_dequeue(event)) {
    if (stream_complete_ && event_queue_.size_approx() == 0) {
      // Stream is complete and queue is empty
      ai::logger::log_debug(
          "Stream complete and queue empty, returning empty event");
      return StreamEvent("");
    }

    // Check for timeout
    if (std::chrono::steady_clock::now() - start_time > kEventTimeout) {
      ai::logger::log_error(
          "Timeout waiting for next stream event after {} seconds",
          kEventTimeout.count());
      return StreamEvent(kStreamEventTypeError,
                         "Timeout waiting for next event");
    }

    std::this_thread::sleep_for(kSleepInterval);
  }

  ai::logger::log_debug("Dequeued event type: {}",
                        static_cast<int>(event.type));
  return event;
}

bool BedrockStreamImpl::has_more_events() const {
  return event_queue_.size_approx() > 0 || !stream_complete_;
}

void BedrockStreamImpl::stop_stream() {
  ai::logger::log_debug("Stopping Bedrock stream");
  stop_requested_ = true;
  if (stream_thread_.joinable()) {
    stream_thread_.join();
  }
}

void BedrockStreamImpl::on_message_start(
    const Aws::BedrockRuntime::Model::MessageStartEvent& event) {
  ai::logger::log_debug("Received messageStart event");
  // messageStart contains metadata about the message
  // We don't need to emit an event for this, just log it
}

void BedrockStreamImpl::on_content_block_delta(
    const Aws::BedrockRuntime::Model::ContentBlockDeltaEvent& event) {
  // Extract text from the delta
  if (event.DeltaHasBeenSet()) {
    const auto& delta = event.GetDelta();
    if (delta.TextHasBeenSet()) {
      std::string text = delta.GetText();
      ai::logger::log_debug("Received content delta: '{}'", text);

      // Create and push text event
      StreamEvent stream_event(text);
      push_event(stream_event);
    }
  }
}

void BedrockStreamImpl::on_message_stop(
    const Aws::BedrockRuntime::Model::MessageStopEvent& event) {
  ai::logger::log_debug("Received messageStop event");

  // Map stop reason to finish reason
  FinishReason finish_reason = kFinishReasonStop;
  if (event.StopReasonHasBeenSet()) {
    switch (event.GetStopReason()) {
      case Aws::BedrockRuntime::Model::StopReason::end_turn:
        finish_reason = kFinishReasonStop;
        break;
      case Aws::BedrockRuntime::Model::StopReason::max_tokens:
        finish_reason = kFinishReasonLength;
        break;
      case Aws::BedrockRuntime::Model::StopReason::content_filtered:
        finish_reason = kFinishReasonContentFilter;
        break;
      case Aws::BedrockRuntime::Model::StopReason::tool_use:
        finish_reason = kFinishReasonToolCalls;
        break;
      default:
        finish_reason = kFinishReasonStop;
        break;
    }
  }

  // Create finish event with usage info if available
  Usage usage;
  // Note: Usage info may be available in metadata events, but for now we use
  // defaults
  StreamEvent finish_event(kStreamEventTypeFinish, usage, finish_reason);
  push_event(finish_event);
}

void BedrockStreamImpl::on_error(
    const Aws::BedrockRuntime::BedrockRuntimeError& error) {
  ai::logger::log_error("Stream error: {} - {}", error.GetExceptionName(),
                        error.GetMessage());

  // Build error message
  std::string error_message;
  if (!error.GetExceptionName().empty()) {
    error_message = error.GetExceptionName() + ": " + error.GetMessage();
  } else {
    error_message = "Stream error: " + error.GetMessage();
  }

  StreamEvent error_event(kStreamEventTypeError, error_message);
  push_event(error_event);
}

void BedrockStreamImpl::push_event(const StreamEvent& event) {
  event_queue_.enqueue(event);
}

void BedrockStreamImpl::mark_complete() {
  stream_complete_ = true;
  
  // Release semaphore if we were given one (for concurrency control)
  if (semaphore_ != nullptr) {
    semaphore_->release();
    semaphore_ = nullptr;  // Prevent double-release
  }
  
  ai::logger::log_debug("Stream marked as complete");
}

}  // namespace bedrock
}  // namespace ai
