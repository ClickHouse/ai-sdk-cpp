#include "bedrock_client.h"

#include "ai/bedrock.h"
#include "ai/errors.h"
#include "ai/tools.h"
#include "aws_sdk_manager.h"
#include "bedrock_credentials.h"
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
#include <aws/core/client/ClientConfiguration.h>

namespace ai {
namespace bedrock {

namespace {

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
