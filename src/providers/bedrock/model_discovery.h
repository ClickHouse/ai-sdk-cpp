#pragma once

#include "ai/bedrock.h"  // For SecurityConfig, BedrockConfig, ModelInfo

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations for AWS SDK types
namespace Aws {
namespace Bedrock {
class BedrockClient;
}
}  // namespace Aws

namespace ai {
namespace bedrock {

/// Handles dynamic model discovery and caching
class ModelDiscovery {
 public:
  /// Create model discovery with configuration
  explicit ModelDiscovery(const BedrockConfig& config);

  /// Destructor
  ~ModelDiscovery();

  /// List available models (uses cache if valid)
  /// @return Vector of ModelInfo for available models
  /// @throws std::runtime_error if API call fails
  std::vector<ModelInfo> list_models();

  /// Check if a specific model is accessible
  /// @param model_id The model identifier to check
  /// @return true if model is in the available list
  bool is_model_accessible(const std::string& model_id);

  /// Invalidate the model cache
  /// Next call to list_models() will fetch fresh data
  void invalidate_cache();

  /// Check if cache is valid (not expired)
  /// @return true if cache is still valid
  bool is_cache_valid() const;

 private:
  /// Fetch models from AWS API
  std::vector<ModelInfo> fetch_models_from_api();

  /// Initialize the Bedrock client
  void initialize_client(const BedrockConfig& config);

  std::shared_ptr<Aws::Bedrock::BedrockClient> client_;
  std::vector<ModelInfo> cached_models_;
  std::chrono::steady_clock::time_point cache_time_;
  std::chrono::minutes cache_ttl_;
  mutable std::mutex cache_mutex_;
  std::string region_;
};

}  // namespace bedrock
}  // namespace ai
