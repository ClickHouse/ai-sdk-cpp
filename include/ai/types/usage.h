#pragma once

namespace ai {

/// Token usage statistics for API calls
struct Usage {
  int prompt_tokens = 0;      ///< Number of tokens in the prompt
  int completion_tokens = 0;  ///< Number of tokens in the completion
  int total_tokens = 0;       ///< Total tokens used (prompt + completion)

  /// Constructor for convenience
  Usage(int prompt = 0, int completion = 0)
      : prompt_tokens(prompt),
        completion_tokens(completion),
        total_tokens(prompt + completion) {}

  /// Check if usage information is available
  bool is_valid() const {
    return total_tokens > 0 || (prompt_tokens > 0 && completion_tokens >= 0);
  }
};

}  // namespace ai