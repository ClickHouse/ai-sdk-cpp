#include "bedrock_factory.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "bedrock_client.h"
#include "model_discovery.h"
#include "secure_logger.h"

#include <cstdlib>
#include <memory>
#include <optional>

namespace ai {
namespace bedrock {

Client create_client() {
  // Create client with default configuration
  // Uses AWS_REGION environment variable and default credential chain
  BedrockConfig config;
  return create_client(config);
}

Client create_client(const BedrockConfig& config) {
  // Create client with explicit configuration
  SecureLogger::log_debug("Creating Bedrock client with explicit config");
  return Client(std::make_unique<BedrockClient>(config));
}

std::optional<Client> try_create_client() {
  // Try to create client without throwing
  // Returns nullopt if credentials or region are unavailable
  SecureLogger::log_debug("Attempting to create Bedrock client");

  try {
    BedrockConfig config;
    auto client = std::make_unique<BedrockClient>(config);

    if (client->is_valid()) {
      SecureLogger::log_debug("Bedrock client created successfully");
      return Client(std::move(client));
    }

    SecureLogger::log_debug("Bedrock client is not valid");
    return std::nullopt;

  } catch (const ConfigurationError& e) {
    // Configuration error (e.g., missing region) - return nullopt
    SecureLogger::log_debug("Bedrock client creation failed: " +
                            std::string(e.what()));
    return std::nullopt;
  } catch (const std::exception& e) {
    // Any other error - return nullopt
    SecureLogger::log_debug("Bedrock client creation failed with exception: " +
                            std::string(e.what()));
    return std::nullopt;
  }
}

std::vector<ModelInfo> list_available_models(const BedrockConfig& config) {
  // List available models from user's AWS account
  SecureLogger::log_debug("Listing available models");

  try {
    ModelDiscovery discovery(config);
    return discovery.list_models();
  } catch (const std::exception& e) {
    SecureLogger::log_error("Failed to list models: " + std::string(e.what()));
    throw;
  }
}

bool validate_model_access(const std::string& model_id,
                           const BedrockConfig& config) {
  // Validate model access - returns false for any access issues, does not throw
  SecureLogger::log_debug("Validating model access: " + model_id);

  try {
    ModelDiscovery discovery(config);
    return discovery.is_model_accessible(model_id);
  } catch (const std::exception& e) {
    SecureLogger::log_debug("Model access validation failed: " +
                            std::string(e.what()));
    return false;
  }
}

}  // namespace bedrock
}  // namespace ai
