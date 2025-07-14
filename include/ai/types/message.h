#pragma once

#include "enums.h"

#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace ai {

using JsonValue = nlohmann::json;

// Base content part types
struct TextContentPart {
  std::string text;

  explicit TextContentPart(std::string t) : text(std::move(t)) {}
};

struct ToolCallContentPart {
  std::string id;
  std::string tool_name;
  JsonValue arguments;

  ToolCallContentPart(std::string i, std::string n, JsonValue a)
      : id(std::move(i)), tool_name(std::move(n)), arguments(std::move(a)) {}
};

struct ToolResultContentPart {
  std::string tool_call_id;
  JsonValue result;
  bool is_error = false;

  ToolResultContentPart(std::string id, JsonValue r, bool err = false)
      : tool_call_id(std::move(id)), result(std::move(r)), is_error(err) {}
};

// Content part variant
using ContentPart =
    std::variant<TextContentPart, ToolCallContentPart, ToolResultContentPart>;

// Message content is now a vector of content parts
using MessageContent = std::vector<ContentPart>;

struct Message {
  MessageRole role;
  MessageContent content;

  Message(MessageRole r, MessageContent c) : role(r), content(std::move(c)) {}

  // Factory methods for convenience
  static Message system(const std::string& text) {
    return Message(kMessageRoleSystem, {TextContentPart{text}});
  }

  static Message user(const std::string& text) {
    return Message(kMessageRoleUser, {TextContentPart{text}});
  }

  static Message assistant(const std::string& text) {
    return Message(kMessageRoleAssistant, {TextContentPart{text}});
  }

  static Message assistant_with_tools(
      const std::string& text,
      const std::vector<ToolCallContentPart>& tools) {
    MessageContent content_parts;

    // Add text content if not empty
    if (!text.empty()) {
      content_parts.emplace_back(TextContentPart{text});
    }

    // Add tool calls
    for (const auto& tool : tools) {
      content_parts.emplace_back(
          ToolCallContentPart{tool.id, tool.tool_name, tool.arguments});
    }

    return Message(kMessageRoleAssistant, std::move(content_parts));
  }

  static Message tool_results(
      const std::vector<ToolResultContentPart>& results) {
    MessageContent content_parts;
    for (const auto& result : results) {
      content_parts.emplace_back(ToolResultContentPart{
          result.tool_call_id, result.result, result.is_error});
    }
    return Message(kMessageRoleUser, std::move(content_parts));
  }

  // Helper methods
  bool has_text() const {
    return std::any_of(content.begin(), content.end(),
                       [](const ContentPart& part) {
                         return std::holds_alternative<TextContentPart>(part);
                       });
  }

  bool has_tool_calls() const {
    return std::any_of(
        content.begin(), content.end(), [](const ContentPart& part) {
          return std::holds_alternative<ToolCallContentPart>(part);
        });
  }

  bool has_tool_results() const {
    return std::any_of(
        content.begin(), content.end(), [](const ContentPart& part) {
          return std::holds_alternative<ToolResultContentPart>(part);
        });
  }

  std::string get_text() const {
    std::string result;
    for (const auto& part : content) {
      if (const auto* text_part = std::get_if<TextContentPart>(&part)) {
        result += text_part->text;
      }
    }
    return result;
  }

  std::vector<ToolCallContentPart> get_tool_calls() const {
    std::vector<ToolCallContentPart> result;
    for (const auto& part : content) {
      if (const auto* tool_part = std::get_if<ToolCallContentPart>(&part)) {
        result.emplace_back(tool_part->id, tool_part->tool_name,
                            tool_part->arguments);
      }
    }
    return result;
  }

  std::vector<ToolResultContentPart> get_tool_results() const {
    std::vector<ToolResultContentPart> result;
    for (const auto& part : content) {
      if (const auto* result_part = std::get_if<ToolResultContentPart>(&part)) {
        result.emplace_back(result_part->tool_call_id, result_part->result,
                            result_part->is_error);
      }
    }
    return result;
  }

  std::string roleToString() const {
    switch (role) {
      case kMessageRoleSystem:
        return "system";
      case kMessageRoleUser:
        return "user";
      case kMessageRoleAssistant:
        return "assistant";
      default:
        return "unknown";
    }
  }
};

using Messages = std::vector<Message>;

}  // namespace ai