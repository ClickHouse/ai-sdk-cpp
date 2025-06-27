#include <iostream>

#include <ai/ai.h>

int main() {
  std::cout << "AI SDK C++ - All Components Demo\n";
  std::cout << "================================\n\n";

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

  std::cout << "\n";

  // Test core functionality
  std::cout << "Testing core functionality...\n";
  ai::GenerateOptions options;
  options.model = "gpt-4o";
  options.prompt = "Hello world";
  std::cout << "✓ Core types work fine\n\n";

  // Test OpenAI functionality
#ifdef AI_SDK_HAS_OPENAI
  std::cout << "Testing OpenAI functionality...\n";
  try {
    auto openai_client = ai::openai::create_client();
    std::cout << "✓ OpenAI client created successfully\n";
    std::cout << "✓ Available models: " << ai::openai::models::kGpt4o << ", "
              << ai::openai::models::kGpt4oMini << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ OpenAI client failed: " << e.what() << "\n";
  }
#else
  std::cout << "OpenAI functionality not available\n";
#endif

  // Test Anthropic functionality
#ifdef AI_SDK_HAS_ANTHROPIC
  std::cout << "\nTesting Anthropic functionality...\n";
  try {
    auto anthropic_client = ai::anthropic::create_client();
    std::cout << "✓ Anthropic client created successfully\n";
    std::cout << "✓ Available models: "
              << ai::anthropic::models::kClaude35Sonnet << ", "
              << ai::anthropic::models::kClaude35Haiku << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ Anthropic client failed: " << e.what() << "\n";
  }
#else
  std::cout << "\nAnthropic functionality not available\n";
#endif

  std::cout << "\n";
  std::cout << "This example demonstrates using all components together.\n";
  std::cout
      << "Both OpenAI and Anthropic should be available when linking ai::sdk\n";

  return 0;
}