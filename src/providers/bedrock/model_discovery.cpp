#include "model_discovery.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "aws_sdk_manager.h"
#include "secure_logger.h"

#include <aws/bedrock/BedrockClient.h>
#include <aws/bedrock/model/ListFoundationModelsRequest.h>
#include <aws/bedrock/model/ListFoundationModelsResult.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>

namespace ai {
namespace bedrock {

namespace {

// Helper to get region from environment
std::string get_region_from_env() {
  if (const char* region = std::getenv("AWS_REGION")) {
    return region;
  }
  if (const char* region = std::getenv("AWS_DEFAULT_REGION")) {
    return region;
  }
  return "";
}

// Extract provider from model ID (e.g., "anthropic" from "anthropic.claude-3-5-sonnet-v1:0")
std::string extract_provider(const std::string& model_id) {
  auto dot_pos = model_id.find('.');
  if (dot_pos != std::string::npos) {
    return model_id.substr(0, dot_pos);
  }
  return "unknown";
}

}  // namespace

ModelDiscovery::ModelDiscovery(const BedrockConfig& config)
    : cache_ttl_(config.security.model_cache_ttl) {
  // Ensure AWS SDK is initialized before creating any AWS clients
  AwsSdkManager::instance().ensure_initialized();
  initialize_client(config);
}

ModelDiscovery::~ModelDiscovery() = default;

void ModelDiscovery::initialize_client(const BedrockConfig& config) {
  // Resolve region
  region_ = config.region;
  if (region_.empty()) {
    region_ = get_region_from_env();
  }

  if (region_.empty()) {
    throw ConfigurationError(
        "AWS region not configured for model discovery. Set AWS_REGION "
        "environment variable or provide region in BedrockConfig.");
  }

  // Configure AWS client
  Aws::Client::ClientConfiguration client_config;
  client_config.region = region_;

  if (!config.endpoint_override.empty()) {
    client_config.endpointOverride = config.endpoint_override;
  }

  // Set timeouts from security config
  client_config.connectTimeoutMs =
      static_cast<long>(config.security.connection_timeout.count() * 1000);
  client_config.requestTimeoutMs =
      static_cast<long>(config.security.request_timeout.count() * 1000);

  // Create credentials provider
  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credentials_provider;

  if (!config.profile.empty()) {
    credentials_provider =
        std::make_shared<Aws::Auth::ProfileConfigFileAWSCredentialsProvider>(
            config.profile.c_str());
  } else {
    credentials_provider =
        std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>();
  }

  // Create Bedrock client (not BedrockRuntime - this is for management APIs)
  client_ = std::make_shared<Aws::Bedrock::BedrockClient>(credentials_provider,
                                                          client_config);

  SecureLogger::log_debug("ModelDiscovery initialized for region: " + region_);
}

std::vector<ModelInfo> ModelDiscovery::list_models() {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  // Return cached results if still valid
  if (is_cache_valid() && !cached_models_.empty()) {
    SecureLogger::log_debug("Returning cached model list (" +
                            std::to_string(cached_models_.size()) + " models)");
    return cached_models_;
  }

  // Fetch fresh data from API
  cached_models_ = fetch_models_from_api();
  cache_time_ = std::chrono::steady_clock::now();

  SecureLogger::log_info("Fetched " + std::to_string(cached_models_.size()) +
                         " models from Bedrock API");

  return cached_models_;
}

bool ModelDiscovery::is_model_accessible(const std::string& model_id) {
  auto models = list_models();

  return std::any_of(models.begin(), models.end(),
                     [&model_id](const ModelInfo& info) {
                       return info.model_id == model_id;
                     });
}

void ModelDiscovery::invalidate_cache() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  cached_models_.clear();
  SecureLogger::log_debug("Model cache invalidated");
}

bool ModelDiscovery::is_cache_valid() const {
  if (cached_models_.empty()) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::minutes>(now - cache_time_);
  return elapsed < cache_ttl_;
}

std::vector<ModelInfo> ModelDiscovery::fetch_models_from_api() {
  std::vector<ModelInfo> models;

  Aws::Bedrock::Model::ListFoundationModelsRequest request;

  auto outcome = client_->ListFoundationModels(request);

  if (!outcome.IsSuccess()) {
    const auto& error = outcome.GetError();
    SecureLogger::log_error("Failed to list foundation models: " +
                            error.GetExceptionName() + " - " +
                            error.GetMessage());
    throw std::runtime_error("Failed to list foundation models: " +
                             error.GetMessage());
  }

  const auto& result = outcome.GetResult();

  for (const auto& model_summary : result.GetModelSummaries()) {
    ModelInfo info;
    info.model_id = model_summary.GetModelId();
    info.provider = extract_provider(info.model_id);
    info.name = model_summary.GetModelName();

    // Check streaming support
    info.supports_streaming = model_summary.GetResponseStreamingSupported();

    // Check for system prompt support (inferred from input modalities)
    const auto& input_modalities = model_summary.GetInputModalities();
    info.supports_system_prompt =
        std::find(input_modalities.begin(), input_modalities.end(),
                  Aws::Bedrock::Model::ModelModality::TEXT) !=
        input_modalities.end();

    // Build capabilities list
    for (const auto& modality : input_modalities) {
      switch (modality) {
        case Aws::Bedrock::Model::ModelModality::TEXT:
          info.capabilities.push_back("text");
          break;
        case Aws::Bedrock::Model::ModelModality::IMAGE:
          info.capabilities.push_back("image");
          break;
        case Aws::Bedrock::Model::ModelModality::EMBEDDING:
          info.capabilities.push_back("embedding");
          break;
        default:
          break;
      }
    }

    models.push_back(std::move(info));
  }

  return models;
}

}  // namespace bedrock
}  // namespace ai
