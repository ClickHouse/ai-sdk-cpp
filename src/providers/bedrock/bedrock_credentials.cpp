#include "bedrock_credentials.h"

#include "ai/errors.h"
#include "secure_logger.h"

#include <chrono>
#include <cstdlib>
#include <mutex>

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
        SecureLogger::log_error(e.what());
        // Keep using the cached credentials until they actually expire (the
        // next call retries the refresh); after that, the request fails with
        // an authentication error instead of using any other identity.
        if (now >= credentials_.GetExpiration()) {
          return Aws::Auth::AWSCredentials();
        }
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

}  // namespace

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

}  // namespace bedrock
}  // namespace ai
