#include <iostream>

#include <ai/ai.h>

int main() {
  std::cout << "AI SDK C++ - Google Component Demo\n";
  std::cout << "=================================\n\n";

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

#ifdef AI_SDK_HAS_GOOGLE
  std::cout << "Testing Google functionality...\n";
  try {
    auto client = ai::google::create_client();
    std::cout << "✓ Google client created successfully\n";
    std::cout << "✓ Default model: " << client.default_model() << "\n";
  } catch (const std::exception& e) {
    std::cout << "✗ Google client failed: " << e.what() << "\n";
  }
#else
  std::cout << "Google functionality not available\n";
#endif

  std::cout << "\nThis example only links the ai::google component\n";
  return 0;
}
