/**
 * Tool Integration Test - AI SDK C++
 *
 * A simple test to verify that tool calling works with the provider
 * implementations. This example tests the integration between the tool system
 * and the OpenAI/Anthropic clients.
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   export ANTHROPIC_API_KEY=your_key_here
 *   ./test_tool_integration
 */

#include <iostream>
#include <string>

#include <ai/anthropic.h>
#include <ai/openai.h>
#include <ai/tools.h>

// Simple test tool
ai::JsonValue simple_test_tool(const ai::JsonValue& args,
                               const ai::ToolExecutionContext& context) {
  std::string input = args["input"].get<std::string>();

  std::cout << "🔧 Tool called with input: " << input << std::endl;

  return ai::JsonValue{{"result", "Tool executed successfully"},
                       {"input_received", input},
                       {"execution_id", context.tool_call_id}};
}

void test_openai_tools() {
  std::cout << "\n=== Testing OpenAI Tool Integration ===\n";

  auto client = ai::openai::create_client();

  // Create a simple tool
  ai::ToolSet tools = {
      {"test_tool", ai::create_simple_tool(
                        "test_tool", "A simple test tool that echoes input",
                        {{"input", "string"}}, simple_test_tool)}};

  ai::GenerateOptions options;
  options.model = ai::openai::models::kGpt4o;
  options.prompt = "Please use the test_tool with input 'hello world'";
  options.tools = tools;
  options.tool_choice =
      ai::ToolChoice::specific("test_tool");  // Force tool usage
  options.max_tokens = 100;

  std::cout << "Making request to OpenAI with forced tool usage...\n";

  auto result = client.generate_text(options);

  if (result) {
    std::cout << "✅ Request successful!\n";
    std::cout << "Response: " << result.text << "\n";
    std::cout << "Tool calls: " << result.tool_calls.size() << "\n";
    std::cout << "Tool results: " << result.tool_results.size() << "\n";

    if (!result.tool_calls.empty()) {
      const auto& call = result.tool_calls[0];
      std::cout << "First tool call: " << call.tool_name << "\n";
      std::cout << "Arguments: " << call.arguments.dump() << "\n";
    }

    if (!result.tool_results.empty()) {
      const auto& tool_result = result.tool_results[0];
      std::cout << "First tool result: " << tool_result.result.dump() << "\n";
    }
  } else {
    std::cout << "❌ Request failed: " << result.error_message() << "\n";
  }
}

void test_anthropic_tools() {
  std::cout << "\n=== Testing Anthropic Tool Integration ===\n";

  auto client = ai::anthropic::create_client();

  // Create a simple tool
  ai::ToolSet tools = {
      {"test_tool", ai::create_simple_tool(
                        "test_tool", "A simple test tool that echoes input",
                        {{"input", "string"}}, simple_test_tool)}};

  ai::GenerateOptions options;
  options.model = ai::anthropic::models::kClaudeSonnet35;
  options.prompt = "Please use the test_tool with input 'hello anthropic'";
  options.tools = tools;
  options.tool_choice =
      ai::ToolChoice::specific("test_tool");  // Force tool usage
  options.max_tokens = 100;

  std::cout << "Making request to Anthropic with forced tool usage...\n";

  auto result = client.generate_text(options);

  if (result) {
    std::cout << "✅ Request successful!\n";
    std::cout << "Response: " << result.text << "\n";
    std::cout << "Tool calls: " << result.tool_calls.size() << "\n";
    std::cout << "Tool results: " << result.tool_results.size() << "\n";

    if (!result.tool_calls.empty()) {
      const auto& call = result.tool_calls[0];
      std::cout << "First tool call: " << call.tool_name << "\n";
      std::cout << "Arguments: " << call.arguments.dump() << "\n";
    }

    if (!result.tool_results.empty()) {
      const auto& tool_result = result.tool_results[0];
      std::cout << "First tool result: " << tool_result.result.dump() << "\n";
    }
  } else {
    std::cout << "❌ Request failed: " << result.error_message() << "\n";
  }
}

void test_multi_step() {
  std::cout << "\n=== Testing Multi-Step Tool Calling ===\n";

  auto client = ai::openai::create_client();

  // Create tools for multi-step
  ai::ToolSet tools = {
      {"get_number",
       ai::create_simple_tool(
           "get_number", "Get a number for calculation",
           {{"description", "string"}},
           [](const ai::JsonValue& args,
              const ai::ToolExecutionContext& context) -> ai::JsonValue {
             std::string desc = args["description"].get<std::string>();
             std::cout << "🔢 Getting number for: " << desc << std::endl;
             return ai::JsonValue{{"number", 42}};
           })},
      {"calculate",
       ai::create_simple_tool(
           "calculate", "Perform a calculation",
           {{"operation", "string"}, {"a", "number"}, {"b", "number"}},
           [](const ai::JsonValue& args,
              const ai::ToolExecutionContext& context) -> ai::JsonValue {
             std::string op = args["operation"].get<std::string>();
             double a = args["a"].get<double>();
             double b = args["b"].get<double>();

             std::cout << "🧮 Calculating: " << a << " " << op << " " << b
                       << std::endl;

             double result = 0;
             if (op == "add")
               result = a + b;
             else if (op == "multiply")
               result = a * b;

             return ai::JsonValue{{"result", result}};
           })}};

  ai::GenerateOptions options;
  options.model = ai::openai::models::kGpt4o;
  options.prompt = "Get a number and then multiply it by 2. Show me the steps.";
  options.tools = tools;
  options.max_steps = 5;  // Enable multi-step
  options.max_tokens = 200;

  std::cout << "Making multi-step request...\n";

  auto result = client.generate_text(options);

  if (result) {
    std::cout << "✅ Multi-step request successful!\n";
    std::cout << "Final response: " << result.text << "\n";
    std::cout << "Total steps: " << result.steps.size() << "\n";
    std::cout << "Total tool calls: " << result.get_all_tool_calls().size()
              << "\n";
    std::cout << "Total tool results: " << result.get_all_tool_results().size()
              << "\n";
  } else {
    std::cout << "❌ Multi-step request failed: " << result.error_message()
              << "\n";
  }
}

int main() {
  std::cout << "AI SDK C++ - Tool Integration Test\n";
  std::cout << "===================================\n";

  // Test OpenAI integration
  test_openai_tools();

  // Test Anthropic integration
  test_anthropic_tools();

  // Test multi-step functionality
  test_multi_step();

  std::cout << "\n=== Test Summary ===\n";
  std::cout << "Tool integration tests completed!\n";
  std::cout << "Check the output above for success/failure status.\n";

  return 0;
}