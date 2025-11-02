#include <cstdlib>
#include <iostream>
#include <string>

#include <ai/ai.h>
#include <ai/logger.h>

int main() {
  try {
    // Enable info logging
    ai::logger::install_logger(std::make_shared<ai::logger::ConsoleLogger>(
        ai::logger::LogLevel::kLogLevelInfo));

    // Get OpenRouter API key from environment variable
    const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
    if (!api_key_env) {
      std::cerr << "Error: OPENROUTER_API_KEY environment variable not set\n";
      std::cerr << "Please set your OpenRouter API key:\n";
      std::cerr << "  export OPENROUTER_API_KEY=your-api-key-here\n";
      return 1;
    }
    std::string api_key(api_key_env);

    // Create OpenAI client with OpenRouter's base URL
    // OpenRouter provides an OpenAI-compatible API endpoint
    auto client =
        ai::openai::create_client(api_key, "https://openrouter.ai/api");

    std::cout << "=== OpenRouter Example (OpenAI-compatible) ===\n\n";

    // Test simple generation with a model available on OpenRouter
    std::cout << "Testing text generation with OpenRouter...\n\n";

    // Using a model that's available on OpenRouter
    // Common models: "openai/gpt-4o", "anthropic/claude-sonnet-4-5",
    // "meta-llama/llama-3.1-8b-instruct" See https://openrouter.ai/models for
    // available models
    ai::GenerateOptions options(
        "anthropic/claude-sonnet-4-5", "You are a helpful assistant.",
        "What are the benefits of using OpenRouter for AI applications? Give a "
        "brief answer.");

    auto result = client.generate_text(options);

    if (result) {
      std::cout << "Response: " << result.text << "\n";
      std::cout << "Model: " << result.model.value_or("unknown") << "\n";
      std::cout << "Tokens used: " << result.usage.total_tokens << "\n";
      std::cout << "Finish reason: " << result.finishReasonToString() << "\n";
    } else {
      std::cout << "Error: " << result.error_message() << "\n";
      return 1;
    }

    // Test streaming with OpenRouter
    std::cout << "\n\nTesting streaming with OpenRouter...\n";

    ai::GenerateOptions stream_opts("anthropic/claude-sonnet-4-5",
                                    "You are a creative writer.",
                                    "Write a haiku about API compatibility.");
    ai::StreamOptions stream_options(stream_opts);

    auto stream = client.stream_text(stream_options);

    std::cout << "Haiku:\n";
    for (const auto& event : stream) {
      if (event.is_text_delta()) {
        std::cout << event.text_delta << std::flush;
      } else if (event.is_error()) {
        std::cout << "\nStream error: " << event.error.value_or("unknown")
                  << "\n";
      } else if (event.is_finish()) {
        std::cout << "\n\nStream finished.\n";
        if (event.usage.has_value()) {
          std::cout << "Total tokens: " << event.usage->total_tokens << "\n";
        }
      }
    }

    std::cout << "\n=== Example completed successfully ===\n";
    std::cout << "\nNote: You can use any model available on OpenRouter.\n";
    std::cout << "Visit https://openrouter.ai/models for the full list.\n";

  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}