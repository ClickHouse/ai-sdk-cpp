#include <iostream>

#include <ai/ai.h>

int main() {
  std::cout << "AI SDK C++ - Anthropic Component Demo\n";
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

  std::cout << "\n";

  // Test core functionality
  std::cout << "Testing core functionality...\n";
  ai::GenerateOptions options;
  options.model = "claude-sonnet-4-5-20250929";
  options.prompt = "Hello world";
  std::cout << "✓ Core types work fine\n\n";

  // Test Anthropic functionality
#ifdef AI_SDK_HAS_ANTHROPIC
  std::cout << "Testing Anthropic functionality...\n";
  try {
    auto client = ai::anthropic::create_client();
    std::cout << "✓ Anthropic client created successfully\n";
    std::cout << "✓ Available models: "
              << ai::anthropic::models::kClaudeSonnet45 << ", "
              << ai::anthropic::models::kClaudeHaiku45 << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ Anthropic client failed: " << e.what() << "\n";
  }
#else
  std::cout << "Anthropic functionality not available\n";
#endif

  // Demonstrate that OpenAI is NOT available
#ifdef AI_SDK_HAS_OPENAI
  std::cout << "\nTesting OpenAI functionality...\n";
  // This would work if OpenAI was linked
  auto openai_client = ai::openai::create_client();
  std::cout << "✓ OpenAI client created\n";
#else
  std::cout
      << "\nOpenAI functionality intentionally not available in this build\n";
  std::cout << "This example only links ai::anthropic component\n";

  // Uncommenting the next line would cause a compile error:
  // auto openai_client = ai::openai::create_client(); // ERROR!
#endif

  return 0;
}