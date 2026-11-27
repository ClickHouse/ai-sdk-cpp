#include "credential_redactor.h"

#include <regex>

namespace ai {
namespace bedrock {

namespace {
// AWS Access Key ID pattern: AKIA followed by 16 alphanumeric characters
// Also matches ASIA (temporary credentials) and AIDA (IAM user IDs)
const std::regex kAccessKeyIdPattern(R"((A[KS]IA|AIDA)[A-Z0-9]{16})");

// AWS Secret Access Key pattern: 40 character base64-like string
// Note: We use a simpler pattern since std::regex doesn't support lookbehind
// This is used in context-aware redaction only
const std::regex kSecretKeyPattern(R"([A-Za-z0-9+/]{40})");

// AWS Session Token pattern: Long base64 string (typically 300+ chars)
// Session tokens start with specific patterns
const std::regex kSessionTokenPattern(
    R"((FwoGZXIvYXdzE|IQoJb3JpZ2luX2Vj)[A-Za-z0-9+/=]{100,})");

// AWS Account ID in ARN context: 12 digit number after arn:aws:...:
const std::regex kAccountIdInArnPattern(
    R"((arn:aws:[a-z0-9-]+:[a-z0-9-]*:)([0-9]{12})(:))");

// Generic 12-digit account ID pattern (more aggressive)
const std::regex kAccountIdPattern(R"(\b[0-9]{12}\b)");

// AWS credentials in environment variable format
const std::regex kEnvCredentialPattern(
    R"((AWS_SECRET_ACCESS_KEY|AWS_SESSION_TOKEN|aws_secret_access_key|aws_session_token)\s*[=:]\s*([^\s'"]+))");

}  // namespace

std::string CredentialRedactor::redact(const std::string& input) {
  if (input.empty()) {
    return input;
  }

  std::string result = input;

  // Order matters - redact more specific patterns first
  result = redact_session_token(result);
  result = redact_access_key_id(result);
  result = redact_secret_key(result);
  result = redact_account_id(result);

  // Redact credentials in environment variable format
  result = std::regex_replace(result, kEnvCredentialPattern,
                              "$1=" + std::string(kRedactedPlaceholder));

  return result;
}

bool CredentialRedactor::contains_sensitive_data(const std::string& input) {
  if (input.empty()) {
    return false;
  }

  // Check for any sensitive patterns
  if (std::regex_search(input, kAccessKeyIdPattern)) {
    return true;
  }
  if (std::regex_search(input, kSessionTokenPattern)) {
    return true;
  }
  if (std::regex_search(input, kAccountIdInArnPattern)) {
    return true;
  }
  if (std::regex_search(input, kEnvCredentialPattern)) {
    return true;
  }

  // Secret key detection is more heuristic, so we're conservative here
  // Only flag if it looks like it's in a credential context
  return false;
}

std::string CredentialRedactor::redact_access_key_id(const std::string& input) {
  return std::regex_replace(input, kAccessKeyIdPattern, kRedactedPlaceholder);
}

std::string CredentialRedactor::redact_secret_key(const std::string& input) {
  // Be more conservative with secret key detection to avoid false positives
  // Only redact if it appears to be in a credential context
  std::string result = input;

  // Look for patterns like "SecretAccessKey": "..." or secret_access_key=...
  std::regex secret_context_pattern(
      R"((secret[_-]?access[_-]?key|SecretAccessKey)\s*[=:"']\s*([A-Za-z0-9+/]{40}))",
      std::regex::icase);

  result = std::regex_replace(result, secret_context_pattern,
                              "$1" + std::string(kRedactedPlaceholder));

  return result;
}

std::string CredentialRedactor::redact_session_token(const std::string& input) {
  return std::regex_replace(input, kSessionTokenPattern, kRedactedPlaceholder);
}

std::string CredentialRedactor::redact_account_id(const std::string& input) {
  // Only redact account IDs in ARN context to avoid false positives
  return std::regex_replace(input, kAccountIdInArnPattern,
                            "$1" + std::string(kRedactedPlaceholder) + "$3");
}

}  // namespace bedrock
}  // namespace ai
