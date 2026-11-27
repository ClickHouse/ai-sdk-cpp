#pragma once

#include "ai/bedrock.h"

#include <memory>
#include <string>

#include <aws/core/auth/AWSCredentialsProvider.h>

namespace ai {
namespace bedrock {

/// Region from `AWS_REGION`, else `AWS_DEFAULT_REGION`, else empty
std::string get_region_from_env();

/// Credentials provider for a Bedrock configuration: web identity (with
/// `role_arn`), STS AssumeRole (`role_arn`), the configured profile, or the
/// default credential chain. Assumed-role credentials are refreshed before
/// they expire.
/// @throws ConfigurationError if the configuration is invalid or the role
///         cannot be assumed
std::shared_ptr<Aws::Auth::AWSCredentialsProvider> make_credentials_provider(
    const BedrockConfig& config,
    const std::string& region);

}  // namespace bedrock
}  // namespace ai
