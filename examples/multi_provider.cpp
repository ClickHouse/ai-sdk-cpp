/**
 * Multi-Provider Example - AI SDK C++
 *
 * This example demonstrates using multiple AI providers with the same API.
 * It shows how to:
 * - Compare responses from different providers
 * - Use provider-specific models
 * - Handle provider-specific errors
 * - Benchmark response times and token usage
 *
 * Usage:
 *   export OPENAI_API_KEY=your_openai_key
 *   export ANTHROPIC_API_KEY=your_anthropic_key
 *   ./multi_provider
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <ai/ai.h>

struct ProviderResult {
  std::string provider_name;
  std::string model_name;
  ai::GenerateResult result;
  std::chrono::milliseconds response_time;
  bool success;
};

ProviderResult test_provider(const std::string& provider_name,
                             const std::string& model,
                             const std::string& prompt) {
  auto start = std::chrono::high_resolution_clock::now();

  ai::Client client;
  if (provider_name == "OpenAI") {
    client = ai::openai::create_client();
  } else if (provider_name == "Anthropic") {
    client = ai::anthropic::create_client();
  } else {
    return {provider_name, model, ai::GenerateResult("Unknown provider"),
            std::chrono::milliseconds(0), false};
  }

  ai::GenerateOptions options;
  options.model = model;
  options.prompt = prompt;
  auto result = client.generate_text(options);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  return {provider_name, model, std::move(result), duration,
          result.is_success()};
}

void print_result(const ProviderResult& pr) {
  std::cout << "Provider: " << pr.provider_name << "\n";
  std::cout << "Model: " << pr.model_name << "\n";

  if (pr.success) {
    std::cout << "Response time: " << pr.response_time.count() << "ms\n";
    std::cout << "Tokens used: " << pr.result.usage.total_tokens << "\n";
    std::cout << "Finish reason: " << pr.result.finishReasonToString() << "\n";
    std::cout << "Response: " << pr.result.text << "\n";
  } else {
    std::cout << "Error: " << pr.result.error_message() << "\n";
  }
  std::cout << std::string(60, '-') << "\n\n";
}

int main() {
  std::cout << "AI SDK C++ - Multi-Provider Comparison\n";
  std::cout << "======================================\n\n";

  // Test 1: Simple question comparison
  std::cout << "Test 1: Simple factual question\n";
  std::cout << "Question: What is the largest planet in our solar system?\n\n";

  std::string simple_question =
      "What is the largest planet in our solar system? Give a brief answer.";

  std::vector<ProviderResult> results1;

  // Test OpenAI models
  results1.push_back(
      test_provider("OpenAI", ai::openai::models::kGpt4o, simple_question));
  results1.push_back(
      test_provider("OpenAI", ai::openai::models::kGpt4oMini, simple_question));

  // Test Anthropic models
  results1.push_back(test_provider(
      "Anthropic", ai::anthropic::models::kClaude35Sonnet, simple_question));
  results1.push_back(test_provider(
      "Anthropic", ai::anthropic::models::kClaude35Haiku, simple_question));

  for (const auto& result : results1) {
    print_result(result);
  }

  // Test 2: Creative writing comparison
  std::cout << "Test 2: Creative writing task\n";
  std::cout
      << "Prompt: Write a creative short story opening about time travel.\n\n";

  std::string creative_prompt =
      "Write a creative opening paragraph for a short story about someone who "
      "discovers they can time travel, but only to the exact same location 24 "
      "hours in the past.";

  std::vector<ProviderResult> results2;

  // Test with different providers for creativity
  results2.push_back(
      test_provider("OpenAI", ai::openai::models::kGpt4o, creative_prompt));
  results2.push_back(test_provider(
      "Anthropic", ai::anthropic::models::kClaude35Sonnet, creative_prompt));

  for (const auto& result : results2) {
    print_result(result);
  }

  // Test 3: Technical explanation comparison
  std::cout << "Test 3: Technical explanation\n";
  std::cout << "Prompt: Explain the concept of RAII in C++.\n\n";

  std::string technical_prompt =
      "Explain the concept of RAII (Resource Acquisition Is Initialization) in "
      "C++ with a simple example.";

  std::vector<ProviderResult> results3;

  results3.push_back(
      test_provider("OpenAI", ai::openai::models::kGpt4o, technical_prompt));
  results3.push_back(test_provider(
      "Anthropic", ai::anthropic::models::kClaude35Sonnet, technical_prompt));

  for (const auto& result : results3) {
    print_result(result);
  }

  // Test 4: Performance and cost analysis
  std::cout << "Test 4: Performance and cost analysis\n\n";

  struct ModelStats {
    std::string name;
    int successful_calls = 0;
    int total_calls = 0;
    int total_tokens = 0;
    std::chrono::milliseconds total_time{0};
  };

  std::vector<ModelStats> stats;

  // Collect stats from all tests
  for (const auto& test_results : {results1, results2, results3}) {
    for (const auto& result : test_results) {
      auto it = std::find_if(
          stats.begin(), stats.end(),
          [&](const ModelStats& s) { return s.name == result.model_name; });

      if (it == stats.end()) {
        stats.push_back(
            {result.model_name, 0, 0, 0, std::chrono::milliseconds{0}});
        it = stats.end() - 1;
      }

      it->total_calls++;
      it->total_time += result.response_time;

      if (result.success) {
        it->successful_calls++;
        it->total_tokens += result.result.usage.total_tokens;
      }
    }
  }

  std::cout << std::left << std::setw(25) << "Model" << std::setw(12)
            << "Success Rate" << std::setw(15) << "Avg Response"
            << std::setw(12) << "Avg Tokens" << "\n";
  std::cout << std::string(64, '=') << "\n";

  for (const auto& stat : stats) {
    double success_rate = stat.total_calls > 0 ? (double)stat.successful_calls /
                                                     stat.total_calls * 100
                                               : 0;
    double avg_time = stat.total_calls > 0
                          ? (double)stat.total_time.count() / stat.total_calls
                          : 0;
    double avg_tokens = stat.successful_calls > 0
                            ? (double)stat.total_tokens / stat.successful_calls
                            : 0;

    std::cout << std::left << std::setw(25) << stat.name << std::setw(12)
              << (std::to_string((int)success_rate) + "%") << std::setw(15)
              << (std::to_string((int)avg_time) + "ms") << std::setw(12)
              << std::to_string((int)avg_tokens) << "\n";
  }

  std::cout << "\n";

  // Test 5: Provider-specific client usage
  std::cout << "Test 5: Using provider-specific clients\n\n";

  // Create clients explicitly
  try {
    auto openai_client = ai::openai::create_client();
    auto anthropic_client = ai::anthropic::create_client();

    if (openai_client.is_valid()) {
      std::cout << "OpenAI client created successfully\n";
      std::cout << "Provider: " << openai_client.provider_name() << "\n";

      auto openai_models = openai_client.supported_models();
      if (!openai_models.empty()) {
        std::cout << "Supported models: ";
        for (size_t i = 0; i < openai_models.size() && i < 3; ++i) {
          std::cout << openai_models[i];
          if (i < 2 && i < openai_models.size() - 1) {
            std::cout << ", ";
          }
        }
        std::cout << "...\n";
      }
    } else {
      std::cout << "Failed to create OpenAI client (check API key)\n";
    }

    if (anthropic_client.is_valid()) {
      std::cout << "Anthropic client created successfully\n";
      std::cout << "Provider: " << anthropic_client.provider_name() << "\n";
    } else {
      std::cout << "Failed to create Anthropic client (check API key)\n";
    }

  } catch (const ai::AIError& e) {
    std::cout << "Client creation error: " << e.what() << "\n";
  }

  std::cout << "\nMulti-provider comparison completed!\n";
  std::cout << "\nKey insights:\n";
  std::cout << "  - Different models excel at different tasks\n";
  std::cout << "  - Response times vary by model size and complexity\n";
  std::cout << "  - Token usage affects cost - consider model efficiency\n";
  std::cout << "  - The AI SDK provides a unified interface across providers\n";
  std::cout << "\nTip: Choose models based on your specific use case:\n";
  std::cout << "  - Fast responses: GPT-4o-mini, Claude-3-5-haiku\n";
  std::cout << "  - High quality: GPT-4o, Claude-3-5-sonnet\n";
  std::cout << "  - Creative tasks: Models with higher temperature settings\n";

  return 0;
}