#pragma once

#include "ai/types/stream_event.h"

#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace ai {
namespace providers {
namespace streaming {

/// One partially received tool call, accumulated across SSE fragments.
struct PendingToolCall {
  std::string id;
  std::string name;
  std::string arguments;      // Accumulated argument fragments (JSON text).
  std::string initial_input;  // Complete input object sent up front, if any.
};

/// Convert an accumulated tool call into a StreamEvent. Returns a tool-call
/// event on success or an error event when the accumulated arguments are not
/// valid JSON.
inline StreamEvent make_tool_call_event(const PendingToolCall& pending) {
  const std::string& raw =
      pending.arguments.empty() ? pending.initial_input : pending.arguments;
  try {
    auto arguments =
        raw.empty() ? nlohmann::json::object() : nlohmann::json::parse(raw);
    return StreamEvent(
        ToolCall(pending.id, pending.name, std::move(arguments)));
  } catch (const nlohmann::json::exception& e) {
    return StreamEvent(kStreamEventTypeError,
                       "Failed to parse streamed tool-call arguments: " +
                           std::string(e.what()));
  }
}

/// Split a URL into origin ("scheme://host[:port]") and path, defaulting the
/// path when the URL has none.
inline std::pair<std::string, std::string> split_origin_and_path(
    std::string_view url,
    std::string_view default_path) {
  const auto scheme_pos = url.find("://");
  const auto authority_start =
      scheme_pos == std::string_view::npos ? 0 : scheme_pos + 3;
  const auto slash_pos = url.find('/', authority_start);
  std::string origin(url.substr(0, slash_pos));
  std::string path = slash_pos == std::string_view::npos
                         ? std::string(default_path)
                         : std::string(url.substr(slash_pos));
  return {std::move(origin), std::move(path)};
}

}  // namespace streaming
}  // namespace providers
}  // namespace ai
