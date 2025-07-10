#pragma once

#include "ai/errors.h"

#include <string>
#include <vector>

namespace ai {
namespace retry {

enum class RetryErrorReason {
  kMaxRetriesExceeded,
  kErrorNotRetryable,
  kAborted
};

class RetryError : public AIError {
 public:
  RetryError(const std::string& message,
             RetryErrorReason reason,
             const std::vector<std::string>& errors)
      : AIError(message),
        reason_(reason),
        errors_(errors),
        last_error_(errors.empty() ? "" : errors.back()) {}

  RetryErrorReason reason() const { return reason_; }
  const std::vector<std::string>& errors() const { return errors_; }
  const std::string& last_error() const { return last_error_; }

  std::string reason_string() const {
    switch (reason_) {
      case RetryErrorReason::kMaxRetriesExceeded:
        return "maxRetriesExceeded";
      case RetryErrorReason::kErrorNotRetryable:
        return "errorNotRetryable";
      case RetryErrorReason::kAborted:
        return "abort";
      default:
        return "unknown";
    }
  }

 private:
  RetryErrorReason reason_;
  std::vector<std::string> errors_;
  std::string last_error_;
};

}  // namespace retry
}  // namespace ai