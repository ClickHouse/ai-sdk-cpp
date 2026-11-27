#include "openai_responses_stream.h"

#include "ai/logger.h"
#include "openai_responses_response_parser.h"

#include <cstddef>
#include <string>

namespace ai {
namespace openai {

namespace {

std::string string_field(const nlohmann::json& object, const char* key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return "";
  }
  return it->get<std::string>();
}

}  // namespace

OpenAIResponsesStreamImpl::~OpenAIResponsesStreamImpl() {
  // Join before this class's state is destroyed; the stream thread calls
  // process_sse_line.
  stop_stream();
}

void OpenAIResponsesStreamImpl::reset_stream_state() {
  has_function_calls_ = false;
  finished_ = false;
}

void OpenAIResponsesStreamImpl::process_sse_line(std::string_view line) {
  // Every `data:` payload names its event in `type`, so the `event:` lines
  // that precede them carry nothing extra.
  if (!line.starts_with("data:")) {
    return;
  }
  auto data = line.substr(5);
  if (data.starts_with(' ')) {
    data.remove_prefix(1);
  }
  if (finished_ || data.empty()) {
    return;
  }

  nlohmann::json event;
  try {
    event = nlohmann::json::parse(data);
  } catch (const nlohmann::json::exception& e) {
    fail("Failed to parse Responses API stream event: " +
         std::string(e.what()));
    return;
  }
  handle_event(event);
}

void OpenAIResponsesStreamImpl::handle_event(const nlohmann::json& event) {
  const auto type = string_field(event, "type");

  if (type == "response.output_text.delta" ||
      type == "response.refusal.delta") {
    auto delta = string_field(event, "delta");
    if (!delta.empty()) {
      push_event(StreamEvent(std::move(delta)));
    }
    return;
  }

  if (type == "response.output_item.added" ||
      type == "response.output_item.done") {
    const auto item_it = event.find("item");
    if (item_it == event.end() || !item_it->is_object() ||
        string_field(*item_it, "type") != "function_call") {
      return;
    }
    const auto index = event.value("output_index", std::size_t{0});
    auto& pending = pending_tool_calls_[index];
    pending.id = string_field(*item_it, "call_id");
    pending.name = string_field(*item_it, "name");
    if (const auto arguments = string_field(*item_it, "arguments");
        !arguments.empty() || type == "response.output_item.done") {
      pending.arguments = arguments;
    }
    if (type == "response.output_item.done") {
      if (string_field(*item_it, "status") == "incomplete") {
        // Cut off by `max_output_tokens`: the arguments are truncated.
        ai::logger::log_warn("Dropping incomplete function call: {}",
                             pending.name);
        pending_tool_calls_.erase(index);
      } else {
        has_function_calls_ = true;
        flush_pending_tool_call(index);
      }
    }
    return;
  }

  if (type == "response.function_call_arguments.delta") {
    const auto index = event.value("output_index", std::size_t{0});
    pending_tool_calls_[index].arguments += string_field(event, "delta");
    return;
  }

  if (type == "response.function_call_arguments.done") {
    const auto index = event.value("output_index", std::size_t{0});
    pending_tool_calls_[index].arguments = string_field(event, "arguments");
    return;
  }

  if (type == "response.completed" || type == "response.incomplete" ||
      type == "response.failed") {
    const auto response_it = event.find("response");
    if (response_it == event.end() || !response_it->is_object()) {
      fail("Responses API stream event " + type + " has no response");
      return;
    }
    finish_response(*response_it);
    return;
  }

  if (type == "error") {
    std::string message = "OpenAI stream error";
    if (const auto code = string_field(event, "code"); !code.empty()) {
      message += " (" + code + ")";
    }
    if (const auto text = string_field(event, "message"); !text.empty()) {
      message += ": " + text;
    }
    fail(message);
    return;
  }

  // Lifecycle (`response.created`, `response.in_progress`, ...), content part,
  // and reasoning summary events carry nothing that is streamed.
}

void OpenAIResponsesStreamImpl::finish_response(
    const nlohmann::json& response) {
  if (const auto usage_it = response.find("usage");
      usage_it != response.end() && usage_it->is_object()) {
    usage_ = OpenAIResponsesResponseParser::parse_usage(*usage_it);
  }

  std::string error;
  const auto finish_reason = OpenAIResponsesResponseParser::parse_finish_reason(
      response, has_function_calls_, error);
  if (!finish_reason) {
    fail(error);
    return;
  }

  finish_reason_ = *finish_reason;
  // Function calls are emitted when their output item is done; one that never
  // got there was cut off.
  pending_tool_calls_.clear();
  finished_ = true;
  if (usage_) {
    ai::logger::log_info(
        "Stream completed - tokens used: {} prompt, {} completion, {} total",
        usage_->prompt_tokens, usage_->completion_tokens, usage_->total_tokens);
  }
  mark_complete();
}

void OpenAIResponsesStreamImpl::fail(const std::string& message) {
  ai::logger::log_error("{}", message);
  pending_tool_calls_.clear();
  finished_ = true;
  note_stream_error();
  push_event(create_error_event(message));
  mark_complete();
}

void OpenAIResponsesStreamImpl::finalize_stream() {
  if (!finished_) {
    fail("Responses API stream ended without a terminal response event");
  }
}

}  // namespace openai
}  // namespace ai
