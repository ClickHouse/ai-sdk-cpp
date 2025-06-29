#pragma once

namespace ai {

struct Usage {
  int prompt_tokens = 0;
  int completion_tokens = 0;
  int total_tokens = 0;

  explicit Usage(int prompt = 0, int completion = 0)
      : prompt_tokens(prompt),
        completion_tokens(completion),
        total_tokens(prompt + completion) {}

  bool is_valid() const {
    return total_tokens > 0 || (prompt_tokens > 0 && completion_tokens >= 0);
  }
};

}  // namespace ai