#include "../utils/test_fixtures.h"
#include "ai/anthropic.h"
#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/tool.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

namespace ai {
namespace test {

// Test fixtures for tool calling functionality
class ToolTestFixtures {
 public:
  // Create a simple weather tool
  static Tool createWeatherTool() {
    return create_simple_tool(
        "weather", "Get current weather information for a location",
        {{"location", "string"}},
        [](const JsonValue& args, const ToolExecutionContext& context) {
          std::string location = args["location"].get<std::string>();
          return JsonValue{{"location", location},
                           {"temperature", 72},
                           {"condition", "Sunny"},
                           {"humidity", 45}};
        });
  }

  // Create a calculator tool (using manual schema for complex validation)
  static Tool createCalculatorTool() {
    auto schema =
        JsonValue{{"type", "object"},
                  {"properties",
                   {{"operation",
                     {{"type", "string"},
                      {"enum", {"add", "subtract", "multiply", "divide"}}}},
                    {"a", {{"type", "number"}}},
                    {"b", {{"type", "number"}}}}},
                  {"required", {"operation", "a", "b"}}};

    return Tool(
        "Perform basic arithmetic operations", schema,
        [](const JsonValue& args, const ToolExecutionContext& context) {
          std::string op = args["operation"].get<std::string>();
          double a = args["a"].get<double>();
          double b = args["b"].get<double>();

          double result = 0.0;
          if (op == "add")
            result = a + b;
          else if (op == "subtract")
            result = a - b;
          else if (op == "multiply")
            result = a * b;
          else if (op == "divide") {
            if (b == 0.0)
              throw std::runtime_error("Division by zero");
            result = a / b;
          }

          return JsonValue{
              {"operation", op}, {"a", a}, {"b", b}, {"result", result}};
        });
  }

  // Create an async tool
  static Tool createAsyncTimeTool() {
    return create_simple_async_tool(
        "time", "Get current time in specified timezone",
        {{"timezone", "string"}},
        [](const JsonValue& args, const ToolExecutionContext& context) {
          return std::async(std::launch::async, [args]() {
            // Simulate async operation
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string timezone = args["timezone"].get<std::string>();
            return JsonValue{{"timezone", timezone},
                             {"time", "2024-01-01T12:00:00Z"}};
          });
        });
  }

  // Create a tool that throws an error
  static Tool createErrorTool() {
    return create_simple_tool(
        "error_tool", "A tool that always throws an error",
        {{"input", "string"}},
        [](const JsonValue& args,
           const ToolExecutionContext& context) -> JsonValue {
          throw std::runtime_error("Intentional test error");
        });
  }

  // Create common tool set
  static ToolSet createTestToolSet() {
    return {{"weather", createWeatherTool()},
            {"calculator", createCalculatorTool()},
            {"time", createAsyncTimeTool()},
            {"error_tool", createErrorTool()}};
  }

  // Create tool calls for testing
  static ToolCall createWeatherCall() {
    return ToolCall("call_123", "weather",
                    JsonValue{{"location", "San Francisco"}});
  }

  static ToolCall createCalculatorCall() {
    return ToolCall("call_456", "calculator",
                    JsonValue{{"operation", "add"}, {"a", 10}, {"b", 5}});
  }

  static ToolCall createInvalidCall() {
    return ToolCall("call_789", "nonexistent", JsonValue{{"param", "value"}});
  }
};

// Parameterized test class that works with both providers
class ToolCallingIntegrationTest
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
        model_ = ai::anthropic::models::kClaude35Sonnet;
      } else {
        use_real_api_ = false;
      }
    }

    tools_ = ToolTestFixtures::createTestToolSet();
  }

  bool use_real_api_ = false;
  std::optional<ai::Client> client_;
  std::string model_;
  ToolSet tools_;
};

// Test single tool calling
TEST_P(ToolCallingIntegrationTest, SingleToolCall) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "What's the weather like in San Francisco?");
  options.tools = tools_;
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // Result should be successful, though text might be empty if only tool calls
  // are returned
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have called the weather tool
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_GT(result.tool_calls.size(), 0);

  auto weather_call = std::find_if(
      result.tool_calls.begin(), result.tool_calls.end(),
      [](const ToolCall& call) { return call.tool_name == "weather"; });
  EXPECT_NE(weather_call, result.tool_calls.end());

  if (weather_call != result.tool_calls.end()) {
    EXPECT_TRUE(weather_call->arguments.contains("location"));
  }
}

// Test multiple tool calls in one request
TEST_P(ToolCallingIntegrationTest, MultipleDifferentToolCalls) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(
      model_, "What's the weather in New York and what's 25 plus 17?");
  options.tools = tools_;
  options.max_tokens = 300;

  auto result = client_->generate_text(options);

  // Result should be successful, text might be empty if only tool calls are
  // returned
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have made tool calls
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_GE(result.tool_calls.size(), 1);

  // Check for weather tool call
  bool found_weather = false;
  bool found_calculator = false;

  for (const auto& call : result.tool_calls) {
    if (call.tool_name == "weather")
      found_weather = true;
    if (call.tool_name == "calculator")
      found_calculator = true;
  }

  // At least one tool should be called, preferably both
  EXPECT_TRUE(found_weather || found_calculator);
}

// Test tool choice control - force specific tool
TEST_P(ToolCallingIntegrationTest, ForcedToolChoice) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "Calculate something for me");
  options.tools = tools_;
  options.tool_choice = ToolChoice::specific("calculator");
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // When forcing tool use, the result should be successful but may have empty
  // text
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have made exactly one tool call to calculator
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_EQ(result.tool_calls.size(), 1);
  EXPECT_EQ(result.tool_calls[0].tool_name, "calculator");
}

// Test tool choice - auto mode
TEST_P(ToolCallingIntegrationTest, AutoToolChoice) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "What's 10 + 5?");
  options.tools = tools_;
  options.tool_choice = ToolChoice::auto_choice();
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // With auto tool choice, result should be successful
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Model may or may not include text when using tools
  // Text content is optional when tool calls are made

  // Model should decide whether to use tools (likely will for math)
  // If tools are used, should be calculator
  if (result.has_tool_calls()) {
    bool found_calculator = false;
    for (const auto& call : result.tool_calls) {
      if (call.tool_name == "calculator")
        found_calculator = true;
    }
    EXPECT_TRUE(found_calculator);
  }
}

// Test tool choice - none (disable tools)
TEST_P(ToolCallingIntegrationTest, NoToolChoice) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // Skip for Anthropic as it doesn't support tool_choice "none"
  if (GetParam() == "anthropic") {
    GTEST_SKIP() << "Anthropic doesn't support tool_choice 'none'";
  }

  GenerateOptions options(model_, "What's the weather like?");
  options.tools = tools_;
  options.tool_choice = ToolChoice::none();
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());

  // Should not have made any tool calls
  EXPECT_FALSE(result.has_tool_calls());
}

// Test tool choice - required (any tool)
TEST_P(ToolCallingIntegrationTest, RequiredToolChoice) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "Help me with something");
  options.tools = tools_;
  options.tool_choice = ToolChoice::required();
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // When requiring tool use, the result should be successful but may have empty
  // text
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have made at least one tool call
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_GT(result.tool_calls.size(), 0);
}

// Test complex tool parameters
TEST_P(ToolCallingIntegrationTest, ComplexToolParameters) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "Calculate 25.5 multiplied by 3.7");
  options.tools = tools_;
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // Result should be successful, text is optional when using tools
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should use calculator with floating point numbers
  EXPECT_TRUE(result.has_tool_calls());

  auto calc_call = std::find_if(
      result.tool_calls.begin(), result.tool_calls.end(),
      [](const ToolCall& call) { return call.tool_name == "calculator"; });

  if (calc_call != result.tool_calls.end()) {
    EXPECT_TRUE(calc_call->arguments.contains("operation"));
    EXPECT_TRUE(calc_call->arguments.contains("a"));
    EXPECT_TRUE(calc_call->arguments.contains("b"));
    EXPECT_EQ(calc_call->arguments["operation"].get<std::string>(), "multiply");
  }
}

// Test tool execution results
TEST_P(ToolCallingIntegrationTest, ToolExecutionResults) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(
      model_, "What's 15 + 25? Please show me the exact calculation.");
  options.tools = tools_;
  options.max_tokens = 300;

  auto result = client_->generate_text(options);

  // Result should be successful, even if text is empty when using tools
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have tool calls and results
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_TRUE(result.has_tool_results());

  // Find calculator result
  auto calc_result = std::find_if(
      result.tool_results.begin(), result.tool_results.end(),
      [](const ToolResult& res) { return res.tool_name == "calculator"; });

  if (calc_result != result.tool_results.end()) {
    EXPECT_TRUE(calc_result->is_success());
    EXPECT_TRUE(calc_result->result.contains("result"));
    EXPECT_EQ(calc_result->result["result"].get<double>(), 40.0);
  }
}

// Test async tool execution
TEST_P(ToolCallingIntegrationTest, AsyncToolExecution) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  GenerateOptions options(model_, "What time is it in UTC?");
  options.tools = tools_;
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // Result should be successful, text might be empty if only tool calls are
  // returned
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should potentially use the time tool
  if (result.has_tool_calls()) {
    auto time_call = std::find_if(
        result.tool_calls.begin(), result.tool_calls.end(),
        [](const ToolCall& call) { return call.tool_name == "time"; });

    if (time_call != result.tool_calls.end()) {
      EXPECT_TRUE(time_call->arguments.contains("timezone"));
    }
  }
}

// Test error handling in tools
TEST_P(ToolCallingIntegrationTest, ToolExecutionError) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // Create a minimal tool set with just the error tool
  ToolSet error_tools = {{"error_tool", ToolTestFixtures::createErrorTool()}};

  GenerateOptions options(model_, "Use the error tool");
  options.tools = error_tools;
  options.tool_choice = ToolChoice::specific("error_tool");
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // The overall result should still be successful even if tool execution fails
  // When forcing tool use, text might be empty
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  // Should have tool calls and results
  EXPECT_TRUE(result.has_tool_calls());
  EXPECT_TRUE(result.has_tool_results());

  // The tool result should indicate an error
  auto error_result = std::find_if(
      result.tool_results.begin(), result.tool_results.end(),
      [](const ToolResult& res) { return res.tool_name == "error_tool"; });

  if (error_result != result.tool_results.end()) {
    EXPECT_FALSE(error_result->is_success());
    EXPECT_TRUE(error_result->error.has_value());
    EXPECT_THAT(error_result->error_message(),
                testing::HasSubstr("Intentional test error"));
  }
}

// Test invalid tool schema validation
TEST_P(ToolCallingIntegrationTest, InvalidToolArguments) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // This test relies on the model providing correct arguments based on schema
  // If the model provides invalid arguments, tool execution should handle it
  // gracefully
  GenerateOptions options(model_,
                          "Calculate the result without providing numbers");
  options.tools = tools_;
  options.tool_choice = ToolChoice::specific("calculator");
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // Result should be successful (API call worked), text might be empty when
  // forcing tool
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";

  if (result.has_tool_calls()) {
    EXPECT_EQ(result.tool_calls[0].tool_name, "calculator");

    // If tool results exist and show an error due to missing/invalid params,
    // that's expected behavior
    if (result.has_tool_results()) {
      auto calc_result = std::find_if(
          result.tool_results.begin(), result.tool_results.end(),
          [](const ToolResult& res) { return res.tool_name == "calculator"; });
      if (calc_result != result.tool_results.end()) {
        // Tool might succeed or fail depending on what the model provides
        // Both outcomes are acceptable for this test
      }
    }
  }
}

// Test concurrent tool calls
TEST_P(ToolCallingIntegrationTest, ConcurrentToolCalls) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  const int num_requests = 2;
  std::vector<std::future<GenerateResult>> futures;
  futures.reserve(num_requests);

  for (int i = 0; i < num_requests; ++i) {
    futures.push_back(std::async(std::launch::async, [this, i]() {
      GenerateOptions options(model_, "What's " + std::to_string(10 + i * 5) +
                                          " + " + std::to_string(20 + i * 3) +
                                          "?");
      options.tools = tools_;
      options.max_tokens = 200;
      return client_->generate_text(options);
    }));
  }

  // Wait for all requests to complete
  for (auto& future : futures) {
    auto result = future.get();
    // Result should be successful, text might be empty if only tool calls are
    // returned
    EXPECT_TRUE(result.is_success())
        << "Expected successful result but got error: "
        << result.error_message();
    EXPECT_FALSE(result.error.has_value())
        << "Expected no error in successful result";
  }
}

// Test tool schema validation
TEST_P(ToolCallingIntegrationTest, ToolSchemaValidation) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  // Test with a tool that has strict schema requirements
  auto strict_tool = Tool(
      "Tool with strict schema validation",
      JsonValue{
          {"type", "object"},
          {"properties",
           {{"required_string", {{"type", "string"}, {"minLength", 1}}},
            {"required_number", {{"type", "number"}, {"minimum", 0}}},
            {"enum_value",
             {{"type", "string"},
              {"enum", {"option1", "option2", "option3"}}}}}},
          {"required", {"required_string", "required_number", "enum_value"}},
          {"additionalProperties", false}},
      [](const JsonValue& args, const ToolExecutionContext&) {
        return JsonValue{{"status", "success"}, {"received_args", args}};
      });

  ToolSet strict_tools = {{"strict_tool", strict_tool}};

  GenerateOptions options(model_,
                          "Use the strict tool with required_string='test', "
                          "required_number=42, and enum_value='option1'");
  options.tools = strict_tools;
  options.tool_choice = ToolChoice::specific("strict_tool");
  options.max_tokens = 200;

  auto result = client_->generate_text(options);

  // When forcing specific tool, text might be empty
  EXPECT_TRUE(result.is_success())
      << "Expected successful result but got error: " << result.error_message();
  EXPECT_FALSE(result.error.has_value())
      << "Expected no error in successful result";
  EXPECT_TRUE(result.has_tool_calls());

  if (result.has_tool_calls()) {
    const auto& call = result.tool_calls[0];
    EXPECT_EQ(call.tool_name, "strict_tool");

    // Verify the model provided arguments that match the schema
    if (call.arguments.contains("required_string") &&
        call.arguments.contains("required_number") &&
        call.arguments.contains("enum_value")) {
      EXPECT_TRUE(call.arguments["required_string"].is_string());
      EXPECT_TRUE(call.arguments["required_number"].is_number());
      EXPECT_TRUE(call.arguments["enum_value"].is_string());

      std::string enum_val = call.arguments["enum_value"].get<std::string>();
      EXPECT_THAT(enum_val, testing::AnyOf("option1", "option2", "option3"));
    }
  }
}

// Instantiate tests for both providers
INSTANTIATE_TEST_SUITE_P(
    ProviderTests,
    ToolCallingIntegrationTest,
    ::testing::Values("openai", "anthropic"),
    [](const ::testing::TestParamInfo<ToolCallingIntegrationTest::ParamType>&
           info) { return info.param; });

// Provider-specific tests for features that don't work on both

class OpenAISpecificToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (api_key) {
      use_real_api_ = true;
      client_ = ai::openai::create_client(api_key);
      model_ = ai::openai::models::kGpt4oMini;
    } else {
      use_real_api_ = false;
    }

    tools_ = ToolTestFixtures::createTestToolSet();
  }

  bool use_real_api_ = false;
  std::optional<ai::Client> client_;
  std::string model_;
  ToolSet tools_;
};

// Test OpenAI-specific tool choice "none"
TEST_F(OpenAISpecificToolTest, ToolChoiceNoneSupport) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No OPENAI_API_KEY environment variable set";
  }

  GenerateOptions options(model_, "Tell me about the weather");
  options.tools = tools_;
  options.tool_choice = ToolChoice::none();
  options.max_tokens = 150;

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_FALSE(result.has_tool_calls());
}

class AnthropicSpecificToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* api_key = std::getenv("ANTHROPIC_API_KEY");
    if (api_key) {
      use_real_api_ = true;
      client_ = ai::anthropic::create_client(api_key);
      model_ = ai::anthropic::models::kClaude35Sonnet;
    } else {
      use_real_api_ = false;
    }

    tools_ = ToolTestFixtures::createTestToolSet();
  }

  bool use_real_api_ = false;
  std::optional<ai::Client> client_;
  std::string model_;
  ToolSet tools_;
};

// Test Anthropic-specific behavior
TEST_F(AnthropicSpecificToolTest, MaxTokensRequiredWithTools) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No ANTHROPIC_API_KEY environment variable set";
  }

  GenerateOptions options(model_, "What's the weather in Paris?");
  options.tools = tools_;
  options.max_tokens = 200;  // Anthropic requires max_tokens

  auto result = client_->generate_text(options);

  TestAssertions::assertSuccess(result);
  EXPECT_FALSE(result.text.empty());
  EXPECT_LE(result.usage.completion_tokens, 200);
}

}  // namespace test
}  // namespace ai