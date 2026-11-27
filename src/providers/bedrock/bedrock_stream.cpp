#include "bedrock_stream.h"

#include "ai/logger.h"

#include <aws/bedrock-runtime/model/ContentBlockDelta.h>
#include <aws/bedrock-runtime/model/ContentBlockStart.h>
#include <aws/bedrock-runtime/model/ConverseStreamHandler.h>
#include <aws/bedrock-runtime/model/StopReason.h>
#include <aws/bedrock-runtime/model/TokenUsage.h>
#include <aws/bedrock-runtime/model/ToolUseBlockDelta.h>
#include <aws/bedrock-runtime/model/ToolUseBlockStart.h>
#include <nlohmann/json.hpp>

namespace ai {
namespace bedrock {

namespace Model = Aws::BedrockRuntime::Model;

BedrockStreamImpl::~BedrockStreamImpl() {
  stop_stream();
  // Release the semaphore if the stream thread never ran to completion
  if (semaphore_) {
    semaphore_->release();
  }
}

void BedrockStreamImpl::start_stream(
    std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
    Model::ConverseStreamRequest request,
    std::shared_ptr<Semaphore> semaphore) {
  ai::logger::log_debug("Starting Bedrock ConverseStream");
  semaphore_ = std::move(semaphore);

  stream_thread_ = std::thread([this, client = std::move(client),
                                request = std::move(request)]() mutable {
    run_stream(std::move(client), std::move(request));
  });
}

void BedrockStreamImpl::run_stream(
    std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client,
    Model::ConverseStreamRequest request) {
  try {
    Model::ConverseStreamHandler handler;
    handler.SetContentBlockStartEventCallback(
        [this](const Model::ContentBlockStartEvent& event) {
          on_content_block_start(event);
        });
    handler.SetContentBlockDeltaEventCallback(
        [this](const Model::ContentBlockDeltaEvent& event) {
          on_content_block_delta(event);
        });
    handler.SetContentBlockStopEventCallback(
        [this](const Model::ContentBlockStopEvent& event) {
          on_content_block_stop(event);
        });
    handler.SetMessageStopEventCallback(
        [this](const Model::MessageStopEvent& event) {
          on_message_stop(event);
        });
    handler.SetConverseStreamMetadataEventCallback(
        [this](const Model::ConverseStreamMetadataEvent& event) {
          on_metadata(event);
        });
    handler.SetOnErrorCallback(
        [this](const Aws::BedrockRuntime::BedrockRuntimeError& error) {
          on_error(error);
        });
    request.SetEventStreamHandler(handler);

    // Abort the HTTP request when the consumer stops the stream
    request.SetContinueRequestHandler(
        [this](const Aws::Http::HttpRequest*) { return !stop_requested_; });

    auto outcome = client->ConverseStream(request);
    if (!outcome.IsSuccess() && !stop_requested_) {
      on_error(outcome.GetError());
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Bedrock stream thread exception: {}", e.what());
    push_error(std::string("Stream error: ") + e.what());
  }

  mark_complete();
}

void BedrockStreamImpl::fail(const std::string& error_message) {
  push_error(error_message);
  mark_complete();
}

StreamEvent BedrockStreamImpl::get_next_event() {
  // No timeout of its own: gaps between events can be long while the model
  // thinks, and a stalled connection is ended by the AWS client's request
  // timeout, after which the stream thread pushes an error and completes.
  std::unique_lock<std::mutex> lock(queue_mutex_);
  queue_cv_.wait(lock, [this] { return !event_queue_.empty() || complete_; });

  if (event_queue_.empty()) {
    return StreamEvent("");
  }

  StreamEvent event = std::move(event_queue_.front());
  event_queue_.pop_front();
  return event;
}

bool BedrockStreamImpl::has_more_events() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return !event_queue_.empty() || !complete_;
}

void BedrockStreamImpl::stop_stream() {
  stop_requested_ = true;
  if (stream_thread_.joinable()) {
    stream_thread_.join();
  }
}

void BedrockStreamImpl::on_content_block_start(
    const Model::ContentBlockStartEvent& event) {
  if (stop_requested_ || !event.StartHasBeenSet() ||
      !event.GetStart().ToolUseHasBeenSet()) {
    return;
  }
  const auto& tool_use = event.GetStart().GetToolUse();
  pending_tool_calls_[event.GetContentBlockIndex()] =
      PendingToolCall{tool_use.GetToolUseId(), tool_use.GetName(), {}};
}

void BedrockStreamImpl::on_content_block_delta(
    const Model::ContentBlockDeltaEvent& event) {
  if (stop_requested_ || !event.DeltaHasBeenSet()) {
    return;
  }
  const auto& delta = event.GetDelta();

  if (delta.TextHasBeenSet()) {
    push_event(StreamEvent(std::string(delta.GetText())));
  }

  if (delta.ToolUseHasBeenSet()) {
    const auto it = pending_tool_calls_.find(event.GetContentBlockIndex());
    if (it != pending_tool_calls_.end()) {
      it->second.input_json += delta.GetToolUse().GetInput();
    }
  }
}

void BedrockStreamImpl::on_content_block_stop(
    const Model::ContentBlockStopEvent& event) {
  if (stop_requested_) {
    return;
  }
  flush_tool_call(event.GetContentBlockIndex());
}

void BedrockStreamImpl::flush_tool_call(int content_block_index) {
  const auto it = pending_tool_calls_.find(content_block_index);
  if (it == pending_tool_calls_.end()) {
    return;
  }
  PendingToolCall pending = std::move(it->second);
  pending_tool_calls_.erase(it);

  nlohmann::json arguments = nlohmann::json::object();
  if (!pending.input_json.empty()) {
    arguments = nlohmann::json::parse(pending.input_json, nullptr,
                                      /*allow_exceptions=*/false);
    if (arguments.is_discarded()) {
      push_error("Invalid JSON input for tool call '" + pending.name +
                 "': " + pending.input_json);
      return;
    }
  }
  push_event(
      StreamEvent(ToolCall(pending.id, pending.name, std::move(arguments))));
}

void BedrockStreamImpl::on_message_stop(const Model::MessageStopEvent& event) {
  if (!event.StopReasonHasBeenSet()) {
    return;
  }
  switch (event.GetStopReason()) {
    case Model::StopReason::max_tokens:
      finish_reason_ = kFinishReasonLength;
      break;
    case Model::StopReason::content_filtered:
    case Model::StopReason::guardrail_intervened:
      finish_reason_ = kFinishReasonContentFilter;
      break;
    case Model::StopReason::tool_use:
      finish_reason_ = kFinishReasonToolCalls;
      break;
    default:
      finish_reason_ = kFinishReasonStop;
      break;
  }
}

void BedrockStreamImpl::on_metadata(
    const Model::ConverseStreamMetadataEvent& event) {
  if (!event.UsageHasBeenSet()) {
    return;
  }
  const auto& usage = event.GetUsage();
  usage_ = Usage(usage.GetInputTokens(), usage.GetOutputTokens());
}

void BedrockStreamImpl::on_error(
    const Aws::BedrockRuntime::BedrockRuntimeError& error) {
  ai::logger::log_error("Bedrock stream error: {} - {}",
                        error.GetExceptionName(), error.GetMessage());
  if (!error.GetExceptionName().empty()) {
    push_error(error.GetExceptionName() + ": " + error.GetMessage());
  } else {
    push_error("Stream error: " + error.GetMessage());
  }
}

void BedrockStreamImpl::push_event(StreamEvent event) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    event_queue_.push_back(std::move(event));
  }
  queue_cv_.notify_one();
}

void BedrockStreamImpl::push_error(const std::string& message) {
  errored_ = true;
  push_event(StreamEvent(kStreamEventTypeError, message));
}

void BedrockStreamImpl::mark_complete() {
  if (errored_) {
    // Do not stamp a failed stream with a clean finish reason or usage
    push_event(StreamEvent(kStreamEventTypeFinish, kFinishReasonError));
  } else if (!stop_requested_) {
    if (usage_) {
      push_event(StreamEvent(kStreamEventTypeFinish, *usage_, finish_reason_));
    } else {
      push_event(StreamEvent(kStreamEventTypeFinish, finish_reason_));
    }
  }

  if (semaphore_) {
    semaphore_->release();
    semaphore_.reset();
  }

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    complete_ = true;
  }
  queue_cv_.notify_all();
}

}  // namespace bedrock
}  // namespace ai
