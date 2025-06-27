#include <ai/ai.h>
#include <iostream>

int main() {
    std::cout << "AI SDK C++ - OpenAI Component Demo\n";
    std::cout << "==================================\n\n";
    
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
        auto client = ai::openai::create_client();
        std::cout << "✓ OpenAI client created successfully\n";
        std::cout << "✓ Available models: " << ai::openai::models::kGpt4o << ", " 
                  << ai::openai::models::kGpt4oMini << "\n";
    } catch (const std::exception& e) {
        std::cout << "✗ OpenAI client failed: " << e.what() << "\n";
    }
#else
    std::cout << "OpenAI functionality not available\n";
#endif
    
    // Demonstrate that Anthropic is NOT available
#ifdef AI_SDK_HAS_ANTHROPIC
    std::cout << "\nTesting Anthropic functionality...\n";
    // This would work if Anthropic was linked
    auto anthropic_client = ai::anthropic::create_client();
    std::cout << "✓ Anthropic client created\n";
#else
    std::cout << "\nAnthropie functionality intentionally not available in this build\n";
    std::cout << "This example only links ai::openai component\n";
    
    // Uncommenting the next line would cause a compile error:
    // auto anthropic_client = ai::anthropic::create_client(); // ERROR!
#endif
    
    return 0;
}