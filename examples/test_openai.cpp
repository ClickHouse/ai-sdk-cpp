#include <iostream>

#include <ai/ai.h>

int main() {
  try {
    // Create OpenAI client
    auto client = ai::openai::create_client();

    // Test simple generation
    std::cout << "Testing OpenAI text generation...\n\n";

    ai::GenerateOptions options(ai::openai::models::kGpt4oMini,
                                "You are a friendly assistant!",
                                "Why is the sky blue? Give a short answer.");

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

    ai::GenerateOptions stream_opts(ai::openai::models::kGpt4oMini,
                                    "Count from 1 to 5 slowly");
    ai::StreamOptions stream_options(stream_opts);

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
        }
      }
    }

  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}