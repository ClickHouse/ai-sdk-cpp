// Bedrock Integration Tests
// Tests actual API calls to AWS Bedrock
//
// These tests only run when:
// - AI_SDK_IT_BEDROCK=1 environment variable is set
// - Valid AWS credentials are available
// - AWS_REGION is configured

#include <gtest/gtest.h>

#ifdef AI_SDK_HAS_BEDROCK

#include "ai/bedrock.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <cstdlib>
#include <iostream>

namespace ai {
namespace bedrock {
namespace {

class BedrockIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip if integration tests not enabled
    if (std::getenv("AI_SDK_IT_BEDROCK") == nullptr) {
      GTEST_SKIP()
          << "Bedrock integration tests disabled. Set AI_SDK_IT_BEDROCK=1 to "
             "enable.";
    }

    // Try to create client - skip if credentials unavailable
    auto maybe_client = try_create_client();
    if (!maybe_client.has_value()) {
      GTEST_SKIP()
          << "AWS credentials not available for Bedrock integration tests.";
    }
    client_ = std::move(maybe_client.value());
  }

  Client client_;
};

// Test basic text generation
TEST_F(BedrockIntegrationTest, GenerateTextReturnsValidResponse) {
  GenerateOptions options;
  // Use Amazon Nova which supports on-demand throughput
  options.model = "amazon.nova-lite-v1:0";
  options.prompt = "Say 'Hello, World!' and nothing else.";
  options.max_tokens = 50;

  auto result = client_.generate_text(options);

  EXPECT_FALSE(result.error.has_value()) << "Error: " << result.error.value_or("");
  EXPECT_FALSE(result.text.empty());
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
}

// Test generation with system prompt
TEST_F(BedrockIntegrationTest, GenerateTextWithSystemPrompt) {
  GenerateOptions options;
  // Use Amazon Nova which supports on-demand throughput
  options.model = "amazon.nova-lite-v1:0";
  options.system = "You are a helpful assistant that responds in exactly one word.";
  options.prompt = "What color is the sky?";
  options.max_tokens = 20;

  auto result = client_.generate_text(options);

  EXPECT_FALSE(result.error.has_value()) << "Error: " << result.error.value_or("");
  EXPECT_FALSE(result.text.empty());
}

// Test generation with multi-turn conversation
TEST_F(BedrockIntegrationTest, GenerateTextWithMessages) {
  GenerateOptions options;
  // Use Amazon Nova which supports on-demand throughput
  options.model = "amazon.nova-lite-v1:0";
  options.messages = {
      Message::user("My name is Alice."),
      Message::assistant("Nice to meet you, Alice!"),
      Message::user("What is my name?"),
  };
  options.max_tokens = 50;

  auto result = client_.generate_text(options);

  EXPECT_FALSE(result.error.has_value()) << "Error: " << result.error.value_or("");
  EXPECT_FALSE(result.text.empty());
  // Response should mention "Alice"
  EXPECT_NE(result.text.find("Alice"), std::string::npos);
}

// Test streaming response
TEST_F(BedrockIntegrationTest, StreamTextYieldsIncrementalOutput) {
  StreamOptions options;
  // Use Amazon Nova which supports on-demand throughput
  options.model = "amazon.nova-lite-v1:0";
  options.prompt = "Count from 1 to 5, one number per line.";
  options.max_tokens = 50;

  auto stream = client_.stream_text(options);

  std::string full_text;
  int chunk_count = 0;
  bool got_finish = false;

  // Use iterator interface for streaming
  for (const auto& event : stream) {
    if (event.type == kStreamEventTypeTextDelta) {
      full_text += event.text_delta;
      chunk_count++;
    } else if (event.type == kStreamEventTypeFinish) {
      got_finish = true;
      break;
    } else if (event.type == kStreamEventTypeError) {
      FAIL() << "Stream error: " << event.error.value_or("unknown error");
    }
  }

  EXPECT_GT(chunk_count, 0) << "Should receive multiple text chunks";
  EXPECT_FALSE(full_text.empty()) << "Should receive text content";
  EXPECT_TRUE(got_finish) << "Should receive finish event";
}

// Test provider name
TEST_F(BedrockIntegrationTest, ProviderNameIsBedrock) {
  EXPECT_EQ(client_.provider_name(), "bedrock");
}

// Test default model
TEST_F(BedrockIntegrationTest, DefaultModelIsSet) {
  EXPECT_FALSE(client_.default_model().empty());
  EXPECT_EQ(client_.default_model(), models::kDefaultModel);
}

// Test config info
TEST_F(BedrockIntegrationTest, ConfigInfoContainsRegion) {
  auto info = client_.config_info();
  EXPECT_NE(info.find("region"), std::string::npos);
}

// Test input validation - invalid temperature
TEST_F(BedrockIntegrationTest, RejectsInvalidTemperature) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.temperature = 1.5;  // Invalid - must be 0.0-1.0

  auto result = client_.generate_text(options);

  EXPECT_TRUE(result.error.has_value());
  EXPECT_NE(result.error->find("Temperature"), std::string::npos);
}

// Test input validation - invalid top_p
TEST_F(BedrockIntegrationTest, RejectsInvalidTopP) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.top_p = -0.5;  // Invalid - must be 0.0-1.0

  auto result = client_.generate_text(options);

  EXPECT_TRUE(result.error.has_value());
  EXPECT_NE(result.error->find("Top_p"), std::string::npos);
}

// Test input validation - invalid max_tokens
TEST_F(BedrockIntegrationTest, RejectsInvalidMaxTokens) {
  GenerateOptions options;
  options.model = models::kClaudeSonnet;
  options.prompt = "Hello";
  options.max_tokens = 0;  // Invalid - must be positive

  auto result = client_.generate_text(options);

  EXPECT_TRUE(result.error.has_value());
  EXPECT_NE(result.error->find("Max tokens"), std::string::npos);
}

// ============================================================================
// Model Discovery Tests
// ============================================================================

class BedrockModelDiscoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (std::getenv("AI_SDK_IT_BEDROCK") == nullptr) {
      GTEST_SKIP()
          << "Bedrock integration tests disabled. Set AI_SDK_IT_BEDROCK=1 to "
             "enable.";
    }
  }
};

// Test list_available_models returns results
TEST_F(BedrockModelDiscoveryTest, ListAvailableModelsReturnsResults) {
  try {
    auto models = list_available_models();
    
    // Should return at least some models
    EXPECT_GT(models.size(), 0) << "Should have at least one available model";
    
    // Print available models for debugging
    std::cout << "Available models (" << models.size() << "):" << std::endl;
    for (const auto& model : models) {
      std::cout << "  - " << model.model_id << " (" << model.provider << ")"
                << " streaming=" << model.supports_streaming << std::endl;
    }
    
    // Check model info structure
    for (const auto& model : models) {
      EXPECT_FALSE(model.model_id.empty());
      EXPECT_FALSE(model.provider.empty());
    }
  } catch (const std::exception& e) {
    FAIL() << "list_available_models threw exception: " << e.what();
  }
}

// Test validate_model_access for known models
TEST_F(BedrockModelDiscoveryTest, ValidateModelAccessForKnownModels) {
  // Test with models that should be available in most accounts
  // Note: Claude models require inference profiles for on-demand access
  // Amazon Nova models support direct on-demand access
  bool has_nova_lite = validate_model_access(models::kNovaLite);
  bool has_nova_micro = validate_model_access(models::kNovaMicro);
  bool has_titan = validate_model_access(models::kTitanTextExpress);
  
  // At least one of these should be accessible
  EXPECT_TRUE(has_nova_lite || has_nova_micro || has_titan) 
      << "At least one common model should be accessible";
}

// Test validate_model_access returns false for invalid model
TEST_F(BedrockModelDiscoveryTest, ValidateModelAccessReturnsFalseForInvalid) {
  bool result = validate_model_access("invalid.nonexistent-model-v1:0");
  EXPECT_FALSE(result);
}

// ============================================================================
// Multi-Model Tests
// ============================================================================

class BedrockMultiModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (std::getenv("AI_SDK_IT_BEDROCK") == nullptr) {
      GTEST_SKIP()
          << "Bedrock integration tests disabled. Set AI_SDK_IT_BEDROCK=1 to "
             "enable.";
    }

    auto maybe_client = try_create_client();
    if (!maybe_client.has_value()) {
      GTEST_SKIP()
          << "AWS credentials not available for Bedrock integration tests.";
    }
    client_ = std::move(maybe_client.value());
  }

  Client client_;
};

// Test generation with different models
TEST_F(BedrockMultiModelTest, GenerateWithDifferentModels) {
  // Get available models
  std::vector<std::string> models_to_test;
  
  try {
    auto available = list_available_models();
    for (const auto& model : available) {
      // Only test text models that support streaming
      if (model.supports_streaming && 
          std::find(model.capabilities.begin(), model.capabilities.end(), "text") 
              != model.capabilities.end()) {
        models_to_test.push_back(model.model_id);
        if (models_to_test.size() >= 3) break;  // Test up to 3 models
      }
    }
  } catch (const std::exception& e) {
    // Fall back to hardcoded models if discovery fails
    models_to_test = {models::kClaudeSonnet};
  }

  if (models_to_test.empty()) {
    GTEST_SKIP() << "No suitable models available for testing";
  }

  for (const auto& model_id : models_to_test) {
    std::cout << "Testing model: " << model_id << std::endl;
    
    GenerateOptions options;
    options.model = model_id;
    options.prompt = "Say 'test' and nothing else.";
    options.max_tokens = 20;

    auto result = client_.generate_text(options);

    // Some models may not be enabled - that's OK
    if (result.error.has_value()) {
      std::cout << "  Model " << model_id << " error: " << result.error.value() 
                << std::endl;
      continue;
    }

    EXPECT_FALSE(result.text.empty()) 
        << "Model " << model_id << " should return text";
    std::cout << "  Model " << model_id << " response: " << result.text 
              << std::endl;
  }
}

// Test using custom model ID (ARN format)
TEST_F(BedrockMultiModelTest, SupportsCustomModelArn) {
  // This test verifies that ARN-format model IDs pass validation
  // The actual API call may fail if the model doesn't exist
  
  GenerateOptions options;
  // Use a valid ARN format (will fail at API level, but should pass validation)
  options.model = "arn:aws:bedrock:us-east-1:123456789012:custom-model/test";
  options.prompt = "Hello";
  options.max_tokens = 10;

  auto result = client_.generate_text(options);

  // Should not fail on validation - may fail on API call
  // The error should be from AWS, not from input validation
  if (result.error.has_value()) {
    // Should be an AWS error, not a validation error
    EXPECT_EQ(result.error->find("Invalid model ID format"), std::string::npos)
        << "ARN format should pass validation";
  }
}

// Test using inference profile for Claude models
TEST_F(BedrockMultiModelTest, SupportsInferenceProfile) {
  GenerateOptions options;
  // Use Claude Sonnet 4.5 inference profile (cross-region)
  options.model = "us.anthropic.claude-sonnet-4-5-20250929-v1:0";
  options.prompt = "Say 'Hello from Claude!' and nothing else.";
  options.max_tokens = 50;

  std::cout << "Testing inference profile: " << options.model << std::endl;
  
  auto result = client_.generate_text(options);

  // Should succeed with inference profile
  EXPECT_FALSE(result.error.has_value()) 
      << "Error: " << result.error.value_or("");
  EXPECT_FALSE(result.text.empty()) << "Should return text";
  
  std::cout << "  Response: " << result.text << std::endl;
  std::cout << "  Tokens - prompt: " << result.usage.prompt_tokens 
            << ", completion: " << result.usage.completion_tokens << std::endl;
}

// Test streaming with inference profile
TEST_F(BedrockMultiModelTest, StreamingWithInferenceProfile) {
  StreamOptions options;
  // Use Claude Sonnet 4.5 inference profile (cross-region)
  options.model = "us.anthropic.claude-sonnet-4-5-20250929-v1:0";
  options.prompt = "Count from 1 to 3.";
  options.max_tokens = 50;

  std::cout << "Testing streaming with inference profile: " << options.model << std::endl;
  
  auto stream = client_.stream_text(options);

  std::string full_text;
  int chunk_count = 0;
  bool got_finish = false;

  for (const auto& event : stream) {
    if (event.type == kStreamEventTypeTextDelta) {
      full_text += event.text_delta;
      chunk_count++;
    } else if (event.type == kStreamEventTypeFinish) {
      got_finish = true;
      break;
    } else if (event.type == kStreamEventTypeError) {
      FAIL() << "Stream error: " << event.error.value_or("unknown error");
    }
  }

  EXPECT_GT(chunk_count, 0) << "Should receive multiple text chunks";
  EXPECT_FALSE(full_text.empty()) << "Should receive text content";
  EXPECT_TRUE(got_finish) << "Should receive finish event";
  
  std::cout << "  Received " << chunk_count << " chunks" << std::endl;
  std::cout << "  Full response: " << full_text << std::endl;
}

}  // namespace
}  // namespace bedrock
}  // namespace ai

#else  // AI_SDK_HAS_BEDROCK

// Placeholder test when Bedrock is not enabled
TEST(BedrockIntegrationTest, BedrockNotEnabled) {
  GTEST_SKIP() << "Bedrock provider not enabled. Set AI_SDK_ENABLE_BEDROCK=ON";
}

#endif  // AI_SDK_HAS_BEDROCK
