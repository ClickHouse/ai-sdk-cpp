#include "google_stream.h"

#include "ai/logger.h"
#include "google_response_parser.h"

#include <utility>

namespace ai {
namespace google {

namespace {

std::string string_field(const nlohmann::json& object, const char* key) {
  const auto it = object.find(key);
  return it != object.end() && it->is_string() ? it->get<std::string>()
                                               : std::string{};
}

}  // namespace

GoogleStreamImpl::~GoogleStreamImpl() {
  // Join before this class's state is destroyed; the stream thread calls
  // process_sse_line.
  stop_stream();
}

void GoogleStreamImpl::reset_stream_state() {
  event_data_.clear();
  call_index_ = 0;
  saw_function_call_ = false;
}

void GoogleStreamImpl::process_sse_line(std::string_view line) {
  if (line.empty()) {
    // Blank line terminates the SSE event.
    if (!event_data_.empty()) {
      process_sse_event(event_data_);
      event_data_.clear();
    }
  } else if (line.starts_with("data:")) {
    auto data = line.substr(5);
    if (!data.empty() && data.front() == ' ') {
      data.remove_prefix(1);
    }
    if (!event_data_.empty()) {
      event_data_ += '\n';
    }
    event_data_.append(data);
  }
}

void GoogleStreamImpl::finalize_stream() {
  // Dispatch an event whose terminating blank line never arrived.
  if (!event_data_.empty()) {
    process_sse_event(event_data_);
    event_data_.clear();
  }
}

void GoogleStreamImpl::process_sse_event(const std::string& data) {
  nlohmann::json chunk;
  try {
    chunk = nlohmann::json::parse(data);
  } catch (const nlohmann::json::exception& e) {
    ai::logger::log_error("Failed to parse Gemini SSE event: {}", e.what());
    note_stream_error();
    push_event(create_error_event("Failed to parse Gemini stream chunk: " +
                                  std::string(e.what())));
    return;
  }
  if (!chunk.is_object()) {
    return;
  }

  if (const auto error = chunk.find("error");
      error != chunk.end() && error->is_object()) {
    std::string message = "Google API error: " +
                          error->value("message", std::string("unknown error"));
    if (const auto status = string_field(*error, "status"); !status.empty()) {
      message += " [" + status + "]";
    }
    note_stream_error();
    push_event(create_error_event(message));
    mark_complete();
    return;
  }

  if (const auto usage = chunk.find("usageMetadata"); usage != chunk.end()) {
    // Every chunk carries the running totals; the last one wins.
    usage_ = parse_usage(*usage);
  }

  if (auto blocked = prompt_block_error(chunk)) {
    note_stream_error();
    push_event(create_error_event(*blocked));
    mark_complete();
    return;
  }

  const auto candidates = chunk.find("candidates");
  if (candidates == chunk.end() || !candidates->is_array() ||
      candidates->empty() || !(*candidates)[0].is_object()) {
    return;
  }
  const auto& candidate = (*candidates)[0];

  if (const auto content = candidate.find("content");
      content != candidate.end() && content->is_object()) {
    if (const auto parts = content->find("parts");
        parts != content->end() && parts->is_array()) {
      for (const auto& part : *parts) {
        if (!part.is_object()) {
          continue;
        }
        if (const auto thought = part.find("thought"); thought != part.end() &&
                                                       thought->is_boolean() &&
                                                       thought->get<bool>()) {
          // Thought summaries are not answer text, and `StreamEvent` has no
          // payload for reasoning deltas yet.
          continue;
        }
        if (const auto call = part.find("functionCall");
            call != part.end() && call->is_object()) {
          // Gemini streams each function call whole, never in fragments.
          auto id = string_field(*call, "id");
          if (id.empty()) {
            id = synthetic_call_id(call_index_);
          }
          ++call_index_;
          JsonValue arguments = JsonValue::object();
          if (const auto args = call->find("args");
              args != call->end() && args->is_object()) {
            arguments = *args;
          }
          saw_function_call_ = true;
          push_event(
              StreamEvent(ToolCall(std::move(id), string_field(*call, "name"),
                                   std::move(arguments))));
        } else if (auto text = string_field(part, "text"); !text.empty()) {
          push_event(StreamEvent(std::move(text)));
        }
      }
    }
  }

  const auto reason = string_field(candidate, "finishReason");
  if (reason.empty()) {
    return;
  }
  if (const auto finish_reason =
          parse_finish_reason(reason, saw_function_call_)) {
    finish_reason_ = *finish_reason;
  } else {
    std::string message = "Gemini response finished with " + reason;
    if (const auto finish_message = string_field(candidate, "finishMessage");
        !finish_message.empty()) {
      message += ": " + finish_message;
    }
    note_stream_error();
    push_event(create_error_event(message));
  }
  // The chunk with the finish reason is the last one; complete right away so
  // consumers are not left waiting for the connection to close.
  mark_complete();
}

}  // namespace google
}  // namespace ai
