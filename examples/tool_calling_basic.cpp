/**
 * Basic Tool Calling Example - AI SDK C++
 *
 * This example demonstrates basic tool calling functionality using the AI SDK.
 * It shows how to:
 * - Define simple tools with schemas and execution functions
 * - Register tools with the generation options
 * - Execute tool calls automatically
 * - Handle tool results
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   ./tool_calling_basic
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include <ai/generate.h>
#include <ai/openai.h>
#include <ai/tools.h>

// Example tool function: Get weather information
ai::JsonValue get_weather(const ai::JsonValue& args,
                          const ai::ToolExecutionContext& context) {
  std::string location = args["location"].get<std::string>();

  // Simulate weather API call
  std::srand(std::time(nullptr));
  int temperature = 60 + (std::rand() % 40);  // Random temp between 60-100°F

  ai::JsonValue result = {{"location", location},
                          {"temperature", temperature},
                          {"unit", "Fahrenheit"},
                          {"condition", "Partly cloudy"}};

  std::cout << "🌤️  Weather tool called for: " << location << std::endl;
  return result;
}

// Example tool function: Get city attractions
ai::JsonValue get_city_attractions(const ai::JsonValue& args,
                                   const ai::ToolExecutionContext& context) {
  std::string city = args["city"].get<std::string>();

  ai::JsonValue attractions;
  if (city == "San Francisco") {
    attractions = {"Golden Gate Bridge", "Alcatraz Island", "Fisherman's Wharf",
                   "Lombard Street"};
  } else if (city == "New York") {
    attractions = {"Statue of Liberty", "Central Park", "Times Square",
                   "Brooklyn Bridge"};
  } else if (city == "Paris") {
    attractions = {"Eiffel Tower", "Louvre Museum", "Notre-Dame Cathedral",
                   "Arc de Triomphe"};
  } else {
    attractions = {"Downtown area", "Local parks", "Historical sites"};
  }

  ai::JsonValue result = {{"city", city}, {"attractions", attractions}};

  std::cout << "🏛️  Attractions tool called for: " << city << std::endl;
  return result;
}

int main() {
  std::cout << "AI SDK C++ - Basic Tool Calling Example\n";
  std::cout << "=========================================\n\n";

  // Create OpenAI client
  auto client = ai::openai::create_client();

  // Define tools using the helper function
  auto weather_tool = ai::create_simple_tool(
      "weather", "Get current weather information for a location",
      {{"location", "string"}}, get_weather);

  auto attractions_tool = ai::create_simple_tool(
      "cityAttractions", "Get popular tourist attractions for a city",
      {{"city", "string"}}, get_city_attractions);

  // Create a tool set
  ai::ToolSet tools = {{"weather", weather_tool},
                       {"cityAttractions", attractions_tool}};

  // Example 1: Single tool call
  std::cout << "1. Single Tool Call Example:\n";
  std::cout << "Question: What's the weather like in San Francisco?\n\n";

  ai::GenerateOptions options1;
  options1.model = ai::openai::models::kGpt4o;
  options1.prompt = "What's the weather like in San Francisco?";
  options1.tools = tools;
  options1.max_tokens = 200;

  auto result1 = client.generate_text(options1);

  if (result1) {
    std::cout << "Assistant: " << result1.text << "\n";

    if (result1.has_tool_calls()) {
      std::cout << "\nTool calls made:\n";
      for (const auto& call : result1.tool_calls) {
        std::cout << "  - " << call.tool_name << ": " << call.arguments.dump()
                  << "\n";
      }
    }

    if (result1.has_tool_results()) {
      std::cout << "\nTool results:\n";
      for (const auto& result : result1.tool_results) {
        std::cout << "  - " << result.tool_name << ": " << result.result.dump()
                  << "\n";
      }
    }

    std::cout << "Usage: " << result1.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result1.error_message() << "\n\n";
  }

  // Example 2: Multiple tools in one request
  std::cout << "2. Multiple Tools Example:\n";
  std::cout << "Question: I'm planning a trip to San Francisco. What's the "
               "weather like and what attractions should I visit?\n\n";

  ai::GenerateOptions options2;
  options2.model = ai::openai::models::kGpt4o;
  options2.prompt =
      "I'm planning a trip to San Francisco. What's the weather like and what "
      "attractions should I visit?";
  options2.tools = tools;
  options2.max_tokens = 300;

  auto result2 = client.generate_text(options2);

  if (result2) {
    std::cout << "Assistant: " << result2.text << "\n";

    if (result2.has_tool_calls()) {
      std::cout << "\nTool calls made (" << result2.tool_calls.size() << "):\n";
      for (const auto& call : result2.tool_calls) {
        std::cout << "  - " << call.tool_name << ": " << call.arguments.dump()
                  << "\n";
      }
    }

    std::cout << "Usage: " << result2.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result2.error_message() << "\n\n";
  }

  // Example 3: Tool choice control
  std::cout << "3. Forced Tool Usage Example:\n";
  std::cout
      << "Question: Tell me about the weather (forced to use weather tool)\n\n";

  ai::GenerateOptions options3;
  options3.model = ai::openai::models::kGpt4o;
  options3.prompt = "Tell me about the weather in New York";
  options3.tools = tools;
  options3.tool_choice =
      ai::ToolChoice::specific("weather");  // Force weather tool
  options3.max_tokens = 150;

  auto result3 = client.generate_text(options3);

  if (result3) {
    std::cout << "Assistant: " << result3.text << "\n";
    std::cout << "Tool choice was forced to 'weather'\n";
    std::cout << "Usage: " << result3.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result3.error_message() << "\n\n";
  }

  // Example 4: No tools allowed
  std::cout << "4. No Tools Example:\n";
  std::cout << "Question: What's the weather like? (tools disabled)\n\n";

  ai::GenerateOptions options4;
  options4.model = ai::openai::models::kGpt4o;
  options4.prompt = "What's the weather like in Boston?";
  options4.tools = tools;
  options4.tool_choice = ai::ToolChoice::none();  // Disable tools
  options4.max_tokens = 100;

  auto result4 = client.generate_text(options4);

  if (result4) {
    std::cout << "Assistant: " << result4.text << "\n";
    std::cout
        << "Tools were disabled - model had to respond without calling tools\n";
    std::cout << "Usage: " << result4.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result4.error_message() << "\n\n";
  }

  std::cout << "Tool calling examples completed!\n";
  std::cout << "\nKey features demonstrated:\n";
  std::cout << "  ✓ Tool definition with schemas and execution functions\n";
  std::cout << "  ✓ Automatic tool execution\n";
  std::cout << "  ✓ Multiple tools in one request\n";
  std::cout << "  ✓ Tool choice control (auto, specific, none)\n";
  std::cout << "  ✓ Tool call and result inspection\n";

  return 0;
}