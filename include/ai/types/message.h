#pragma once

#include "enums.h"

#include <string>
#include <vector>

namespace ai {

struct Message {
  MessageRole role;
  std::string content;

  Message(MessageRole r, std::string c) : role(r), content(std::move(c)) {}

  static Message system(const std::string& content) {
    return Message(kMessageRoleSystem, content);
  }

  static Message user(const std::string& content) {
    return Message(kMessageRoleUser, content);
  }

  static Message assistant(const std::string& content) {
    return Message(kMessageRoleAssistant, content);
  }

  bool empty() const { return content.empty(); }

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