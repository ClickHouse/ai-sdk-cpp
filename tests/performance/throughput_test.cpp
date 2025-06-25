#include "../utils/mock_openai_client.h"
#include "../utils/test_fixtures.h"
#include "ai/types/generate_options.h"

#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace ai {
namespace test {

class ThroughputTest : public OpenAITestFixture {
 protected:
  void SetUp() override {
    OpenAITestFixture::SetUp();
    client_ = std::make_unique<ControllableOpenAIClient>(kTestApiKey);
    client_->setPredefinedResponse(
        ResponseBuilder::buildSuccessResponse("Performance test response"));
  }

  void TearDown() override {
    client_.reset();
    OpenAITestFixture::TearDown();
  }

  std::unique_ptr<ControllableOpenAIClient> client_;
};

// Sequential Request Performance
TEST_F(ThroughputTest, SequentialRequestThroughput) {
  constexpr int num_requests = 100;
  constexpr int max_duration_ms = 5000;  // 5 seconds max

  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < num_requests; ++i) {
    GenerateOptions options("gpt-4o-mini", "Request " + std::to_string(i));
    auto result = client_->generate_text(options);

    TestAssertions::assertSuccess(result);
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_EQ(client_->getCallCount(), num_requests);
  EXPECT_LT(duration.count(), max_duration_ms)
      << "Sequential requests took too long";

  double requests_per_second =
      static_cast<double>(num_requests) / (duration.count() / 1000.0);
  std::cout << "Sequential throughput: " << requests_per_second
            << " requests/second\n";

  // Should achieve reasonable throughput (mock client should be fast)
  EXPECT_GT(requests_per_second, 50.0) << "Sequential throughput too low";
}

// Concurrent Request Performance
TEST_F(ThroughputTest, ConcurrentRequestThroughput) {
  constexpr int num_threads = 10;
  constexpr int requests_per_thread = 10;
  constexpr int total_requests = num_threads * requests_per_thread;
  constexpr int max_duration_ms = 3000;  // 3 seconds max

  auto start_time = std::chrono::steady_clock::now();

  std::vector<std::future<void>> futures;

  for (int t = 0; t < num_threads; ++t) {
    futures.emplace_back(
        std::async(std::launch::async, [this, t, requests_per_thread]() {
          for (int i = 0; i < requests_per_thread; ++i) {
            GenerateOptions options(
                "gpt-4o-mini", "Thread " + std::to_string(t) + " Request " +
                                   std::to_string(i));
            auto result = client_->generate_text(options);
            TestAssertions::assertSuccess(result);
          }
        }));
  }

  // Wait for all threads to complete
  for (auto& future : futures) {
    future.wait();
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_EQ(client_->getCallCount(), total_requests);
  EXPECT_LT(duration.count(), max_duration_ms)
      << "Concurrent requests took too long";

  double requests_per_second =
      static_cast<double>(total_requests) / (duration.count() / 1000.0);
  std::cout << "Concurrent throughput: " << requests_per_second
            << " requests/second\n";

  // Concurrent requests should be significantly faster than sequential
  EXPECT_GT(requests_per_second, 100.0) << "Concurrent throughput too low";
}

// Large Prompt Performance
TEST_F(ThroughputTest, LargePromptPerformance) {
  std::vector<size_t> prompt_sizes = {1000, 10000, 50000,
                                      100000};  // 1KB to 100KB

  for (size_t size : prompt_sizes) {
    auto large_prompt = TestDataGenerator::createLargePrompt(size);

    auto start_time = std::chrono::steady_clock::now();

    GenerateOptions options("gpt-4o", large_prompt);
    auto result = client_->generate_text(options);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    TestAssertions::assertSuccess(result);

    std::cout << "Prompt size: " << size
              << " bytes, Duration: " << duration.count() << "ms\n";

    // Even large prompts should be processed quickly with mock client
    EXPECT_LT(duration.count(), 1000)
        << "Large prompt (" << size << " bytes) took too long";
  }
}

// Response Size Performance
TEST_F(ThroughputTest, LargeResponsePerformance) {
  std::vector<size_t> response_sizes = {1000, 10000,
                                        50000};  // 1KB to 50KB responses

  for (size_t size : response_sizes) {
    std::string large_response = TestDataGenerator::createLargePrompt(size);
    client_->setPredefinedResponse(
        ResponseBuilder::buildSuccessResponse(large_response));

    auto start_time = std::chrono::steady_clock::now();

    GenerateOptions options("gpt-4o", "Generate large response");
    auto result = client_->generate_text(options);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    TestAssertions::assertSuccess(result);
    EXPECT_GE(result.text.length(),
              size * 0.9);  // Should be roughly the expected size

    std::cout << "Response size: " << result.text.length()
              << " bytes, Duration: " << duration.count() << "ms\n";

    EXPECT_LT(duration.count(), 2000)
        << "Large response (" << size << " bytes) took too long";
  }
}

// Stress Testing
TEST_F(ThroughputTest, StressTestManyRequests) {
  constexpr int num_requests = 1000;
  constexpr int max_duration_ms = 30000;  // 30 seconds max

  auto start_time = std::chrono::steady_clock::now();

  int successful_requests = 0;
  int failed_requests = 0;

  for (int i = 0; i < num_requests; ++i) {
    GenerateOptions options("gpt-4o-mini", "Stress test " + std::to_string(i));
    auto result = client_->generate_text(options);

    if (result.is_success()) {
      successful_requests++;
    } else {
      failed_requests++;
    }

    // Brief pause to avoid overwhelming
    if (i % 100 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_EQ(successful_requests, num_requests)
      << "Some requests failed in stress test";
  EXPECT_EQ(failed_requests, 0);
  EXPECT_LT(duration.count(), max_duration_ms) << "Stress test took too long";

  double requests_per_second =
      static_cast<double>(num_requests) / (duration.count() / 1000.0);
  std::cout << "Stress test throughput: " << requests_per_second
            << " requests/second\n";

  EXPECT_GT(requests_per_second, 30.0) << "Stress test throughput too low";
}

// Memory Usage Patterns
TEST_F(ThroughputTest, MemoryUsagePattern) {
  constexpr int num_iterations = 50;
  constexpr int requests_per_iteration = 20;

  // Simulate creating many clients and making requests
  for (int iter = 0; iter < num_iterations; ++iter) {
    std::vector<std::unique_ptr<ControllableOpenAIClient>> clients;

    // Create multiple clients
    for (int c = 0; c < 5; ++c) {
      auto client = std::make_unique<ControllableOpenAIClient>(kTestApiKey);
      client->setPredefinedResponse(ResponseBuilder::buildSuccessResponse(
          "Memory test response " + std::to_string(c)));
      clients.push_back(std::move(client));
    }

    // Make requests with each client
    for (auto& client : clients) {
      for (int r = 0; r < requests_per_iteration; ++r) {
        GenerateOptions options("gpt-4o-mini",
                                "Memory iteration " + std::to_string(iter) +
                                    " request " + std::to_string(r));
        auto result = client->generate_text(options);
        TestAssertions::assertSuccess(result);
      }
    }

    // Clients will be destroyed at end of iteration
    // This tests memory cleanup patterns
  }

  // If we get here without crashes, memory management is working
  SUCCEED();
}

// Latency Distribution
TEST_F(ThroughputTest, LatencyDistribution) {
  constexpr int num_requests = 200;
  std::vector<double> latencies;
  latencies.reserve(num_requests);

  for (int i = 0; i < num_requests; ++i) {
    auto start_time = std::chrono::high_resolution_clock::now();

    GenerateOptions options("gpt-4o-mini", "Latency test " + std::to_string(i));
    auto result = client_->generate_text(options);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration<double, std::milli>(end_time - start_time);

    TestAssertions::assertSuccess(result);
    latencies.push_back(duration.count());
  }

  // Calculate statistics
  std::sort(latencies.begin(), latencies.end());

  double min_latency = latencies.front();
  double max_latency = latencies.back();
  double median_latency = latencies[latencies.size() / 2];
  double p95_latency = latencies[static_cast<size_t>(latencies.size() * 0.95)];
  double p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];

  double sum = 0.0;
  for (double latency : latencies) {
    sum += latency;
  }
  double mean_latency = sum / latencies.size();

  std::cout << "Latency statistics (ms):\n";
  std::cout << "  Min: " << min_latency << "\n";
  std::cout << "  Mean: " << mean_latency << "\n";
  std::cout << "  Median: " << median_latency << "\n";
  std::cout << "  P95: " << p95_latency << "\n";
  std::cout << "  P99: " << p99_latency << "\n";
  std::cout << "  Max: " << max_latency << "\n";

  // With mock client, latencies should be very low
  EXPECT_LT(mean_latency, 50.0) << "Mean latency too high";
  EXPECT_LT(p95_latency, 100.0) << "P95 latency too high";
  EXPECT_LT(max_latency, 500.0) << "Max latency too high";
}

}  // namespace test
}  // namespace ai