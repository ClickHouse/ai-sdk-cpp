/**
 * Retry Configuration Example - AI SDK C++
 *
 * This example demonstrates how to configure custom retry behavior for API
 * calls. It shows how to:
 * - Create clients with custom retry configuration
 * - Set maximum retry attempts
 * - Configure initial delay and backoff factor
 * - Handle transient failures gracefully
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   ./retry_config_example
 */

#include <chrono>
#include <iostream>
#include <string>

#include <ai/openai.h>
#include <ai/retry/retry_policy.h>

int main() {
  std::cout << "AI SDK C++ - Retry Configuration Example\n";
  std::cout << "========================================\n\n";

  // Example 1: Default retry configuration
  std::cout << "1. Client with default retry configuration:\n";
  std::cout << "   - Max retries: 2\n";
  std::cout << "   - Initial delay: 2000ms\n";
  std::cout << "   - Backoff factor: 2.0\n\n";

  auto default_client = ai::openai::create_client();

  ai::GenerateOptions options;
  options.model = ai::openai::models::kGpt4oMini;
  options.prompt = "Say 'Hello with default retry config!'";

  auto result1 = default_client.generate_text(options);
  if (result1) {
    std::cout << "Response: " << result1.text << "\n\n";
  } else {
    std::cout << "Error: " << result1.error_message() << "\n\n";
  }

  // Example 2: Aggressive retry configuration for unreliable networks
  std::cout << "2. Client with aggressive retry configuration:\n";
  std::cout << "   - Max retries: 5 (for unreliable networks)\n";
  std::cout << "   - Initial delay: 1000ms (faster initial retry)\n";
  std::cout << "   - Backoff factor: 1.5 (gentler backoff)\n\n";

  ai::retry::RetryConfig aggressive_config;
  aggressive_config.max_retries = 5;
  aggressive_config.initial_delay = std::chrono::milliseconds(1000);
  aggressive_config.backoff_factor = 1.5;

  // Get API key from environment
  const char* api_key = std::getenv("OPENAI_API_KEY");
  if (!api_key) {
    std::cerr << "Error: OPENAI_API_KEY environment variable not set\n";
    return 1;
  }

  auto aggressive_client = ai::openai::create_client(
      api_key, "https://api.openai.com", aggressive_config);

  options.prompt = "Say 'Hello with aggressive retry config!'";

  auto result2 = aggressive_client.generate_text(options);
  if (result2) {
    std::cout << "Response: " << result2.text << "\n\n";
  } else {
    std::cout << "Error: " << result2.error_message() << "\n\n";
  }

  // Example 3: Conservative retry configuration for rate-limited APIs
  std::cout << "3. Client with conservative retry configuration:\n";
  std::cout << "   - Max retries: 3\n";
  std::cout
      << "   - Initial delay: 5000ms (longer wait to avoid rate limits)\n";
  std::cout << "   - Backoff factor: 3.0 (aggressive backoff)\n\n";

  ai::retry::RetryConfig conservative_config;
  conservative_config.max_retries = 3;
  conservative_config.initial_delay = std::chrono::milliseconds(5000);
  conservative_config.backoff_factor = 3.0;

  auto conservative_client = ai::openai::create_client(
      api_key, "https://api.openai.com", conservative_config);

  options.prompt = "Say 'Hello with conservative retry config!'";

  auto result3 = conservative_client.generate_text(options);
  if (result3) {
    std::cout << "Response: " << result3.text << "\n\n";
  } else {
    std::cout << "Error: " << result3.error_message() << "\n\n";
  }

  // Example 4: No retry configuration (for debugging)
  std::cout << "4. Client with no retries (fail fast for debugging):\n";
  std::cout << "   - Max retries: 0\n";
  std::cout << "   - Initial delay: N/A\n";
  std::cout << "   - Backoff factor: N/A\n\n";

  ai::retry::RetryConfig no_retry_config;
  no_retry_config.max_retries = 0;

  auto no_retry_client = ai::openai::create_client(
      api_key, "https://api.openai.com", no_retry_config);

  options.prompt = "Say 'Hello with no retry config!'";

  auto result4 = no_retry_client.generate_text(options);
  if (result4) {
    std::cout << "Response: " << result4.text << "\n\n";
  } else {
    std::cout << "Error (no retries): " << result4.error_message() << "\n\n";
  }

  // Example 5: Custom retry for specific use case
  std::cout << "5. Custom retry configuration for batch processing:\n";
  std::cout << "   This configuration is optimized for batch jobs that can\n";
  std::cout << "   tolerate longer delays but need high reliability.\n";
  std::cout << "   - Max retries: 10\n";
  std::cout << "   - Initial delay: 3000ms\n";
  std::cout << "   - Backoff factor: 2.0\n";
  std::cout << "   - Max total retry time: ~102 seconds\n\n";

  ai::retry::RetryConfig batch_config;
  batch_config.max_retries = 10;
  batch_config.initial_delay = std::chrono::milliseconds(3000);
  batch_config.backoff_factor = 2.0;

  auto batch_client = ai::openai::create_client(
      api_key, "https://api.openai.com", batch_config);

  options.prompt =
      "Generate a list of 5 creative project names for a batch processing "
      "system.";
  options.model = ai::openai::models::kGpt4o;

  std::cout << "Processing batch request (this might take a while if retries "
               "occur)...\n";

  auto result5 = batch_client.generate_text(options);
  if (result5) {
    std::cout << "Batch result:\n" << result5.text << "\n\n";
  } else {
    std::cout << "Batch error: " << result5.error_message() << "\n\n";
  }

  std::cout << "Example completed!\n\n";

  std::cout << "Notes on retry behavior:\n";
  std::cout << "- Retries occur for: 408, 409, 429, and 5xx status codes\n";
  std::cout << "- Network errors are also retried\n";
  std::cout << "- Delays between retries: initial_delay * (backoff_factor ^ "
               "attempt)\n";
  std::cout << "- Total attempts = 1 initial + max_retries\n";

  return 0;
}