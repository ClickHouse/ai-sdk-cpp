#pragma once

#include "ai/types/client.h"

#include <optional>
#include <string>

namespace ai {
namespace bedrock {

struct BedrockConfig;

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

}  // namespace bedrock
}  // namespace ai
