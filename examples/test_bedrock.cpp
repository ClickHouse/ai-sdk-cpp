#include "ai/logger.h"

#include <iostream>

#include <ai/ai.h>

#ifdef AI_SDK_HAS_BEDROCK
#include <ai/bedrock.h>
#endif

int main() {
#ifdef AI_SDK_HAS_BEDROCK
  try {
    // Enable debug logging
    ai::logger::install_logger(std::make_shared<ai::logger::ConsoleLogger>(
        ai::logger::LogLevel::kLogLevelInfo));

    // Create Bedrock client
    auto maybe_client = ai::bedrock::try_create_client();
    if (!maybe_client.has_value()) {
      std::cerr << "Failed to create Bedrock client. Check AWS credentials.\n";
      return 1;
    }
    auto& client = maybe_client.value();

    std::cout << "Bedrock client created successfully\n";
    std::cout << "Provider: " << client.provider_name() << "\n";
    std::cout << "Default model: " << client.default_model() << "\n\n";

    // Test simple generation
    std::cout << "Testing Bedrock text generation...\n\n";

    ai::GenerateOptions options;
    options.model = ai::bedrock::models::kClaudeSonnet;
    options.system = "You are a helpful assistant.";
    options.prompt = "Why is the sky blue? Give a short answer.";
    options.max_tokens = 200;

    auto result = client.generate_text(options);

    if (result) {
      std::cout << "Response: " << result.text << "\n";
      std::cout << "Model: " << result.model.value_or("unknown") << "\n";
      std::cout << "Tokens used: " << result.usage.total_tokens << "\n";
      std::cout << "Finish reason: " << result.finishReasonToString() << "\n";
    } else {
      std::cout << "Error: " << result.error_message() << "\n";
    }

    // Test streaming
    std::cout << "\nTesting streaming...\n";

    ai::StreamOptions stream_options;
    stream_options.model = ai::bedrock::models::kClaudeSonnet;
    stream_options.prompt = "Count from 1 to 5 slowly and with each number say 'tick'";
    stream_options.max_tokens = 100;

    auto stream = client.stream_text(stream_options);

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
        } else {
          std::cout << "Note: Token usage data may not be available in "
                       "streaming mode.\n";
        }
      }
    }

  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }
#else
  std::cout << "Bedrock support not enabled in this build.\n";
  std::cout << "Build with -DAI_SDK_ENABLE_BEDROCK=ON to enable.\n";
#endif

  return 0;
}
