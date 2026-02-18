// Circuit Breaker Unit Tests
// Tests circuit breaker state machine behavior

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/circuit_breaker.h"

#include <thread>

namespace ai {
namespace bedrock {
namespace {

class CircuitBreakerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use short timeout for testing
    SecurityConfig config;
    config.circuit_breaker_threshold = 3;
    config.circuit_breaker_timeout = std::chrono::seconds(1);
    breaker_ = std::make_unique<CircuitBreaker>(config);
  }

  std::unique_ptr<CircuitBreaker> breaker_;
};

// Test initial state is Closed
TEST_F(CircuitBreakerTest, InitialStateIsClosed) {
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  EXPECT_FALSE(breaker_->is_open());
}

// Test circuit stays closed on success
TEST_F(CircuitBreakerTest, StaysClosedOnSuccess) {
  breaker_->record_success();
  breaker_->record_success();
  breaker_->record_success();
  
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  EXPECT_FALSE(breaker_->is_open());
}

// Test circuit opens after threshold failures
TEST_F(CircuitBreakerTest, OpensAfterThresholdFailures) {
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  
  breaker_->record_failure();  // Third failure - should open
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Open);
  EXPECT_TRUE(breaker_->is_open());
}

// Test success resets failure count
TEST_F(CircuitBreakerTest, SuccessResetsFailureCount) {
  breaker_->record_failure();
  breaker_->record_failure();
  breaker_->record_success();  // Reset
  
  EXPECT_EQ(breaker_->get_failure_count(), 0);
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  
  // Need 3 more failures to open
  breaker_->record_failure();
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
}

// Test transition to HalfOpen after timeout
TEST_F(CircuitBreakerTest, TransitionsToHalfOpenAfterTimeout) {
  // Open the circuit
  breaker_->record_failure();
  breaker_->record_failure();
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Open);
  
  // Wait for timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  
  // is_open() should trigger transition to HalfOpen
  EXPECT_FALSE(breaker_->is_open());
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::HalfOpen);
}

// Test HalfOpen closes on success
TEST_F(CircuitBreakerTest, HalfOpenClosesOnSuccess) {
  // Open the circuit
  breaker_->record_failure();
  breaker_->record_failure();
  breaker_->record_failure();
  
  // Wait for timeout to transition to HalfOpen
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  breaker_->is_open();  // Trigger transition
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::HalfOpen);
  
  // Success should close the circuit
  breaker_->record_success();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  EXPECT_FALSE(breaker_->is_open());
}

// Test HalfOpen reopens on failure
TEST_F(CircuitBreakerTest, HalfOpenReopensOnFailure) {
  // Open the circuit
  breaker_->record_failure();
  breaker_->record_failure();
  breaker_->record_failure();
  
  // Wait for timeout to transition to HalfOpen
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  breaker_->is_open();  // Trigger transition
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::HalfOpen);
  
  // Failure should reopen the circuit
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Open);
  EXPECT_TRUE(breaker_->is_open());
}

// Test reset functionality
TEST_F(CircuitBreakerTest, ResetClosesCircuit) {
  // Open the circuit
  breaker_->record_failure();
  breaker_->record_failure();
  breaker_->record_failure();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Open);
  
  // Reset should close it
  breaker_->reset();
  EXPECT_EQ(breaker_->get_state(), CircuitBreaker::State::Closed);
  EXPECT_EQ(breaker_->get_failure_count(), 0);
  EXPECT_FALSE(breaker_->is_open());
}

// Test default configuration
TEST_F(CircuitBreakerTest, DefaultConfiguration) {
  CircuitBreaker default_breaker;
  
  // Default threshold is 5
  for (int i = 0; i < 4; i++) {
    default_breaker.record_failure();
    EXPECT_EQ(default_breaker.get_state(), CircuitBreaker::State::Closed);
  }
  
  default_breaker.record_failure();  // 5th failure
  EXPECT_EQ(default_breaker.get_state(), CircuitBreaker::State::Open);
}

// Test to_string helper
TEST_F(CircuitBreakerTest, ToStringHelper) {
  EXPECT_STREQ(to_string(CircuitBreaker::State::Closed), "Closed");
  EXPECT_STREQ(to_string(CircuitBreaker::State::Open), "Open");
  EXPECT_STREQ(to_string(CircuitBreaker::State::HalfOpen), "HalfOpen");
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

TEST(CircuitBreakerTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
