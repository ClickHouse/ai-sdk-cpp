#pragma once

#include <string>

namespace ai {
namespace bedrock {

/// Redacts sensitive information from strings before logging
class CredentialRedactor {
 public:
  /// Placeholder text for redacted content
  static constexpr const char* kRedactedPlaceholder = "[REDACTED]";

  /// Redact all sensitive patterns from a string
  /// Patterns redacted:
  /// - AWS Access Key IDs (AKIA...)
  /// - AWS Secret Access Keys (40-char base64-like strings)
  /// - Session tokens (long base64 strings)
  /// - Account IDs in ARN context
  /// @param input The string to redact
  /// @return String with sensitive data replaced by [REDACTED]
  static std::string redact(const std::string& input);

  /// Check if a string contains sensitive patterns
  /// @param input The string to check
  /// @return true if sensitive patterns are detected
  static bool contains_sensitive_data(const std::string& input);

  /// Redact AWS Access Key ID pattern (AKIA followed by 16 alphanumeric chars)
  static std::string redact_access_key_id(const std::string& input);

  /// Redact AWS Secret Access Key pattern (40 char base64-like string)
  static std::string redact_secret_key(const std::string& input);

  /// Redact AWS Session Token (long base64 string)
  static std::string redact_session_token(const std::string& input);

  /// Redact AWS Account IDs in ARN context
  static std::string redact_account_id(const std::string& input);
};

}  // namespace bedrock
}  // namespace ai
