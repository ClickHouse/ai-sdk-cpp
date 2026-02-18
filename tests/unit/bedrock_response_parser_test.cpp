// Bedrock Response Parser Unit Tests
// Tests response parsing, error mapping, and retryability classification

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/bedrock_response_parser.h"

#include "ai/bedrock.h"
#include "ai/core.h"

#include <aws/bedrock-runtime/BedrockRuntimeErrors.h>
#include <aws/bedrock-runtime/model/ConverseResult.h>
#include <aws/bedrock-runtime/model/StopReason.h>

namespace ai {
namespace bedrock {
namespace {

class BedrockResponseParserTest : public ::testing::Test {
 protected:
  BedrockResponseParser parser_;
};

// Test error retryability - ThrottlingException
TEST_F(BedrockResponseParserTest, ThrottlingErrorIsRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::THROTTLING, "ThrottlingException",
      "Rate exceeded", false);

  EXPECT_TRUE(parser_.is_error_retryable(error));
}

// Test error retryability - ServiceUnavailable
TEST_F(BedrockResponseParserTest, ServiceUnavailableIsRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::SERVICE_UNAVAILABLE,
      "ServiceUnavailableException", "Service temporarily unavailable", false);

  EXPECT_TRUE(parser_.is_error_retryable(error));
}

// Test error retryability - InternalFailure
TEST_F(BedrockResponseParserTest, InternalFailureIsRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::INTERNAL_FAILURE,
      "InternalServerException", "Internal server error", false);

  EXPECT_TRUE(parser_.is_error_retryable(error));
}

// Test error retryability - ValidationException
TEST_F(BedrockResponseParserTest, ValidationErrorIsNotRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::VALIDATION, "ValidationException",
      "Invalid request", false);

  EXPECT_FALSE(parser_.is_error_retryable(error));
}

// Test error retryability - AccessDenied
TEST_F(BedrockResponseParserTest, AccessDeniedIsNotRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::ACCESS_DENIED,
      "AccessDeniedException", "Access denied", false);

  EXPECT_FALSE(parser_.is_error_retryable(error));
}

// Test error retryability - ResourceNotFound
TEST_F(BedrockResponseParserTest, ResourceNotFoundIsNotRetryable) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::RESOURCE_NOT_FOUND,
      "ResourceNotFoundException", "Model not found", false);

  EXPECT_FALSE(parser_.is_error_retryable(error));
}

// Test error message contains exception name
TEST_F(BedrockResponseParserTest, ErrorMessageContainsExceptionName) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::VALIDATION, "ValidationException",
      "Invalid model ID", false);

  auto result = parser_.parse_converse_error(error);

  EXPECT_TRUE(result.error.has_value());
  EXPECT_NE(result.error->find("ValidationException"), std::string::npos);
  EXPECT_NE(result.error->find("Invalid model ID"), std::string::npos);
}

// Test error result has correct finish reason
TEST_F(BedrockResponseParserTest, ErrorResultHasErrorFinishReason) {
  Aws::Client::AWSError<Aws::BedrockRuntime::BedrockRuntimeErrors> error(
      Aws::BedrockRuntime::BedrockRuntimeErrors::VALIDATION, "ValidationException",
      "Test error", false);

  auto result = parser_.parse_converse_error(error);

  EXPECT_EQ(result.finish_reason, kFinishReasonError);
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

// Placeholder test when Bedrock is not enabled
TEST(BedrockResponseParserTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
