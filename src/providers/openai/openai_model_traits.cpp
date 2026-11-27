#include "openai_model_traits.h"

namespace ai {
namespace openai {

bool is_reasoning_family(std::string_view model) {
  return model.starts_with("gpt-5") || model.starts_with("gpt-6");
}

std::optional<std::string_view> lowest_reasoning_effort(
    std::string_view model) {
  if (model.starts_with("gpt-6")) {
    // The pro and cyber variants have value sets of their own.
    for (const std::string_view variant : {"-pro", "-cyber"}) {
      if (model.find(variant) != std::string_view::npos) {
        return std::nullopt;
      }
    }
    // Every GPT-6 model accepts "low"; GPT-6.2 and later reject "none", which
    // only GPT-6 Luna and GPT-6.1 Terra are documented to accept.
    if (model.starts_with("gpt-6-luna") || model.starts_with("gpt-6.1-terra")) {
      return "none";
    }
    return "low";
  }

  if (!model.starts_with("gpt-5")) {
    return std::nullopt;
  }

  // The pro variants only take the higher efforts (and are served by the
  // Responses API only), the chat variants are the non-reasoning ChatGPT
  // snapshots, and the codex variants have value sets of their own.
  for (const std::string_view variant : {"-pro", "-chat", "-codex"}) {
    if (model.find(variant) != std::string_view::npos) {
      return std::nullopt;
    }
  }

  // GPT-5.1 and later ("gpt-5.1", "gpt-5.4-mini", "gpt-5.6-sol", ...) accept
  // "none", which replaced "minimal": they reject "minimal".
  if (model.starts_with("gpt-5.")) {
    return "none";
  }

  // The original GPT-5 models ("gpt-5", "gpt-5-mini", "gpt-5-nano" and their
  // dated snapshots) reject "none" ("Supported values are: 'minimal', 'low',
  // 'medium', and 'high'."); "minimal" is their floor.
  if (model == "gpt-5" || model.starts_with("gpt-5-")) {
    return "minimal";
  }

  return std::nullopt;
}

}  // namespace openai
}  // namespace ai
