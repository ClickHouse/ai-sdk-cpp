#pragma once

#ifndef AI_SDK_HAS_BEDROCK
#error \
    "Bedrock component not available. Link with ai::bedrock or enable AI_SDK_ENABLE_BEDROCK to use Bedrock functionality."
#endif

#include "types/client.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace ai {
namespace bedrock {

namespace models {
/// Claude models on Bedrock (using inference profiles for cross-region support)
/// Use inference profile IDs (us.*, eu.*, global.*) for on-demand access
constexpr const char* kClaudeSonnet = "us.anthropic.claude-3-5-sonnet-20241022-v2:0";
constexpr const char* kClaudeHaiku = "us.anthropic.claude-3-5-haiku-20241022-v1:0";
constexpr const char* kClaudeOpus = "us.anthropic.claude-3-opus-20240229-v1:0";
constexpr const char* kClaudeSonnet4 = "global.anthropic.claude-sonnet-4-20250514-v1:0";
constexpr const char* kClaudeSonnet45 = "us.anthropic.claude-sonnet-4-5-20250929-v1:0";

/// Amazon Titan models (direct model IDs - on-demand supported)
constexpr const char* kTitanTextExpress = "amazon.titan-text-express-v1";
constexpr const char* kTitanTextLite = "amazon.titan-text-lite-v1";
constexpr const char* kTitanTextPremier = "amazon.titan-text-premier-v1:0";

/// Amazon Nova models (direct model IDs - on-demand supported)
constexpr const char* kNovaLite = "amazon.nova-lite-v1:0";
constexpr const char* kNovaPro = "amazon.nova-pro-v1:0";
constexpr const char* kNovaMicro = "amazon.nova-micro-v1:0";

/// Meta Llama models
constexpr const char* kLlama3_8B = "meta.llama3-8b-instruct-v1:0";
constexpr const char* kLlama3_70B = "meta.llama3-70b-instruct-v1:0";

/// Default model used when none is specified
constexpr const char* kDefaultModel = kClaudeSonnet;
}  // namespace models

/// Model information returned by model discovery
struct ModelInfo {
  std::string model_id;       // Full model identifier
  std::string provider;       // Model provider (e.g., "anthropic", "amazon", "meta")
  std::string name;           // Human-readable model name
  bool supports_streaming;    // Whether model supports streaming
  bool supports_system_prompt;  // Whether model supports system prompts
  std::vector<std::string> capabilities;  // List of capabilities
};

/// Security configuration for the Bedrock client
struct SecurityConfig {
  /// Maximum concurrent requests per client (default: 10)
  int max_concurrent_requests = 10;

  /// Circuit breaker failure threshold before opening (default: 5)
  int circuit_breaker_threshold = 5;

  /// Circuit breaker reset timeout (default: 30 seconds)
  std::chrono::seconds circuit_breaker_timeout{30};

  /// Connection timeout (default: 5 seconds)
  std::chrono::seconds connection_timeout{5};

  /// Request timeout (default: 120 seconds)
  std::chrono::seconds request_timeout{120};

  /// Maximum prompt length in characters (default: 200000)
  size_t max_prompt_length = 200000;

  /// Maximum total message content length (default: 200000)
  size_t max_message_length = 200000;

  /// Maximum max_tokens value (default: 200000)
  int max_tokens_limit = 200000;

  /// Model list cache TTL (default: 5 minutes)
  std::chrono::minutes model_cache_ttl{5};
};

/// Configuration for Bedrock client
struct BedrockConfig {
  /// AWS region (e.g., "us-east-1"). If empty, uses AWS_REGION env var.
  std::string region;

  /// AWS profile name for credential resolution. If empty, uses default chain.
  std::string profile;

  /// Custom endpoint URL for VPC endpoints or local testing.
  std::string endpoint_override;

  /// IAM role ARN for cross-account access via STS AssumeRole.
  /// If set, the provider will assume this role before making API calls.
  std::string role_arn;

  /// External ID for STS AssumeRole (optional, for enhanced security).
  std::string external_id;

  /// Path to web identity token file for EKS/OIDC authentication.
  /// Used with role_arn for web identity federation.
  std::string web_identity_token_file;

  /// Role session name for STS AssumeRole (default: "bedrock-provider-session").
  std::string role_session_name = "bedrock-provider-session";

  /// Security configuration
  SecurityConfig security;
};

/// Create a Bedrock client with default configuration
/// Uses AWS_REGION environment variable and default credential chain
/// @throws ConfigurationError if region is not configured
/// @return Configured Bedrock client
Client create_client();

/// Create a Bedrock client with explicit configuration
/// @param config Bedrock configuration
/// @throws ConfigurationError if region is not configured
/// @return Configured Bedrock client
Client create_client(const BedrockConfig& config);

/// Try to create a Bedrock client using environment configuration
/// @return Optional client - has value if credentials and region are available
/// @note This is useful for chaining creation attempts with other providers
std::optional<Client> try_create_client();

/// List available foundation models in the user's AWS account
/// @param config Bedrock configuration (uses region and credentials)
/// @return Vector of ModelInfo for available models
/// @throws ConfigurationError if region is not configured
/// @throws std::runtime_error if API call fails
/// @note Results are cached according to SecurityConfig.model_cache_ttl
std::vector<ModelInfo> list_available_models(const BedrockConfig& config = {});

/// Validate that a model is accessible in the user's AWS account
/// @param model_id The model identifier to validate
/// @param config Bedrock configuration
/// @return true if the model is accessible, false otherwise
/// @note This does not throw - returns false for any access issues
bool validate_model_access(const std::string& model_id,
                           const BedrockConfig& config = {});

/// Get the provider name
/// @return "bedrock"
constexpr const char* provider_name() { return "bedrock"; }

}  // namespace bedrock
}  // namespace ai
