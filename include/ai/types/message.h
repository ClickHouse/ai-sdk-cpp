#pragma once

#include "enums.h"

#include <string>
#include <vector>

namespace ai {

/// A single message in a conversation
struct Message {
  MessageRole role;     ///< Role of the message sender
  std::string content;  ///< Text content of the message

  /// Constructor
  Message(MessageRole r, std::string c) : role(r), content(std::move(c)) {}

  /// Factory method to create a system message
  static Message system(const std::string& content) {
    return Message(kMessageRoleSystem, content);
  }

  /// Factory method to create a user message
  static Message user(const std::string& content) {
    return Message(kMessageRoleUser, content);
  }

  /// Factory method to create an assistant message
  static Message assistant(const std::string& content) {
    return Message(kMessageRoleAssistant, content);
  }

  /// Check if message is empty
  bool empty() const { return content.empty(); }

  /// Get role as string (for debugging/logging)
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

/// Collection of messages representing a conversation
using Messages = std::vector<Message>;

}  // namespace ai