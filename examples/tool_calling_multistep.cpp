/**
 * Multi-Step Tool Calling Example - AI SDK C++
 *
 * This example demonstrates multi-step tool calling functionality.
 * It shows how to:
 * - Enable multi-step tool calling with maxSteps
 * - Chain tool calls together
 * - Monitor progress with step callbacks
 * - Handle complex workflows
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   ./tool_calling_multistep
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <string>

#include <ai/generate.h>
#include <ai/openai.h>
#include <ai/tools.h>

// Simulated database of user profiles
std::map<std::string, ai::JsonValue> user_database = {
    {"alice",
     {{"name", "Alice Johnson"},
      {"email", "alice@example.com"},
      {"city", "San Francisco"}}},
    {"bob",
     {{"name", "Bob Smith"},
      {"email", "bob@example.com"},
      {"city", "New York"}}},
    {"charlie",
     {{"name", "Charlie Brown"},
      {"email", "charlie@example.com"},
      {"city", "Paris"}}}};

// Tool: Look up user information
ai::JsonValue lookup_user(const ai::JsonValue& args,
                          const ai::ToolExecutionContext& context) {
  std::string user_id = args["user_id"].get<std::string>();

  std::cout << "🔍 Looking up user: " << user_id << std::endl;

  auto it = user_database.find(user_id);
  if (it != user_database.end()) {
    return it->second;
  } else {
    return ai::JsonValue{{"error", "User not found"}};
  }
}

// Tool: Get weather for a location
ai::JsonValue get_weather(const ai::JsonValue& args,
                          const ai::ToolExecutionContext& context) {
  std::string location = args["location"].get<std::string>();

  std::cout << "🌤️  Getting weather for: " << location << std::endl;

  // Simulate weather API call
  std::srand(std::time(nullptr) + location.length());  // Add some variety
  int temperature = 50 + (std::rand() % 50);

  std::vector<std::string> conditions = {"Sunny", "Cloudy", "Rainy",
                                         "Partly cloudy"};
  std::string condition = conditions[std::rand() % conditions.size()];

  return ai::JsonValue{{"location", location},
                       {"temperature", temperature},
                       {"condition", condition},
                       {"unit", "Fahrenheit"}};
}

// Tool: Send email notification
ai::JsonValue send_email(const ai::JsonValue& args,
                         const ai::ToolExecutionContext& context) {
  std::string to = args["to"].get<std::string>();
  std::string subject = args["subject"].get<std::string>();
  std::string body = args["body"].get<std::string>();

  std::cout << "📧 Sending email to: " << to << std::endl;
  std::cout << "   Subject: " << subject << std::endl;

  // Simulate email sending
  return ai::JsonValue{{"status", "sent"},
                       {"message_id", "msg_" + std::to_string(std::rand())},
                       {"recipient", to}};
}

// Tool: Get city recommendations
ai::JsonValue get_recommendations(const ai::JsonValue& args,
                                  const ai::ToolExecutionContext& context) {
  std::string city = args["city"].get<std::string>();
  std::string weather_condition =
      args.contains("weather") ? args["weather"].get<std::string>() : "unknown";

  std::cout << "💡 Getting recommendations for: " << city
            << " (weather: " << weather_condition << ")" << std::endl;

  std::vector<std::string> indoor_activities = {
      "Visit museums", "Go shopping", "Try local restaurants", "See a movie"};
  std::vector<std::string> outdoor_activities = {
      "Walk in parks", "Visit landmarks", "Take photos", "Outdoor dining"};

  ai::JsonValue activities;
  if (weather_condition == "Rainy") {
    activities = indoor_activities;
  } else {
    // Mix of indoor and outdoor
    activities = ai::JsonValue::array();
    for (int i = 0; i < 2; ++i) {
      activities.push_back(indoor_activities[i]);
      activities.push_back(outdoor_activities[i]);
    }
  }

  return ai::JsonValue{{"city", city},
                       {"weather_condition", weather_condition},
                       {"recommendations", activities}};
}

int main() {
  std::cout << "AI SDK C++ - Multi-Step Tool Calling Example\n";
  std::cout << "=============================================\n\n";

  // Create OpenAI client
  auto client = ai::openai::create_client();

  // Define tools
  ai::ToolSet tools = {
      {"lookup_user", ai::create_simple_tool(
                          "lookup_user", "Look up user information by user ID",
                          {{"user_id", "string"}}, lookup_user)},
      {"get_weather", ai::create_simple_tool(
                          "get_weather", "Get current weather for a location",
                          {{"location", "string"}}, get_weather)},
      {"send_email",
       ai::create_simple_tool(
           "send_email", "Send an email notification",
           {{"to", "string"}, {"subject", "string"}, {"body", "string"}},
           send_email)},
      {"get_recommendations",
       ai::create_simple_tool("get_recommendations",
                              "Get activity recommendations for a city, "
                              "optionally considering weather",
                              {{"city", "string"}, {"weather", "string"}},
                              get_recommendations)}};

  // Step callback to monitor progress
  auto step_callback = [](const ai::GenerateStep& step) {
    std::cout << "\n--- Step Completed ---\n";
    std::cout << "Text: " << step.text << "\n";
    std::cout << "Tool calls: " << step.tool_calls.size() << "\n";
    std::cout << "Tool results: " << step.tool_results.size() << "\n";
    std::cout << "Finish reason: ";
    switch (step.finish_reason) {
      case ai::kFinishReasonStop:
        std::cout << "stop";
        break;
      case ai::kFinishReasonLength:
        std::cout << "length";
        break;
      case ai::kFinishReasonToolCalls:
        std::cout << "tool_calls";
        break;
      case ai::kFinishReasonError:
        std::cout << "error";
        break;
      default:
        std::cout << "unknown";
        break;
    }
    std::cout << "\n----------------------\n\n";
  };

  // Example 1: Multi-step workflow
  std::cout << "1. Multi-Step Workflow Example:\n";
  std::cout << "Task: Look up user 'alice', get weather for her city, get "
               "recommendations, and send her an email\n\n";

  ai::GenerateOptions options1;
  options1.model = ai::openai::models::kGpt4o;
  options1.prompt = R"(
    Please help me with this task:
    1. Look up the user 'alice' to get her information
    2. Get the current weather for her city
    3. Get activity recommendations for her city based on the weather
    4. Send her an email with a personalized message about the weather and recommendations
    
    Use the available tools to complete this task step by step.
  )";
  options1.tools = tools;
  options1.max_steps = 6;  // Allow multiple steps
  options1.max_tokens = 200;
  options1.on_step_finish = step_callback;

  auto result1 = client.generate_text(options1);

  if (result1) {
    std::cout << "Final Result:\n";
    std::cout << "=============\n";
    std::cout << "Text: " << result1.text << "\n";
    std::cout << "Total Steps: " << result1.steps.size() << "\n";
    std::cout << "Total Tool Calls: " << result1.get_all_tool_calls().size()
              << "\n";
    std::cout << "Total Tool Results: " << result1.get_all_tool_results().size()
              << "\n";
    std::cout << "Total Tokens: " << result1.usage.total_tokens << "\n\n";

    // Show the workflow breakdown
    std::cout << "Workflow Breakdown:\n";
    for (size_t i = 0; i < result1.steps.size(); ++i) {
      const auto& step = result1.steps[i];
      std::cout << "  Step " << (i + 1) << ": ";
      if (!step.tool_calls.empty()) {
        std::cout << "Called " << step.tool_calls.size() << " tool(s): ";
        for (const auto& call : step.tool_calls) {
          std::cout << call.tool_name << " ";
        }
      } else {
        std::cout << "Generated text response";
      }
      std::cout << "\n";
    }
  } else {
    std::cout << "Error: " << result1.error_message() << "\n\n";
  }

  // Example 2: User choice workflow
  std::cout << "\n2. Interactive Multi-Step Example:\n";
  std::cout
      << "Task: Look up user 'bob' and create a personalized travel report\n\n";

  ai::GenerateOptions options2;
  options2.model = ai::openai::models::kGpt4o;
  options2.prompt = R"(
    Create a personalized travel report for user 'bob':
    1. Look up Bob's profile to get his location
    2. Get the current weather for his city
    3. Get recommendations for activities based on the weather
    4. Create a summary report and ask if he'd like it emailed to him
    
    Be conversational and helpful in your responses.
  )";
  options2.tools = tools;
  options2.max_steps = 5;
  options2.max_tokens = 250;

  auto result2 = client.generate_text(options2);

  if (result2) {
    std::cout << "Interactive Report:\n";
    std::cout << "==================\n";
    std::cout << result2.text << "\n";
    std::cout << "Steps taken: " << result2.steps.size() << "\n";
    std::cout << "Tokens used: " << result2.usage.total_tokens << "\n\n";
  } else {
    std::cout << "Error: " << result2.error_message() << "\n\n";
  }

  std::cout << "Multi-step tool calling examples completed!\n";
  std::cout << "\nKey features demonstrated:\n";
  std::cout << "  ✓ Multi-step tool calling with max_steps\n";
  std::cout << "  ✓ Tool chaining and workflow coordination\n";
  std::cout << "  ✓ Step-by-step progress monitoring\n";
  std::cout << "  ✓ Complex task breakdown\n";
  std::cout << "  ✓ Tool result integration across steps\n";
  std::cout << "  ✓ Error handling in multi-step workflows\n";

  return 0;
}