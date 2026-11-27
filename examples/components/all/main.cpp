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

#ifdef AI_SDK_HAS_GOOGLE
  std::cout << "- Google: YES\n";
#else
  std::cout << "- Google: NO\n";
#endif

  std::cout << "\n";

  // Test core functionality
  std::cout << "Testing core functionality...\n";
  ai::GenerateOptions options;
  options.model = ai::openai::models::kGpt56;
  options.prompt = "Hello world";
  std::cout << "✓ Core types work fine\n\n";

  // Test OpenAI functionality
#ifdef AI_SDK_HAS_OPENAI
  std::cout << "Testing OpenAI functionality...\n";
  try {
    auto openai_client = ai::openai::create_client();
    std::cout << "✓ OpenAI client created successfully\n";
    std::cout << "✓ Available models: " << ai::openai::models::kGpt56 << ", "
              << ai::openai::models::kGpt56Terra << "\n";
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
              << ai::anthropic::models::kClaudeSonnet55 << ", "
              << ai::anthropic::models::kClaudeHaiku55 << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ Anthropic client failed: " << e.what() << "\n";
  }
#else
  std::cout << "\nAnthropic functionality not available\n";
#endif

  // Test Google functionality
#ifdef AI_SDK_HAS_GOOGLE
  std::cout << "\nTesting Google functionality...\n";
  try {
    auto google_client = ai::google::create_client();
    std::cout << "✓ Google client created successfully\n";
    std::cout << "✓ Available models: " << ai::google::models::kGemini4Hydrogen
              << ", " << ai::google::models::kGemini35FlashLite << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ Google client failed: " << e.what() << "\n";
  }
#else
  std::cout << "\nGoogle functionality not available\n";
#endif

  std::cout << "\n";
  std::cout << "This example demonstrates using all components together.\n";
  std::cout << "OpenAI, Anthropic, and Google should be available when linking "
               "ai::sdk\n";

  return 0;
}
