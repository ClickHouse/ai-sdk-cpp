#include "bedrock_client.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "ai/tools.h"
#include "aws_sdk_manager.h"
#include "bedrock_request_mapper.h"
#include "bedrock_response_parser.h"
#include "bedrock_stream.h"
#include "input_validator.h"
#include "secure_logger.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <semaphore>

#include <aws/bedrock-runtime/BedrockRuntimeClient.h>
#include <aws/bedrock-runtime/model/ConverseRequest.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/auth/STSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSServiceClientModel.h>
#include <aws/sts/model/AssumeRoleRequest.h>

namespace ai {
namespace bedrock {

namespace {

// Refresh assumed-role credentials this long before they expire
constexpr auto kCredentialsRefreshMargin = std::chrono::minutes(5);

std::string get_region_from_env() {
  // Check AWS_REGION first, then AWS_DEFAULT_REGION
  if (const char* region = std::getenv("AWS_REGION")) {
    return region;
  }
  if (const char* region = std::getenv("AWS_DEFAULT_REGION")) {
    return region;
  }
  return "";
}

/// Credentials of the source identity: the configured profile, or the
/// default credential chain (env vars, profile, IMDS, ...)
std::shared_ptr<Aws::Auth::AWSCredentialsProvider> make_source_provider(
    const BedrockConfig& config) {
  if (!config.profile.empty()) {
    return std::make_shared<Aws::Auth::ProfileConfigFileAWSCredentialsProvider>(
        config.profile.c_str());
  }
  return std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>();
}

/// Credentials of an IAM role assumed via STS `AssumeRole`, refreshed before
/// they expire. The source credentials come from the configured profile or
/// the default credential chain.
class AssumeRoleCredentialsProvider : public Aws::Auth::AWSCredentialsProvider {
 public:
  AssumeRoleCredentialsProvider(const BedrockConfig& config,
                                const std::string& region)
      : role_arn_(config.role_arn),
        role_session_name_(config.role_session_name),
        external_id_(config.external_id) {
    Aws::STS::STSClientConfiguration sts_config;
    sts_config.region = region;
    sts_client_ = std::make_shared<Aws::STS::STSClient>(
        make_source_provider(config), nullptr, sts_config);

    // Fail at client creation, not at the first request, if the role cannot
    // be assumed
    std::lock_guard<std::mutex> lock(mutex_);
    refresh_locked();
  }

  Aws::Auth::AWSCredentials GetAWSCredentials() override {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Aws::Utils::DateTime::Now();
    const auto refresh_at =
        credentials_.GetExpiration() -
        std::chrono::milliseconds(kCredentialsRefreshMargin);
    if (now >= refresh_at) {
      try {
        refresh_locked();
      } catch (const ConfigurationError& e) {
        // The request will fail with an authentication error; the
        // credentials are not replaced by anything else.
        SecureLogger::log_error(e.what());
        return Aws::Auth::AWSCredentials();
      }
    }
    return credentials_;
  }

 private:
  void refresh_locked() {
    Aws::STS::Model::AssumeRoleRequest request;
    request.SetRoleArn(role_arn_);
    request.SetRoleSessionName(role_session_name_);
    if (!external_id_.empty()) {
      request.SetExternalId(external_id_);
    }

    auto outcome = sts_client_->AssumeRole(request);
    if (!outcome.IsSuccess()) {
      throw ConfigurationError("Failed to assume role: " +
                               outcome.GetError().GetMessage());
    }

    const auto& credentials = outcome.GetResult().GetCredentials();
    credentials_ = Aws::Auth::AWSCredentials(
        credentials.GetAccessKeyId(), credentials.GetSecretAccessKey(),
        credentials.GetSessionToken(), credentials.GetExpiration());
    SecureLogger::log_debug("Assumed role credentials refreshed");
  }

  std::string role_arn_;
  std::string role_session_name_;
  std::string external_id_;
  std::shared_ptr<Aws::STS::STSClient> sts_client_;
  std::mutex mutex_;
  Aws::Auth::AWSCredentials credentials_;
};

std::shared_ptr<Aws::Auth::AWSCredentialsProvider> make_credentials_provider(
    const BedrockConfig& config,
    const std::string& region) {
  // Web identity (EKS / OIDC): the SDK provider re-reads the token file and
  // refreshes the credentials
  if (!config.web_identity_token_file.empty()) {
    if (config.role_arn.empty()) {
      throw ConfigurationError(
          "web_identity_token_file requires role_arn to be set in "
          "BedrockConfig");
    }
    SecureLogger::log_debug("Using STS web identity credentials");
    Aws::Client::ClientConfiguration::CredentialProviderConfiguration
        provider_config;
    provider_config.region = region;
    provider_config.profile = config.profile;
    provider_config.stsCredentialsProviderConfig.roleArn = config.role_arn;
    provider_config.stsCredentialsProviderConfig.sessionName =
        config.role_session_name;
    provider_config.stsCredentialsProviderConfig.tokenFilePath =
        config.web_identity_token_file;
    return std::make_shared<
        Aws::Auth::STSAssumeRoleWebIdentityCredentialsProvider>(
        provider_config);
  }

  // Cross-account access via STS AssumeRole
  if (!config.role_arn.empty()) {
    SecureLogger::log_debug("Using STS AssumeRole credentials");
    return std::make_shared<AssumeRoleCredentialsProvider>(config, region);
  }

  SecureLogger::log_debug(config.profile.empty()
                              ? "Using default AWS credential chain"
                              : "Using AWS profile for credentials");
  return make_source_provider(config);
}

/// Releases an acquired semaphore slot on scope exit
class SemaphoreGuard {
 public:
  explicit SemaphoreGuard(std::counting_semaphore<>& semaphore)
      : semaphore_(semaphore) {
    semaphore_.acquire();
  }
  ~SemaphoreGuard() { semaphore_.release(); }

  SemaphoreGuard(const SemaphoreGuard&) = delete;
  SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;

 private:
  std::counting_semaphore<>& semaphore_;
};

}  // namespace

// PImpl implementation - hides AWS SDK dependencies from public headers
struct BedrockClient::Impl {
  // Shared with streams, which may outlive the client
  std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client;
  std::shared_ptr<std::counting_semaphore<>> request_semaphore;

  BedrockRequestMapper request_mapper;
  BedrockResponseParser response_parser;
  InputValidator input_validator;
  std::string region;
  std::string profile;
  std::string endpoint_override;
  bool valid{false};

  explicit Impl(const BedrockConfig& config)
      : input_validator(config.security) {
    if (config.security.max_concurrent_requests <= 0) {
      throw ConfigurationError("max_concurrent_requests must be positive");
    }
    request_semaphore = std::make_shared<std::counting_semaphore<>>(
        config.security.max_concurrent_requests);

    region = config.region.empty() ? get_region_from_env() : config.region;
    if (region.empty()) {
      throw ConfigurationError(
          "AWS region not configured. Set AWS_REGION environment variable or "
          "provide region in BedrockConfig.");
    }

    profile = config.profile;
    endpoint_override = config.endpoint_override;

    Aws::Client::ClientConfiguration client_config;
    client_config.region = region;
    client_config.connectTimeoutMs =
        static_cast<long>(config.security.connection_timeout.count() * 1000);
    client_config.requestTimeoutMs =
        static_cast<long>(config.security.request_timeout.count() * 1000);
    if (!endpoint_override.empty()) {
      // For VPC endpoints or local testing
      client_config.endpointOverride = endpoint_override;
    }

    client = std::make_shared<Aws::BedrockRuntime::BedrockRuntimeClient>(
        make_credentials_provider(config, region), client_config);

    valid = true;
    SecureLogger::log_debug("Bedrock client initialized - region: " + region);
  }
};

BedrockClient::BedrockClient(const BedrockConfig& config) {
  ensure_aws_sdk_initialized();
  pimpl_ = std::make_unique<Impl>(config);
  SecureLogger::log_info("Bedrock client created for region: " +
                         pimpl_->region);
}

BedrockClient::~BedrockClient() = default;

GenerateResult BedrockClient::generate_text(const GenerateOptions& options) {
  if (options.has_tools() && options.is_multi_step()) {
    return MultiStepCoordinator::execute_multi_step(
        options, [this](const GenerateOptions& step_options) {
          return generate_text_single_step(step_options);
        });
  }
  return generate_text_single_step(options);
}

GenerateResult BedrockClient::generate_text_single_step(
    const GenerateOptions& options) {
  auto start_time = std::chrono::steady_clock::now();
  SecureLogger::log_debug("generate_text called", options.model);

  if (auto validation_error = pimpl_->input_validator.validate(options)) {
    SecureLogger::log_warn("Input validation failed: " + *validation_error);
    GenerateResult result(*validation_error);
    result.finish_reason = kFinishReasonError;
    result.is_retryable = false;
    return result;
  }

  try {
    auto request = pimpl_->request_mapper.map_to_converse_request(options);

    Aws::BedrockRuntime::Model::ConverseOutcome outcome;
    {
      // Limit the number of concurrent requests per client
      SemaphoreGuard guard(*pimpl_->request_semaphore);
      outcome = pimpl_->client->Converse(request);
    }

    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);

    if (!outcome.IsSuccess()) {
      auto result =
          pimpl_->response_parser.parse_converse_error(outcome.GetError());
      SecureLogger::log_api_error("Converse", options.model, pimpl_->region,
                                  outcome.GetError().GetExceptionName(),
                                  outcome.GetError().GetMessage(),
                                  result.is_retryable.value_or(false));
      return result;
    }

    auto result =
        pimpl_->response_parser.parse_converse_response(outcome.GetResult());
    SecureLogger::log_api_call("Converse", options.model, pimpl_->region,
                               outcome.GetResult().GetRequestId(), latency);

    // Execute tools if the model made tool calls
    if (result.has_tool_calls() && options.has_tools()) {
      result.tool_results = ToolExecutor::execute_tools_with_options(
          result.tool_calls, options, false);
    }

    return result;
  } catch (const std::exception& e) {
    SecureLogger::log_error("Bedrock API exception: " + std::string(e.what()));
    GenerateResult result(std::string("Bedrock API error: ") + e.what());
    result.finish_reason = kFinishReasonError;
    result.is_retryable = false;
    return result;
  }
}

EmbeddingResult BedrockClient::embeddings(const EmbeddingOptions& /*options*/) {
  EmbeddingResult result;
  result.error = "Bedrock embeddings are not supported yet";
  return result;
}

StreamResult BedrockClient::stream_text(const StreamOptions& options) {
  SecureLogger::log_debug("stream_text called", options.model);
  const GenerateOptions& generate_options = options;

  auto impl = std::make_unique<BedrockStreamImpl>();

  if (auto validation_error =
          pimpl_->input_validator.validate(generate_options)) {
    SecureLogger::log_warn("Input validation failed: " + *validation_error);
    impl->fail(*validation_error);
    return StreamResult(std::move(impl));
  }

  Aws::BedrockRuntime::Model::ConverseStreamRequest request;
  try {
    request =
        pimpl_->request_mapper.map_to_converse_stream_request(generate_options);
  } catch (const std::exception& e) {
    impl->fail(std::string("Bedrock API error: ") + e.what());
    return StreamResult(std::move(impl));
  }

  // Limit the number of concurrent requests per client; the stream releases
  // the slot when it completes
  pimpl_->request_semaphore->acquire();
  impl->start_stream(pimpl_->client, std::move(request),
                     pimpl_->request_semaphore);

  return StreamResult(std::move(impl));
}

bool BedrockClient::is_valid() const {
  return pimpl_ && pimpl_->valid;
}

std::string BedrockClient::provider_name() const {
  return "bedrock";
}

std::vector<std::string> BedrockClient::supported_models() const {
  return {
      // Claude models on Bedrock
      models::kClaudeFable52,
      models::kClaudeOpus55,
      models::kClaudeSonnet55,
      models::kClaudeHaiku55,
      // Amazon Titan models
      models::kTitanTextExpress,
      models::kTitanTextLite,
      models::kTitanTextPremier,
      // Amazon Nova models
      models::kNovaLite,
      models::kNovaPro,
      models::kNovaMicro,
      // Meta Llama models
      models::kLlama3_8B,
      models::kLlama3_70B,
  };
}

bool BedrockClient::supports_model(const std::string& model_name) const {
  auto models = supported_models();
  return std::find(models.begin(), models.end(), model_name) != models.end();
}

std::string BedrockClient::config_info() const {
  std::string info = "Bedrock API (region: " + pimpl_->region;
  if (!pimpl_->profile.empty()) {
    info += ", profile: " + pimpl_->profile;
  }
  if (!pimpl_->endpoint_override.empty()) {
    info += ", endpoint: " + pimpl_->endpoint_override;
  }
  info += ")";
  return info;
}

std::string BedrockClient::default_model() const {
  return models::kDefaultModel;
}

}  // namespace bedrock
}  // namespace ai
