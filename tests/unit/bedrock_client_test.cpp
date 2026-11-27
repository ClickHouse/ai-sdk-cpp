// Bedrock client unit tests (no network access)

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "ai/bedrock.h"
#include "ai/errors.h"

#include <algorithm>
#include <cstdlib>

namespace ai {
namespace bedrock {
namespace {

BedrockConfig offline_config() {
  BedrockConfig config;
  config.region = "us-east-1";
  // Nothing listens there; the tests below do not send requests
  config.endpoint_override = "http://127.0.0.1:1";
  return config;
}

TEST(BedrockClientTest, CreatesClientWithRegion) {
  auto client = create_client(offline_config());

  EXPECT_TRUE(client.is_valid());
  EXPECT_EQ(client.provider_name(), "bedrock");
  EXPECT_EQ(client.default_model(), models::kDefaultModel);
  EXPECT_NE(client.config_info().find("us-east-1"), std::string::npos);
}

TEST(BedrockClientTest, SupportsAnyNonEmptyModelId) {
  auto client = create_client(offline_config());

  EXPECT_TRUE(client.supports_model(models::kClaudeSonnet));
  EXPECT_TRUE(client.supports_model("vendor.some-new-model-v1:0"));
  EXPECT_TRUE(client.supports_model(
      "arn:aws:bedrock:us-east-1:123456789012:inference-profile/"
      "us.anthropic.claude-sonnet-5-5"));
  EXPECT_FALSE(client.supports_model(""));
}

TEST(BedrockClientTest, SupportedModelsContainsCurrentClaudeModels) {
  auto client = create_client(offline_config());
  const auto models = client.supported_models();

  for (const char* model : {models::kClaudeFable, models::kClaudeOpus,
                            models::kClaudeSonnet, models::kClaudeHaiku}) {
    EXPECT_NE(std::find(models.begin(), models.end(), model), models.end())
        << model;
  }
}

TEST(BedrockClientTest, RequiresRegion) {
  BedrockConfig config = offline_config();
  config.region.clear();
  if (std::getenv("AWS_REGION") != nullptr ||
      std::getenv("AWS_DEFAULT_REGION") != nullptr) {
    GTEST_SKIP() << "AWS_REGION or AWS_DEFAULT_REGION is set";
  }

  EXPECT_THROW(create_client(config), ConfigurationError);
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

TEST(BedrockClientTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
