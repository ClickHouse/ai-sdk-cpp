/**
 * OpenAI Responses API Example - AI SDK C++
 *
 * The GPT-6 models call tools through the Responses API only. This example
 * creates a Responses API client with `ai::openai::create_responses_client`
 * and shows:
 * - a multi-step tool loop on GPT-6.2 Sol (the reasoning items of each step
 *   are replayed automatically along with the tool results)
 * - continuing the conversation from `response_messages`
 * - streaming
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   ./openai_responses
 */

#include <iostream>
#include <string>

#include <ai/openai.h>
#include <ai/tools.h>

namespace {

ai::JsonValue get_weather(const ai::JsonValue& args,
                          const ai::ToolExecutionContext& /*context*/) {
  const auto location = args["location"].get<std::string>();
  std::cout << "  [tool] weather(" << location << ")\n";
  return {{"location", location},
          {"temperature", 68},
          {"unit", "Fahrenheit"},
          {"condition", "Light rain"}};
}

ai::JsonValue get_time(const ai::JsonValue& args,
                       const ai::ToolExecutionContext& /*context*/) {
  const auto city = args["city"].get<std::string>();
  std::cout << "  [tool] local_time(" << city << ")\n";
  return {{"city", city}, {"local_time", "16:20"}};
}

}  // namespace

int main() {
  auto client = ai::openai::try_create_responses_client();
  if (!client) {
    std::cerr << "Set OPENAI_API_KEY to run this example\n";
    return 1;
  }
  std::cout << "Using " << client->config_info() << "\n\n";

  // 1. Multi-step tool calling.
  ai::GenerateOptions options(
      ai::openai::models::kGpt62Sol,
      "What is the weather in Seattle, and what time is it there? Should I "
      "take an umbrella for a walk now?");
  options.tools = {
      {"weather",
       ai::create_simple_tool("weather", "Get the current weather for a city",
                              {{"location", "string"}}, get_weather)},
      {"local_time",
       ai::create_simple_tool("local_time", "Get the local time in a city",
                              {{"city", "string"}}, get_time)}};
  options.max_steps = 5;

  auto result = client->generate_text(options);
  if (!result) {
    std::cerr << "Error: " << result.error_message() << "\n";
    return 1;
  }
  std::cout << "Answer (" << result.steps.size()
            << " steps): " << result.text << "\n";
  std::cout << "Tokens: " << result.usage.total_tokens << "\n\n";

  // 2. Continue the conversation. `response_messages` holds the assistant
  // turns, tool results, and reasoning items of the previous call.
  ai::Messages conversation = {ai::Message::user(options.prompt)};
  conversation.insert(conversation.end(), result.response_messages.begin(),
                      result.response_messages.end());
  conversation.push_back(ai::Message::user("And what about in two hours?"));

  ai::GenerateOptions follow_up(ai::openai::models::kGpt62Sol,
                                std::move(conversation));
  follow_up.tools = options.tools;
  follow_up.max_steps = 3;
  auto follow_up_result = client->generate_text(follow_up);
  if (!follow_up_result) {
    std::cerr << "Error: " << follow_up_result.error_message() << "\n";
    return 1;
  }
  std::cout << "Follow-up: " << follow_up_result.text << "\n\n";

  // 3. Streaming.
  std::cout << "Streaming: ";
  auto stream = client->stream_text(ai::StreamOptions(ai::GenerateOptions(
      ai::openai::models::kGpt6Luna, "Write a haiku about rain.")));
  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      std::cout << event.text_delta << std::flush;
    } else if (event.is_error()) {
      std::cerr << "\nStream error: " << event.error.value_or("") << "\n";
      return 1;
    } else if (event.is_finish()) {
      break;
    }
  }
  std::cout << "\n";
  return 0;
}
