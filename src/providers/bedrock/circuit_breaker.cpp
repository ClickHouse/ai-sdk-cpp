#include "circuit_breaker.h"

#include "secure_logger.h"

namespace ai {
namespace bedrock {

CircuitBreaker::CircuitBreaker()
    : threshold_(5), timeout_(std::chrono::seconds(30)) {}

CircuitBreaker::CircuitBreaker(const SecurityConfig& config)
    : threshold_(config.circuit_breaker_threshold),
      timeout_(config.circuit_breaker_timeout) {}

bool CircuitBreaker::is_open() {
  std::lock_guard<std::mutex> lock(mutex_);

  State current_state = state_.load();

  switch (current_state) {
    case State::Closed:
      return false;

    case State::Open:
      // Check if we should transition to HalfOpen
      if (timeout_elapsed()) {
        transition_to(State::HalfOpen);
        SecureLogger::log_debug("Circuit breaker transitioning to HalfOpen");
        return false;  // Allow one request through
      }
      return true;  // Still open, reject request

    case State::HalfOpen:
      // In HalfOpen, we allow requests through to test recovery
      return false;

    default:
      return false;
  }
}

void CircuitBreaker::record_success() {
  std::lock_guard<std::mutex> lock(mutex_);

  State current_state = state_.load();

  switch (current_state) {
    case State::Closed:
      // Reset failure count on success
      failure_count_ = 0;
      break;

    case State::HalfOpen:
      // Success in HalfOpen means service recovered
      transition_to(State::Closed);
      failure_count_ = 0;
      SecureLogger::log_info("Circuit breaker closed - service recovered");
      break;

    case State::Open:
      // Shouldn't happen, but handle gracefully
      break;
  }
}

void CircuitBreaker::record_failure() {
  std::lock_guard<std::mutex> lock(mutex_);

  State current_state = state_.load();
  last_failure_time_ = std::chrono::steady_clock::now();

  switch (current_state) {
    case State::Closed:
      failure_count_++;
      if (failure_count_ >= threshold_) {
        transition_to(State::Open);
        SecureLogger::log_warn(
            "Circuit breaker opened after " + std::to_string(failure_count_) +
            " consecutive failures");
      }
      break;

    case State::HalfOpen:
      // Failure in HalfOpen means service still unhealthy
      transition_to(State::Open);
      SecureLogger::log_warn(
          "Circuit breaker reopened - service still unhealthy");
      break;

    case State::Open:
      // Already open, just update failure time
      break;
  }
}

CircuitBreaker::State CircuitBreaker::get_state() const {
  return state_.load();
}

int CircuitBreaker::get_failure_count() const {
  return failure_count_.load();
}

void CircuitBreaker::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = State::Closed;
  failure_count_ = 0;
  SecureLogger::log_debug("Circuit breaker reset to Closed state");
}

bool CircuitBreaker::timeout_elapsed() const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      now - last_failure_time_);
  return elapsed >= timeout_;
}

void CircuitBreaker::transition_to(State new_state) {
  state_ = new_state;
}

}  // namespace bedrock
}  // namespace ai
