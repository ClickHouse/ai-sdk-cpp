#include "bedrock_client.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "aws_sdk_manager.h"
#include "bedrock_request_mapper.h"
#include "bedrock_response_parser.h"
#include "bedrock_stream.h"
#include "circuit_breaker.h"
#include "input_validator.h"
#include "secure_logger.h"

#include <aws/bedrock-runtime/BedrockRuntimeClient.h>
#include <aws/bedrock-runtime/model/ConverseRequest.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <semaphore>

namespace ai {
namespace bedrock {

namespace {

// Helper to get region from environment
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

// Read web identity token from file
std::string read_web_identity_token(const std::string& token_file) {
  std::ifstream file(token_file);
  if (!file.is_open()) {
    throw ConfigurationError("Failed to open web identity token file: " +
                             token_file);
  }
  std::string token((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
  return token;
}

}  // namespace

// PImpl implementation - hides AWS SDK dependencies from public headers
struct BedrockClient::Impl {
  std::shared_ptr<Aws::BedrockRuntime::BedrockRuntimeClient> client;
  BedrockRequestMapper request_mapper;
  BedrockResponseParser response_parser;
  InputValidator input_validator;
  CircuitBreaker circuit_breaker;
  std::string region;
  std::string profile;
  std::string endpoint_override;
  SecurityConfig security_config;
  bool valid{false};

  // Concurrency control - limits parallel requests to prevent overload
  std::unique_ptr<std::counting_semaphore<>> request_semaphore;

  Impl(const BedrockConfig& config)
      : input_validator(config.security),
        circuit_breaker(config.security),
        security_config(config.security) {
    // Note: AWS SDK initialization is handled by BedrockClient constructor
    // via AwsSdkManager::acquire()

    // Initialize concurrency limiter
    request_semaphore = std::make_unique<std::counting_semaphore<>>(
        config.security.max_concurrent_requests);

    // Resolve region from config or environment
    region = config.region;
    if (region.empty()) {
      region = get_region_from_env();
    }

    if (region.empty()) {
      throw ConfigurationError(
          "AWS region not configured. Set AWS_REGION environment variable or "
          "provide region in BedrockConfig.");
    }

    profile = config.profile;
    endpoint_override = config.endpoint_override;

    // Configure AWS client
    Aws::Client::ClientConfiguration client_config;
    client_config.region = region;

    // Set timeouts from security config
    client_config.connectTimeoutMs =
        static_cast<long>(config.security.connection_timeout.count() * 1000);
    client_config.requestTimeoutMs =
        static_cast<long>(config.security.request_timeout.count() * 1000);

    // Set endpoint override if provided (for VPC endpoints or local testing)
    if (!endpoint_override.empty()) {
      client_config.endpointOverride = endpoint_override;
    }

    // Create credentials provider based on configuration
    auto credentials_provider = create_credentials_provider(config);

    // Create Bedrock runtime client
    client = std::make_shared<Aws::BedrockRuntime::BedrockRuntimeClient>(
        credentials_provider, client_config);

    valid = true;
    SecureLogger::log_debug("Bedrock client initialized - region: " + region);
  }

  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> create_credentials_provider(
      const BedrockConfig& config) {
    // Priority 1: STS AssumeRole for cross-account access
    if (!config.role_arn.empty()) {
      return create_assume_role_provider(config);
    }

    // Priority 2: Web Identity Token for EKS/OIDC
    if (!config.web_identity_token_file.empty()) {
      return create_web_identity_provider(config);
    }

    // Priority 3: Profile-based credentials
    if (!config.profile.empty()) {
      SecureLogger::log_debug("Using AWS profile for credentials");
      return std::make_shared<Aws::Auth::ProfileConfigFileAWSCredentialsProvider>(
          config.profile.c_str());
    }

    // Priority 4: Default credential chain (env vars, instance profile, etc.)
    SecureLogger::log_debug("Using default AWS credential chain");
    return std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>();
  }

  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> create_assume_role_provider(
      const BedrockConfig& config) {
    SecureLogger::log_debug("Using STS AssumeRole for credentials");

    // Create STS client with default credentials
    Aws::Client::ClientConfiguration sts_config;
    sts_config.region = region;

    auto sts_client = std::make_shared<Aws::STS::STSClient>(sts_config);

    Aws::STS::Model::AssumeRoleRequest assume_request;
    assume_request.SetRoleArn(config.role_arn);
    assume_request.SetRoleSessionName(config.role_session_name);

    if (!config.external_id.empty()) {
      assume_request.SetExternalId(config.external_id);
    }

    auto outcome = sts_client->AssumeRole(assume_request);

    if (!outcome.IsSuccess()) {
      throw ConfigurationError("Failed to assume role: " +
                               outcome.GetError().GetMessage());
    }

    const auto& credentials = outcome.GetResult().GetCredentials();

    // Create a simple credentials provider with the assumed role credentials
    return std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
        credentials.GetAccessKeyId(), credentials.GetSecretAccessKey(),
        credentials.GetSessionToken());
  }

  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> create_web_identity_provider(
      const BedrockConfig& config) {
    SecureLogger::log_debug("Using Web Identity Token for credentials");

    // Read the token from file
    std::string token = read_web_identity_token(config.web_identity_token_file);

    // Create STS client
    Aws::Client::ClientConfiguration sts_config;
    sts_config.region = region;

    auto sts_client = std::make_shared<Aws::STS::STSClient>(sts_config);

    Aws::STS::Model::AssumeRoleWithWebIdentityRequest request;
    request.SetRoleArn(config.role_arn);
    request.SetRoleSessionName(config.role_session_name);
    request.SetWebIdentityToken(token);

    auto outcome = sts_client->AssumeRoleWithWebIdentity(request);

    if (!outcome.IsSuccess()) {
      throw ConfigurationError("Failed to assume role with web identity: " +
                               outcome.GetError().GetMessage());
    }

    const auto& credentials = outcome.GetResult().GetCredentials();

    return std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
        credentials.GetAccessKeyId(), credentials.GetSecretAccessKey(),
        credentials.GetSessionToken());
  }
};

BedrockClient::BedrockClient(const BedrockConfig& config) {
  // Acquire SDK reference first - ensures SDK is initialized before creating Impl
  AwsSdkManager::instance().acquire();
  
  // Now create the implementation (which uses AWS SDK)
  pimpl_ = std::make_unique<Impl>(config);
  
  SecureLogger::log_info("Bedrock client created for region: " +
                         pimpl_->region);
}

BedrockClient::~BedrockClient() {
  // Destroy pimpl first (releases AWS resources)
  pimpl_.reset();
  
  // Then release SDK reference - SDK will shutdown when last client is destroyed
  AwsSdkManager::instance().release();
}

// RAII guard for semaphore to prevent deadlocks
class SemaphoreGuard {
 public:
  explicit SemaphoreGuard(std::counting_semaphore<>& sem) : sem_(sem) {
    sem_.acquire();
  }
  ~SemaphoreGuard() { sem_.release(); }

  // Non-copyable, non-movable
  SemaphoreGuard(const SemaphoreGuard&) = delete;
  SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;

 private:
  std::counting_semaphore<>& sem_;
};

GenerateResult BedrockClient::generate_text(const GenerateOptions& options) {
  auto start_time = std::chrono::steady_clock::now();

  SecureLogger::log_debug("generate_text called", options.model);

  // Validate input parameters
  auto validation_error = pimpl_->input_validator.validate(options);
  if (validation_error.has_value()) {
    SecureLogger::log_warn("Input validation failed: " +
                           validation_error.value());
    GenerateResult result;
    result.finish_reason = kFinishReasonError;
    result.error = validation_error.value();
    result.is_retryable = false;
    return result;
  }

  // Check circuit breaker state
  if (pimpl_->circuit_breaker.is_open()) {
    SecureLogger::log_warn("Circuit breaker is open, rejecting request");
    GenerateResult result;
    result.finish_reason = kFinishReasonError;
    result.error = "Service temporarily unavailable (circuit breaker open)";
    result.is_retryable = true;
    return result;
  }

  // Acquire semaphore for concurrency control
  // Using RAII guard to ensure release even if exceptions occur
  SemaphoreGuard guard(*pimpl_->request_semaphore);

  try {
    // Build Converse request
    auto request = pimpl_->request_mapper.map_to_converse_request(options);

    // Call AWS Bedrock Converse API
    auto outcome = pimpl_->client->Converse(request);

    // Calculate latency
    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    GenerateResult result;

    if (outcome.IsSuccess()) {
      result = pimpl_->response_parser.parse_converse_response(
          outcome.GetResult());
      pimpl_->circuit_breaker.record_success();

      // Log successful API call
      std::string request_id = outcome.GetResult().GetRequestId();
      SecureLogger::log_api_call("Converse", options.model, pimpl_->region,
                                 request_id, latency);
    } else {
      result = pimpl_->response_parser.parse_converse_error(outcome.GetError());
      pimpl_->circuit_breaker.record_failure();

      // Log API error
      SecureLogger::log_api_error(
          "Converse", options.model, pimpl_->region,
          outcome.GetError().GetExceptionName(),
          outcome.GetError().GetMessage(), result.is_retryable.value_or(false));
    }

    return result;

  } catch (const std::exception& e) {
    pimpl_->circuit_breaker.record_failure();

    SecureLogger::log_error("Bedrock API exception: " + std::string(e.what()));

    GenerateResult result;
    result.finish_reason = kFinishReasonError;
    result.error = std::string("Bedrock API error: ") + e.what();
    result.is_retryable = false;
    return result;
  }
}

EmbeddingResult BedrockClient::embeddings(const EmbeddingOptions& options) {
  // Embeddings not yet supported
  SecureLogger::log_debug("embeddings called - not supported in V1");

  EmbeddingResult result;
  result.error = "Bedrock embeddings are not supported in V1";
  return result;
}

StreamResult BedrockClient::stream_text(const StreamOptions& options) {
  SecureLogger::log_debug("stream_text called", options.model);

  // Build GenerateOptions for validation
  GenerateOptions gen_options;
  gen_options.model = options.model;
  gen_options.prompt = options.prompt;
  gen_options.system = options.system;
  gen_options.messages = options.messages;
  gen_options.max_tokens = options.max_tokens;
  gen_options.temperature = options.temperature;
  gen_options.top_p = options.top_p;

  // Validate input parameters
  auto validation_error = pimpl_->input_validator.validate(gen_options);
  if (validation_error.has_value()) {
    SecureLogger::log_warn("Input validation failed: " +
                           validation_error.value());
    // Return a stream that immediately yields an error
    auto impl = std::make_unique<BedrockStreamImpl>();
    // The stream will be in error state
    return StreamResult(std::move(impl));
  }

  // Check circuit breaker state
  if (pimpl_->circuit_breaker.is_open()) {
    SecureLogger::log_warn("Circuit breaker is open, rejecting stream request");
    auto impl = std::make_unique<BedrockStreamImpl>();
    return StreamResult(std::move(impl));
  }

  // Acquire semaphore for concurrency control
  // Note: For streaming, we acquire here but the stream impl is responsible
  // for releasing when the stream completes or is stopped
  pimpl_->request_semaphore->acquire();

  // Create stream implementation with semaphore reference for cleanup
  auto impl = std::make_unique<BedrockStreamImpl>();

  auto request =
      pimpl_->request_mapper.map_to_converse_stream_request(gen_options);

  // Start streaming - pass semaphore for release on completion
  impl->start_stream(pimpl_->client, std::move(request),
                     pimpl_->request_semaphore.get());

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
      // Claude models on Bedrock (inference profiles)
      models::kClaudeSonnet,
      models::kClaudeHaiku,
      models::kClaudeOpus,
      models::kClaudeSonnet4,
      models::kClaudeSonnet45,
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
