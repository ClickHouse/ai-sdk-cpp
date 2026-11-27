/**
 * Google Gemini Example - AI SDK C++
 *
 * Uses the native Gemini API (`generateContent` and `streamGenerateContent`)
 * through `ai::google::create_client` and shows:
 * - a simple generation with a system instruction
 * - a multi-step tool loop (the thought signatures that Gemini attaches to
 *   its function calls are replayed automatically along with the tool
 *   results)
 * - structured JSON output
 * - streaming
 *
 * Usage:
 *   export GEMINI_API_KEY=your_key_here
 *   ./google_gemini
 */

#include <iostream>
#include <string>

#include <ai/google.h>
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
  auto client = ai::google::try_create_client();
  if (!client) {
    std::cerr << "Set GEMINI_API_KEY to run this example\n";
    return 1;
  }
  std::cout << "Using " << client->config_info() << "\n\n";

  // 1. Simple generation.
  ai::GenerateOptions simple(ai::google::models::kDefaultModel,
                             "You answer in one sentence.",
                             "Why is the sky blue?");
  auto answer = client->generate_text(simple);
  if (!answer) {
    std::cerr << "Error: " << answer.error_message() << "\n";
    return 1;
  }
  std::cout << "Answer: " << answer.text << "\n";
  std::cout << "Tokens: " << answer.usage.total_tokens << "\n\n";

  // 2. Multi-step tool calling.
  ai::GenerateOptions options(
      ai::google::models::kDefaultModel,
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
  std::cout << "Answer (" << result.steps.size() << " steps): " << result.text
            << "\n\n";

  // 3. Structured output: `responseMimeType` + `responseJsonSchema`.
  ai::GenerateOptions structured(ai::google::models::kDefaultModel,
                                 "Name the three largest cities of Japan.");
  structured.response_format = {
      {"type", "json_schema"},
      {"json_schema",
       {{"name", "cities"},
        {"schema",
         {{"type", "object"},
          {"properties",
           {{"cities", {{"type", "array"}, {"items", {{"type", "string"}}}}}}},
          {"required", {"cities"}}}}}}};
  auto cities = client->generate_text(structured);
  if (!cities) {
    std::cerr << "Error: " << cities.error_message() << "\n";
    return 1;
  }
  std::cout << "JSON: " << cities.text << "\n\n";

  // 4. Streaming.
  ai::GenerateOptions stream_options(ai::google::models::kDefaultModel,
                                     "Write a haiku about the sea.");
  auto stream = client->stream_text(ai::StreamOptions(stream_options));
  std::cout << "Streaming: ";
  for (const auto& event : stream) {
    if (event.is_text_delta()) {
      std::cout << event.text_delta << std::flush;
    } else if (event.is_error()) {
      std::cerr << "\nStream error: " << event.error.value_or("") << "\n";
      return 1;
    }
  }
  std::cout << "\n";
  return 0;
}
