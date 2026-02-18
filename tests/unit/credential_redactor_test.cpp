// Credential Redactor Unit Tests
// Tests credential redaction in logs

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "providers/bedrock/credential_redactor.h"

namespace ai {
namespace bedrock {
namespace {

class CredentialRedactorTest : public ::testing::Test {};

// Test AWS Access Key ID redaction
TEST_F(CredentialRedactorTest, RedactsAccessKeyId) {
  std::string input = "Access key: AKIAIOSFODNN7EXAMPLE";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("AKIAIOSFODNN7EXAMPLE"), std::string::npos);
  EXPECT_NE(result.find("[REDACTED]"), std::string::npos);
}

TEST_F(CredentialRedactorTest, RedactsMultipleAccessKeyIds) {
  std::string input = "Keys: AKIAIOSFODNN7EXAMPLE and AKIAI44QH8DHBEXAMPLE";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("AKIAIOSFODNN7EXAMPLE"), std::string::npos);
  EXPECT_EQ(result.find("AKIAI44QH8DHBEXAMPLE"), std::string::npos);
}

// Test temporary credentials (ASIA prefix)
TEST_F(CredentialRedactorTest, RedactsTemporaryAccessKeyId) {
  // Test that ASIA-prefixed keys (temporary credentials) are redacted
  // ASIA + 16 uppercase alphanumeric chars = 20 chars total
  std::string input = "Temp key: ASIAX7EXAMPLE1234567";
  std::string result = CredentialRedactor::redact(input);
  
  // The ASIA pattern should be redacted
  EXPECT_EQ(result.find("ASIAX7EXAMPLE1234567"), std::string::npos);
  EXPECT_NE(result.find("[REDACTED]"), std::string::npos);
}

// Test session token redaction
TEST_F(CredentialRedactorTest, RedactsSessionToken) {
  // Session tokens start with specific patterns
  std::string input = "Token: FwoGZXIvYXdzEBYaDHVzLWVhc3QtMSJHMEUCIQDExample" +
                      std::string(100, 'A');
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("FwoGZXIvYXdzE"), std::string::npos);
  EXPECT_NE(result.find("[REDACTED]"), std::string::npos);
}

// Test account ID redaction in ARN context
TEST_F(CredentialRedactorTest, RedactsAccountIdInArn) {
  std::string input = "arn:aws:bedrock:us-east-1:123456789012:model/my-model";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("123456789012"), std::string::npos);
  EXPECT_NE(result.find("[REDACTED]"), std::string::npos);
  // ARN structure should be preserved
  EXPECT_NE(result.find("arn:aws:bedrock:us-east-1:"), std::string::npos);
}

// Test environment variable format redaction
TEST_F(CredentialRedactorTest, RedactsEnvVarFormat) {
  std::string input = "AWS_SECRET_ACCESS_KEY=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("wJalrXUtnFEMI"), std::string::npos);
  EXPECT_NE(result.find("[REDACTED]"), std::string::npos);
}

// Test that non-sensitive data is preserved
TEST_F(CredentialRedactorTest, PreservesNonSensitiveData) {
  std::string input = "Model: anthropic.claude-3-5-sonnet-v1:0, Region: us-east-1";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result, input);
}

// Test empty string handling
TEST_F(CredentialRedactorTest, HandlesEmptyString) {
  std::string input = "";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result, "");
}

// Test contains_sensitive_data detection
TEST_F(CredentialRedactorTest, DetectsSensitiveData) {
  EXPECT_TRUE(CredentialRedactor::contains_sensitive_data(
      "Key: AKIAIOSFODNN7EXAMPLE"));
  EXPECT_TRUE(CredentialRedactor::contains_sensitive_data(
      "arn:aws:bedrock:us-east-1:123456789012:model"));
  EXPECT_FALSE(CredentialRedactor::contains_sensitive_data(
      "Model: anthropic.claude-3-5-sonnet-v1:0"));
}

// Test mixed content
TEST_F(CredentialRedactorTest, RedactsMixedContent) {
  std::string input = 
      "Request to arn:aws:bedrock:us-east-1:123456789012:model "
      "with key AKIAIOSFODNN7EXAMPLE failed";
  std::string result = CredentialRedactor::redact(input);
  
  EXPECT_EQ(result.find("123456789012"), std::string::npos);
  EXPECT_EQ(result.find("AKIAIOSFODNN7EXAMPLE"), std::string::npos);
  EXPECT_NE(result.find("Request to"), std::string::npos);
  EXPECT_NE(result.find("failed"), std::string::npos);
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

TEST(CredentialRedactorTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
