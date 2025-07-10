#pragma once

#include "retry_error.h"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

namespace ai {
namespace retry {

struct RetryConfig {
  int max_retries = 2;
  std::chrono::milliseconds initial_delay{2000};
  double backoff_factor = 2.0;
};

class RetryPolicy {
 public:
  explicit RetryPolicy(const RetryConfig& config = RetryConfig())
      : config_(config) {}

  template <typename Func, typename Result>
  Result execute_with_retry(Func&& func,
                            std::function<bool(const Result&)> is_retryable) {
    std::vector<std::string> errors;
    std::chrono::milliseconds current_delay = config_.initial_delay;

    for (int attempt = 0; attempt <= config_.max_retries; ++attempt) {
      Result result = func();

      // Check if the result indicates a retryable error
      if (!result.is_success()) {
        errors.push_back(result.error_message());

        if (is_retryable(result) && attempt < config_.max_retries) {
          // Wait before retrying
          std::this_thread::sleep_for(current_delay);
          auto delay_ms =
              std::chrono::duration<double, std::milli>(current_delay) *
              config_.backoff_factor;
          current_delay =
              std::chrono::duration_cast<std::chrono::milliseconds>(delay_ms);
          continue;
        }

        // Non-retryable error or max retries exceeded
        if (attempt == config_.max_retries) {
          throw RetryError(
              "Failed after " + std::to_string(attempt + 1) +
                  " attempts. Last error: " + result.error_message(),
              RetryErrorReason::kMaxRetriesExceeded, errors);
        } else {
          throw RetryError("Failed after " + std::to_string(attempt + 1) +
                               " attempts with non-retryable error: " +
                               result.error_message(),
                           RetryErrorReason::kErrorNotRetryable, errors);
        }
      }

      return result;
    }

    // This should never be reached due to the loop structure
    return Result("Retry logic error");
  }

 private:
  RetryConfig config_;
};

}  // namespace retry
}  // namespace ai