#pragma once

#include "ai/bedrock.h"  // For SecurityConfig

#include <atomic>
#include <chrono>
#include <mutex>

namespace ai {
namespace bedrock {

/// Circuit breaker pattern implementation for fault tolerance
///
/// State machine:
/// - Closed: Normal operation, requests pass through
/// - Open: Circuit tripped, requests fail immediately
/// - HalfOpen: Testing if service recovered, one request allowed
///
/// Transitions:
/// - Closed -> Open: When failure_count >= threshold
/// - Open -> HalfOpen: After timeout elapsed
/// - HalfOpen -> Closed: On success
/// - HalfOpen -> Open: On failure
class CircuitBreaker {
 public:
  /// Circuit breaker states
  enum class State {
    Closed,   // Normal operation
    Open,     // Circuit tripped, rejecting requests
    HalfOpen  // Testing recovery
  };

  /// Create circuit breaker with default configuration
  CircuitBreaker();

  /// Create circuit breaker with custom configuration
  explicit CircuitBreaker(const SecurityConfig& config);

  /// Check if circuit is open (requests should be rejected)
  /// @return true if circuit is open and requests should fail fast
  bool is_open();

  /// Record a successful request
  /// Resets failure count in Closed state
  /// Closes circuit in HalfOpen state
  void record_success();

  /// Record a failed request
  /// Increments failure count, may trip circuit
  void record_failure();

  /// Get current state for diagnostics
  /// @return Current circuit state
  State get_state() const;

  /// Get current failure count
  /// @return Number of consecutive failures
  int get_failure_count() const;

  /// Reset the circuit breaker to closed state
  void reset();

 private:
  /// Check if timeout has elapsed since last failure (for Open -> HalfOpen)
  bool timeout_elapsed() const;

  /// Transition to a new state
  void transition_to(State new_state);

  mutable std::mutex mutex_;
  std::atomic<State> state_{State::Closed};
  std::atomic<int> failure_count_{0};
  std::chrono::steady_clock::time_point last_failure_time_;

  int threshold_;
  std::chrono::seconds timeout_;
};

/// Convert circuit breaker state to string for logging
inline const char* to_string(CircuitBreaker::State state) {
  switch (state) {
    case CircuitBreaker::State::Closed:
      return "Closed";
    case CircuitBreaker::State::Open:
      return "Open";
    case CircuitBreaker::State::HalfOpen:
      return "HalfOpen";
    default:
      return "Unknown";
  }
}

}  // namespace bedrock
}  // namespace ai
