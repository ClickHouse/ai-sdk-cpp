#include <iostream>

#include <ai/ai.h>
#include <ai/bedrock.h>

int main() {
  std::cout << "AI SDK C++ - Bedrock Component Demo\n";
  std::cout << "====================================\n\n";

  std::cout << "Available components:\n";
  std::cout << "- Core: YES\n";

#ifdef AI_SDK_HAS_OPENAI
  std::cout << "- OpenAI: YES\n";
#else
  std::cout << "- OpenAI: NO\n";
#endif

#ifdef AI_SDK_HAS_ANTHROPIC
  std::cout << "- Anthropic: YES\n";
#else
  std::cout << "- Anthropic: NO\n";
#endif

#ifdef AI_SDK_HAS_BEDROCK
  std::cout << "- Bedrock: YES\n";
#else
  std::cout << "- Bedrock: NO\n";
#endif

  std::cout << "\n";

  // Test core functionality
  std::cout << "Testing core functionality...\n";
  ai::GenerateOptions options;
  options.model = "anthropic.claude-sonnet-4-20250514-v1:0";
  options.prompt = "Hello world";
  std::cout << "✓ Core types work fine\n\n";

  // Test Bedrock functionality
#ifdef AI_SDK_HAS_BEDROCK
  std::cout << "Testing Bedrock functionality...\n";
  try {
    auto maybe_client = ai::bedrock::try_create_client();
    if (maybe_client.has_value()) {
      std::cout << "✓ Bedrock client created successfully\n";
      std::cout << "✓ Provider: " << maybe_client->provider_name() << "\n";
      std::cout << "✓ Default model: " << maybe_client->default_model() << "\n";
      std::cout << "✓ Available models: "
                << ai::bedrock::models::kClaudeSonnet << ", "
                << ai::bedrock::models::kClaudeHaiku << ", "
                << ai::bedrock::models::kTitanTextExpress << "\n";
    } else {
      std::cout << "✗ Bedrock client creation failed (check AWS credentials)\n";
    }
  } catch (const std::exception& e) {
    std::cout << "✗ Bedrock client failed: " << e.what() << "\n";
  }
#else
  std::cout << "Bedrock functionality not available\n";
#endif

  // Demonstrate that OpenAI is NOT available
#ifdef AI_SDK_HAS_OPENAI
  std::cout << "\nTesting OpenAI functionality...\n";
  auto openai_client = ai::openai::create_client();
  std::cout << "✓ OpenAI client created\n";
#else
  std::cout
      << "\nOpenAI functionality intentionally not available in this build\n";
  std::cout << "This example only links ai::bedrock component\n";
#endif

  return 0;
}
