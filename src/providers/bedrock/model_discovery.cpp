#include "model_discovery.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "aws_sdk_manager.h"
#include "bedrock_credentials.h"
#include "secure_logger.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>

#include <aws/bedrock/BedrockClient.h>
#include <aws/bedrock/model/ListFoundationModelsRequest.h>
#include <aws/bedrock/model/ListFoundationModelsResult.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>

namespace ai {
namespace bedrock {

namespace {

// Extract provider from model ID (e.g., "anthropic" from
// "anthropic.claude-3-5-sonnet-v1:0")
std::string extract_provider(const std::string& model_id) {
  auto dot_pos = model_id.find('.');
  if (dot_pos != std::string::npos) {
    return model_id.substr(0, dot_pos);
  }
  return "unknown";
}

}  // namespace

ModelDiscovery::ModelDiscovery(const BedrockConfig& config) {
  // Ensure AWS SDK is initialized before creating any AWS clients
  ensure_aws_sdk_initialized();
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

  // Configure AWS client. `endpoint_override` is not applied: it is the
  // endpoint of the Bedrock runtime service, not of the Bedrock control plane
  // that lists the models.
  Aws::Client::ClientConfiguration client_config;
  client_config.region = region_;
  client_config.connectTimeoutMs =
      static_cast<long>(config.security.connection_timeout.count() * 1000);
  client_config.requestTimeoutMs =
      static_cast<long>(config.security.request_timeout.count() * 1000);

  // The same identity as `BedrockClient` (role, web identity, profile, or the
  // default chain)
  auto credentials_provider = make_credentials_provider(config, region_);

  // Create Bedrock client (not BedrockRuntime - this is for management APIs)
  client_ = std::make_shared<Aws::Bedrock::BedrockClient>(credentials_provider,
                                                          client_config);

  SecureLogger::log_debug("ModelDiscovery initialized for region: " + region_);
}

std::vector<ModelInfo> ModelDiscovery::list_models() {
  auto models = fetch_models_from_api();
  SecureLogger::log_info("Fetched " + std::to_string(models.size()) +
                         " models from Bedrock API");
  return models;
}

bool ModelDiscovery::is_model_accessible(const std::string& model_id) {
  auto models = list_models();

  // A cross-region inference profile ID (`us.anthropic.claude-...`,
  // `global.anthropic.claude-...`) refers to the foundation model ID after
  // the geography prefix
  std::string foundation_model_id = model_id;
  const auto first_dot = model_id.find('.');
  if (model_id.rfind("arn:", 0) != 0 && first_dot != std::string::npos &&
      model_id.find('.', first_dot + 1) != std::string::npos) {
    foundation_model_id = model_id.substr(first_dot + 1);
  }

  return std::any_of(models.begin(), models.end(), [&](const ModelInfo& info) {
    return info.model_id == model_id || info.model_id == foundation_model_id;
  });
}

std::vector<ModelInfo> ModelDiscovery::fetch_models_from_api() {
  std::vector<ModelInfo> models;

  Aws::Bedrock::Model::ListFoundationModelsRequest request;

  auto outcome = client_->ListFoundationModels(request);

  if (!outcome.IsSuccess()) {
    const auto& error = outcome.GetError();
    SecureLogger::log_error(
        "Failed to list foundation models: " + error.GetExceptionName() +
        " - " + error.GetMessage());
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
