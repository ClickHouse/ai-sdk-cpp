#include "../utils/test_fixtures.h"
#include "ai/anthropic.h"
#include "ai/logger.h"
#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/tool.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

// Test for issue #26: Tools executed twice in multi-step mode
// https://github.com/ClickHouse/ai-sdk-cpp/issues/26

// Parameterized test class for multi-step tool execution
class MultiStepDuplicateExecutionTest
    : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() override {
    std::string provider = GetParam();

    if (provider == "openai") {
      const char* api_key = std::getenv("OPENAI_API_KEY");
      if (api_key) {
        use_real_api_ = true;
        client_ = ai::openai::create_client(api_key);
        model_ = ai::openai::models::kGpt4oMini;
      } else {
        use_real_api_ = false;
      }
    } else if (provider == "anthropic") {
      const char* api_key = std::getenv("ANTHROPIC_API_KEY");
      if (api_key) {
        use_real_api_ = true;
        client_ = ai::anthropic::create_client(api_key);
        model_ = ai::anthropic::models::kClaudeSonnet35;
      } else {
        use_real_api_ = false;
      }
    }
  }

  bool use_real_api_ = false;
  std::optional<ai::Client> client_;
  std::string model_;
};

// Test that tools are executed only once per step in multi-step mode
TEST_P(MultiStepDuplicateExecutionTest, ToolsExecutedOncePerStepNotTwice) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // Create a shared counter to track tool executions
  auto execution_count = std::make_shared<std::atomic<int>>(0);

  // Create a tool that increments the counter each time it's called
  Tool counter_tool = create_simple_tool(
      "get_counter", "Returns the current count", {{"message", "string"}},
      [execution_count](const JsonValue& args,
                        const ToolExecutionContext& context) {
        int count = execution_count->fetch_add(1) + 1;
        std::string message = args["message"].get<std::string>();
        ai::logger::log_info("Counter tool executed! Count: {}, Message: {}",
                             count, message);
        return JsonValue{{"count", count}, {"message", message}};
      });

  ToolSet tools = {{"get_counter", counter_tool}};

  // Configure options with multi-step mode enabled (max_steps > 1)
  GenerateOptions options(model_,
                          "Please use the get_counter tool with message "
                          "'test' to get the current count.");
  options.tools = tools;
  options.max_steps = 2;  // Enable multi-step mode
  options.max_tokens = 300;

  // Reset counter before test
  execution_count->store(0);

  // Execute the request
  auto result = client_->generate_text(options);

  // Verify result is successful
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();

  // Verify the tool was called
  EXPECT_TRUE(result.has_tool_calls()) << "Expected tool calls to be made";
  EXPECT_GT(result.tool_calls.size(), 0) << "Expected at least one tool call";

  // Verify tool results exist
  EXPECT_TRUE(result.has_tool_results())
      << "Expected tool results to be present";

  // Log the execution count
  int final_count = execution_count->load();
  ai::logger::log_info(
      "Final execution count: {} (expected 1 per tool call, got {})",
      final_count, result.tool_calls.size());

  // CRITICAL ASSERTION: Each tool call should be executed exactly once
  // If this fails, it means tools are being executed multiple times
  EXPECT_EQ(final_count, static_cast<int>(result.tool_calls.size()))
      << "Tool execution count (" << final_count
      << ") does not match tool call count (" << result.tool_calls.size()
      << "). This indicates duplicate execution! "
      << "See https://github.com/ClickHouse/ai-sdk-cpp/issues/26";

  // Additional verification: check the tool results
  for (const auto& tool_result : result.tool_results) {
    EXPECT_TRUE(tool_result.is_success())
        << "Tool execution failed: " << tool_result.error_message();

    if (tool_result.is_success() && tool_result.result.contains("count")) {
      ai::logger::log_info("Tool result count: {}",
                           tool_result.result["count"].get<int>());
    }
  }
}

// Test with multiple steps to verify the issue persists across steps
TEST_P(MultiStepDuplicateExecutionTest, MultipleStepsNoDuplicateExecution) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // Create a shared counter to track tool executions
  auto execution_count = std::make_shared<std::atomic<int>>(0);

  // Create a calculator tool that also tracks executions
  Tool tracked_calculator = create_simple_tool(
      "add", "Add two numbers together", {{"a", "number"}, {"b", "number"}},
      [execution_count](const JsonValue& args,
                        const ToolExecutionContext& context) {
        int exec_num = execution_count->fetch_add(1) + 1;
        double a = args["a"].get<double>();
        double b = args["b"].get<double>();
        double result = a + b;

        ai::logger::log_info(
            "Calculator executed (execution #{}): {} + {} = {}", exec_num, a, b,
            result);

        return JsonValue{{"result", result},
                         {"execution_number", exec_num},
                         {"a", a},
                         {"b", b}};
      });

  ToolSet tools = {{"add", tracked_calculator}};

  // Configure options with higher max_steps
  GenerateOptions options(model_,
                          "Calculate 5 + 3 using the add tool. "
                          "Then tell me the result.");
  options.tools = tools;
  options.max_steps = 3;  // Allow multiple steps
  options.max_tokens = 500;

  // Reset counter before test
  execution_count->store(0);

  // Execute the request
  auto result = client_->generate_text(options);

  // Verify result is successful
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();

  // Verify the tool was called
  EXPECT_TRUE(result.has_tool_calls()) << "Expected tool calls to be made";

  // Log step information
  ai::logger::log_info("Total steps executed: {}", result.steps.size());
  ai::logger::log_info("Total tool calls: {}", result.tool_calls.size());

  int final_count = execution_count->load();
  ai::logger::log_info("Total tool executions: {}", final_count);

  // CRITICAL ASSERTION: Total executions should equal total tool calls
  EXPECT_EQ(final_count, static_cast<int>(result.tool_calls.size()))
      << "Tool execution count (" << final_count
      << ") does not match tool call count (" << result.tool_calls.size()
      << "). This indicates duplicate execution across steps! "
      << "See https://github.com/ClickHouse/ai-sdk-cpp/issues/26";

  // Verify each step's tool results show unique execution numbers
  for (size_t i = 0; i < result.steps.size(); ++i) {
    const auto& step = result.steps[i];
    if (!step.tool_results.empty()) {
      ai::logger::log_info("Step {} had {} tool results", i + 1,
                           step.tool_results.size());
      for (const auto& tool_result : step.tool_results) {
        if (tool_result.is_success() &&
            tool_result.result.contains("execution_number")) {
          int exec_num = tool_result.result["execution_number"].get<int>();
          ai::logger::log_info("  - Execution number: {}", exec_num);
        }
      }
    }
  }
}

// Instantiate tests for both providers
INSTANTIATE_TEST_SUITE_P(
    ProviderTests,
    MultiStepDuplicateExecutionTest,
    ::testing::Values("openai", "anthropic"),
    [](const ::testing::TestParamInfo<
        MultiStepDuplicateExecutionTest::ParamType>& info) {
      return info.param;
    });

}  // namespace test
}  // namespace ai
