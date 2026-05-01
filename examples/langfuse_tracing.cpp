/**
 * Langfuse Tracing Example - AI SDK C++
 *
 * Demonstrates how to wrap an `ai::Client::generate_text` call (with tools and
 * multi-step) so the LLM call and each tool execution show up as a trace in
 * Langfuse.
 *
 * Required env:
 *   OPENAI_API_KEY        (or use ai::anthropic instead)
 *   LANGFUSE_PUBLIC_KEY
 *   LANGFUSE_SECRET_KEY
 * Optional:
 *   LANGFUSE_HOST         (default: https://cloud.langfuse.com)
 */

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include <ai/langfuse.h>
#include <ai/openai.h>
#include <ai/tools.h>

namespace {

const char* getenv_nonempty(const char* name) {
  const char* v = std::getenv(name);
  return (v && *v) ? v : nullptr;
}

ai::JsonValue lookup_user(const ai::JsonValue& args,
                          const ai::ToolExecutionContext&) {
  static const std::map<std::string, ai::JsonValue> users = {
      {"alice", {{"name", "Alice"}, {"city", "San Francisco"}}},
      {"bob", {{"name", "Bob"}, {"city", "New York"}}},
  };
  auto id = args.value("user_id", std::string{});
  auto it = users.find(id);
  if (it != users.end())
    return it->second;
  return ai::JsonValue{{"error", "user not found"}};
}

ai::JsonValue get_weather(const ai::JsonValue& args,
                          const ai::ToolExecutionContext&) {
  auto loc = args.value("location", std::string{"unknown"});
  return ai::JsonValue{
      {"location", loc}, {"temperature_c", 21}, {"sky", "clear"}};
}

}  // namespace

int main() {
  const char* lf_pk = std::getenv("LANGFUSE_PUBLIC_KEY");
  const char* lf_sk = std::getenv("LANGFUSE_SECRET_KEY");
  if (!lf_pk || !lf_sk) {
    std::cerr << "Set LANGFUSE_PUBLIC_KEY and LANGFUSE_SECRET_KEY first.\n";
    return 1;
  }

  const char* host = std::getenv("LANGFUSE_HOST");
  if (!host || !*host)
    host = std::getenv("LANGFUSE_BASE_URL");
  if (!host || !*host)
    host = "https://cloud.langfuse.com";

  ai::langfuse::Tracer tracer({
      .host = host,
      .public_key = lf_pk,
      .secret_key = lf_sk,
      .environment = "ai-sdk-cpp-example",
  });
  if (!tracer.is_valid()) {
    std::cerr << "Langfuse tracer not configured.\n";
    return 1;
  }

  auto client = ai::openai::create_client();
  if (!client.is_valid()) {
    std::cerr << "OpenAI client not configured (set OPENAI_API_KEY).\n";
    return 1;
  }

  ai::ToolSet tools;
  tools["lookup_user"] = ai::create_tool(
      "Look up a user's profile by id",
      ai::create_object_schema({{"user_id", "string"}}), lookup_user);
  tools["get_weather"] = ai::create_tool(
      "Get the current weather for a location",
      ai::create_object_schema({{"location", "string"}}), get_weather);

  ai::GenerateOptions options;
  options.model = ai::openai::models::kGpt4oMini;
  options.system =
      "You are a concise assistant. Use the available tools when helpful.";
  options.prompt = "Look up alice and tell me the weather where she lives.";
  options.tools = std::move(tools);
  options.max_steps = 4;
  options.temperature = 0.0;

  auto trace = tracer.start_trace("langfuse_tracing_example");
  trace->set_input(options.prompt);
  trace->set_metadata({{"example", "langfuse_tracing"}, {"sdk", "ai-sdk-cpp"}});

  auto result = ai::langfuse::generate_text(client, std::move(options), *trace);

  if (result) {
    std::cout << "Output: " << result.text << "\n";
    trace->set_output(result.text);
  } else {
    std::cerr << "Generation failed: " << result.error_message() << "\n";
    trace->set_output(ai::JsonValue{{"error", result.error_message()}});
  }

  trace->end();
  std::cout << "Trace flushed: id=" << trace->id() << "\n";
  return result ? 0 : 2;
}
