#include "secure_logger.h"

#include "ai/logger.h"
#include "credential_redactor.h"

namespace ai {
namespace bedrock {

void SecureLogger::log_debug(const std::string& message,
                             const std::string& content,
                             size_t max_content_length) {
  std::string sanitized_message = sanitize_for_log(message);

  if (content.empty()) {
    ai::logger::log_debug("[bedrock] {}", sanitized_message);
  } else {
    // Truncate and redact content
    std::string truncated = truncate_content(content, max_content_length);
    std::string sanitized_content = sanitize_for_log(truncated);
    ai::logger::log_debug("[bedrock] {} - content: {}", sanitized_message,
                          sanitized_content);
  }
}

void SecureLogger::log_info(const std::string& message) {
  // INFO level: NO content, only operation summaries
  std::string sanitized = sanitize_for_log(message);
  ai::logger::log_info("[bedrock] {}", sanitized);
}

void SecureLogger::log_warn(const std::string& message) {
  std::string sanitized = sanitize_for_log(message);
  ai::logger::log_warn("[bedrock] {}", sanitized);
}

void SecureLogger::log_error(const std::string& message) {
  std::string sanitized = sanitize_for_log(message);
  ai::logger::log_error("[bedrock] {}", sanitized);
}

void SecureLogger::log_api_call(const std::string& operation,
                                const std::string& model,
                                const std::string& region,
                                const std::string& request_id,
                                std::chrono::milliseconds latency) {
  // Structured logging for observability
  // Redact model in case it contains account info (custom model ARNs)
  std::string safe_model = sanitize_for_log(model);

  ai::logger::log_info(
      "[bedrock] API call completed - operation: {}, model: {}, region: {}, "
      "request_id: {}, latency_ms: {}",
      operation, safe_model, region, request_id, latency.count());
}

void SecureLogger::log_api_error(const std::string& operation,
                                 const std::string& model,
                                 const std::string& region,
                                 const std::string& error_code,
                                 const std::string& error_message,
                                 bool is_retryable) {
  std::string safe_model = sanitize_for_log(model);
  std::string safe_error = sanitize_for_log(error_message);

  ai::logger::log_error(
      "[bedrock] API error - operation: {}, model: {}, region: {}, "
      "error_code: {}, retryable: {}, message: {}",
      operation, safe_model, region, error_code, is_retryable ? "yes" : "no",
      safe_error);
}

std::string SecureLogger::truncate_content(const std::string& content,
                                           size_t max_length) {
  if (content.length() <= max_length) {
    return content;
  }

  // Truncate and add ellipsis
  return content.substr(0, max_length) + "...";
}

std::string SecureLogger::sanitize_for_log(const std::string& message) {
  // Use CredentialRedactor to remove any sensitive data
  return CredentialRedactor::redact(message);
}

}  // namespace bedrock
}  // namespace ai
