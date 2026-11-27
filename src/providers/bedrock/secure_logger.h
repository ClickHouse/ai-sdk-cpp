#pragma once

#include <chrono>
#include <string>

namespace ai {
namespace bedrock {

/// Secure logging wrapper that ensures sensitive data is never logged
class SecureLogger {
 public:
  /// Maximum content length for debug logging (chars)
  static constexpr size_t kMaxDebugContentLength = 500;

  /// Log at debug level with content truncation and redaction
  /// Content is truncated to kMaxDebugContentLength and redacted
  /// @param message The log message
  /// @param content Optional content to log (will be truncated/redacted)
  /// @param max_content_length Maximum content length (default: 100)
  static void log_debug(const std::string& message,
                        const std::string& content = "",
                        size_t max_content_length = 100);

  /// Log at info level - NO content allowed
  /// Only operation summaries and metrics
  /// @param message The log message (will be redacted)
  static void log_info(const std::string& message);

  /// Log at warning level with redaction
  /// @param message The warning message (will be redacted)
  static void log_warn(const std::string& message);

  /// Log at error level with full redaction
  /// @param message The error message (will be redacted)
  static void log_error(const std::string& message);

  /// Log API call with observability fields
  /// Logs: operation, model, region, request_id, latency
  /// @param operation The API operation (e.g., "Converse", "ConverseStream")
  /// @param model The model ID used
  /// @param region The AWS region
  /// @param request_id The AWS request ID (for troubleshooting)
  /// @param latency The request latency
  static void log_api_call(const std::string& operation,
                           const std::string& model,
                           const std::string& region,
                           const std::string& request_id,
                           std::chrono::milliseconds latency);

  /// Log API error with observability fields
  /// @param operation The API operation
  /// @param model The model ID
  /// @param region The AWS region
  /// @param error_code The error code
  /// @param error_message The error message (will be redacted)
  /// @param is_retryable Whether the error is retryable
  static void log_api_error(const std::string& operation,
                            const std::string& model,
                            const std::string& region,
                            const std::string& error_code,
                            const std::string& error_message,
                            bool is_retryable);

 private:
  /// Truncate content to specified length, appending "..." if truncated
  static std::string truncate_content(const std::string& content,
                                      size_t max_length);

  /// Redact and sanitize a message for logging
  static std::string sanitize_for_log(const std::string& message);
};

}  // namespace bedrock
}  // namespace ai
