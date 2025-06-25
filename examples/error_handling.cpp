/**
 * Error Handling Example - AI SDK C++
 *
 * This example demonstrates comprehensive error handling in the AI SDK.
 * It shows how to:
 * - Handle different types of errors gracefully
 * - Use exception-safe patterns
 * - Validate inputs before API calls
 * - Recover from common error conditions
 * - Log errors appropriately
 *
 * Usage:
 *   ./error_handling
 */

#include <iostream>
#include <string>

#include <ai/ai.h>

void demonstrate_api_errors() {
  std::cout << "1. API Error Handling\n";
  std::cout << "======================\n\n";

  // Test with invalid API key
  std::cout << "Testing with invalid model name:\n";
  auto client = ai::openai::create_client();
  ai::GenerateOptions options1;
  options1.model = "invalid-model-2024";
  options1.prompt = "Hello, world!";
  auto result1 = client.generate_text(options1);

  if (!result1) {
    std::cout << "Expected error: " << result1.error_message() << "\n\n";
  }

  // Test with empty prompt
  std::cout << "Testing with empty prompt:\n";
  ai::GenerateOptions options2;
  options2.model = ai::openai::models::kGpt4o;
  options2.prompt = "";
  auto result2 = client.generate_text(options2);

  if (!result2) {
    std::cout << "Expected error: " << result2.error_message() << "\n\n";
  }

  // Test with malformed model identifier
  std::cout << "Testing with malformed model identifier:\n";
  ai::GenerateOptions options3;
  options3.model = "";
  options3.prompt = "Hello, world!";
  auto result3 = client.generate_text(options3);

  if (!result3) {
    std::cout << "Expected error: " << result3.error_message() << "\n\n";
  }
}

void demonstrate_validation() {
  std::cout << "2. Input Validation\n";
  std::cout << "==================\n\n";

  // Test GenerateOptions validation
  std::cout << "Testing invalid GenerateOptions:\n";

  ai::GenerateOptions invalid_options;
  // Leave model and prompt empty

  if (!invalid_options.is_valid()) {
    std::cout
        << "Options validation correctly failed (empty model and prompt)\n";
  }

  auto client = ai::openai::create_client();
  auto result = client.generate_text(invalid_options);
  if (!result) {
    std::cout << "API call correctly failed: " << result.error_message()
              << "\n\n";
  }

  // Test valid options
  ai::GenerateOptions valid_options;
  valid_options.model = ai::openai::models::kGpt4o;
  valid_options.prompt = "Hello";

  if (valid_options.is_valid()) {
    std::cout << "Options validation correctly passed\n\n";
  }
}

void demonstrate_network_errors() {
  std::cout << "3. Network Error Simulation\n";
  std::cout << "===========================\n\n";

  // Test with non-existent base URL (this would typically be done with client
  // config)
  std::cout << "Network errors are typically handled internally.\n";
  std::cout << "Common network errors include:\n";
  std::cout << "  - Connection timeouts\n";
  std::cout << "  - DNS resolution failures\n";
  std::cout << "  - SSL/TLS certificate errors\n";
  std::cout << "  - Rate limiting (HTTP 429)\n";
  std::cout << "  - Server errors (HTTP 5xx)\n\n";

  // Demonstrate rate limiting handling
  std::cout << "The SDK automatically handles:\n";
  std::cout << "  - Retries with exponential backoff\n";
  std::cout << "  - Rate limit respect\n";
  std::cout << "  - Connection pooling\n";
  std::cout << "  - Timeout management\n\n";
}

void demonstrate_streaming_errors() {
  std::cout << "4. Streaming Error Handling\n";
  std::cout << "===========================\n\n";

  std::cout << "Testing streaming with invalid model:\n";

  auto client = ai::openai::create_client();
  ai::GenerateOptions gen_options;
  gen_options.model = "invalid-streaming-model";
  gen_options.prompt = "Tell me a story";
  ai::StreamOptions stream_options(std::move(gen_options));
  auto stream = client.stream_text(stream_options);

  // Check for immediate errors
  if (stream.has_error()) {
    std::cout << "Stream error detected: " << stream.error_message() << "\n\n";
  } else {
    // Process stream and handle errors during iteration
    for (const auto& event : stream) {
      if (event.is_error()) {
        std::cout << "Stream event error: " << event.error.value_or("Unknown")
                  << "\n";
        break;
      } else if (event.is_text_delta()) {
        std::cout << event.text_delta;
      } else if (event.is_finish()) {
        std::cout << "\nStream completed successfully\n";
        break;
      }
    }
    std::cout << "\n";
  }
}

void demonstrate_exception_handling() {
  std::cout << "5. Exception Handling\n";
  std::cout << "=====================\n\n";

  try {
    // Most SDK functions use error return values instead of exceptions
    // for better performance and clearer error handling
    std::cout << "The AI SDK uses error return values instead of exceptions\n";
    std::cout << "for most operations. This provides:\n";
    std::cout << "  - Better performance (no exception overhead)\n";
    std::cout << "  - Clearer error handling code\n";
    std::cout << "  - Easier debugging\n";
    std::cout << "  - Predictable control flow\n\n";

    // However, some operations may still throw for programming errors
    std::cout << "Programming errors may still throw exceptions:\n";

    // Example: trying to use moved-from objects, null pointers, etc.
    // These are typically ai::ConfigurationError or std::logic_error

  } catch (const ai::AIError& e) {
    std::cout << "AI SDK Error: " << e.what() << "\n";
  } catch (const std::exception& e) {
    std::cout << "Standard exception: " << e.what() << "\n";
  }
}

void demonstrate_recovery_patterns() {
  std::cout << "6. Error Recovery Patterns\n";
  std::cout << "==========================\n\n";

  // Pattern 1: Fallback models
  std::cout << "Pattern 1 - Fallback models:\n";

  std::vector<std::string> fallback_models = {
      "primary-model-v3",          // This will fail
      ai::openai::models::kGpt4o,  // This should work (if API key is available)
      ai::openai::models::kGpt4oMini  // Faster fallback
  };

  std::string prompt = "What is machine learning?";
  ai::GenerateResult final_result;

  for (const auto& model : fallback_models) {
    std::cout << "Trying model: " << model << "... ";
    auto client = ai::openai::create_client();
    ai::GenerateOptions options;
    options.model = model;
    options.prompt = prompt;
    auto result = client.generate_text(options);

    if (result) {
      std::cout << "Success!\n";
      final_result = std::move(result);
      break;
    } else {
      std::cout << "Failed (" << result.error_message() << ")\n";
    }
  }

  if (final_result) {
    std::cout << "Final result: " << final_result.text.substr(0, 100)
              << "...\n\n";
  } else {
    std::cout << "All fallback models failed\n\n";
  }

  // Pattern 2: Retry with exponential backoff
  std::cout << "Pattern 2 - Retry with backoff:\n";

  int max_retries = 3;
  int retry_count = 0;
  int base_delay_ms = 1000;

  while (retry_count < max_retries) {
    std::cout << "Attempt " << (retry_count + 1) << "/" << max_retries
              << "... ";

    auto client = ai::openai::create_client();
    ai::GenerateOptions options;
    options.model = "unstable-model";
    options.prompt = prompt;
    auto result = client.generate_text(options);

    if (result) {
      std::cout << "Success!\n";
      break;
    } else {
      std::cout << "Failed (" << result.error_message() << ")\n";
      retry_count++;

      if (retry_count < max_retries) {
        int delay = base_delay_ms * (1 << retry_count);  // Exponential backoff
        std::cout << "Waiting " << delay << "ms before retry...\n";
        // std::this_thread::sleep_for(std::chrono::milliseconds(delay));
      }
    }
  }

  if (retry_count >= max_retries) {
    std::cout << "Max retries exceeded\n\n";
  }

  // Pattern 3: Graceful degradation
  std::cout << "Pattern 3 - Graceful degradation:\n";

  auto client = ai::openai::create_client();
  ai::GenerateOptions options;
  options.model = "advanced-model";
  options.prompt = "Explain quantum computing";
  auto result = client.generate_text(options);

  if (result) {
    std::cout << "Full AI response available\n";
  } else {
    std::cout << "AI unavailable, using fallback response:\n";
    std::cout << "Quantum computing is a complex topic that uses quantum "
                 "mechanics...\n";
    std::cout << "(This would be a pre-written or template response)\n";
  }
  std::cout << "\n";
}

void demonstrate_logging() {
  std::cout << "7. Error Logging Best Practices\n";
  std::cout << "===============================\n\n";

  std::cout << "Best practices for error logging in AI applications:\n\n";

  std::cout << "1. Log error details but not sensitive data:\n";
  auto client = ai::openai::create_client();
  ai::GenerateOptions options;
  options.model = "";
  options.prompt = "Secret information";
  auto result = client.generate_text(options);
  if (!result) {
    // Good: Log error type and code
    std::cout << "   ERROR: API call failed - " << result.error_message()
              << "\n";
    // Bad: Don't log API keys, user data, etc.
  }

  std::cout << "\n2. Include context information:\n";
  std::cout << "   ERROR: Text generation failed\n";
  std::cout << "   Context: model=gpt-4o, prompt_length=156, user_id=12345\n";
  std::cout << "   Details: " << result.error_message() << "\n";

  std::cout << "\n3. Use structured logging:\n";
  std::cout << "   "
               "{\"level\":\"error\",\"component\":\"ai-sdk\",\"operation\":"
               "\"generate_text\",";
  std::cout << "\"model\":\"gpt-4o\",\"error\":\"invalid_model\"}\n";

  std::cout << "\n4. Log performance metrics:\n";
  std::cout << "   INFO: Text generation completed in 1250ms, 45 tokens used\n";

  std::cout << "\n5. Don't log every retry, but log final failures:\n";
  std::cout << "   ERROR: Text generation failed after 3 retries\n\n";
}

int main() {
  std::cout << "AI SDK C++ - Error Handling Examples\n";
  std::cout << "=====================================\n\n";

  demonstrate_api_errors();
  demonstrate_validation();
  demonstrate_network_errors();
  demonstrate_streaming_errors();
  demonstrate_exception_handling();
  demonstrate_recovery_patterns();
  demonstrate_logging();

  std::cout << "Error handling examples completed!\n\n";

  std::cout << "Key takeaways:\n";
  std::cout
      << "  1. Always check result.is_success() before using result.text\n";
  std::cout << "  2. Use fallback models for better reliability\n";
  std::cout << "  3. Implement retry logic with exponential backoff\n";
  std::cout << "  4. Validate inputs before making API calls\n";
  std::cout
      << "  5. Log errors appropriately without exposing sensitive data\n";
  std::cout << "  6. Design graceful degradation for critical applications\n";
  std::cout << "  7. Handle streaming errors during iteration\n";
  std::cout
      << "  8. Use the SDK's built-in error types for specific handling\n";

  return 0;
}