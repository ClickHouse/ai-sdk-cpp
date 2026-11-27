#include <cstdlib>
#include <httplib.h>
#include <iostream>
#include <stdexcept>
#include <string>

#include <ai/tools.h>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: loom_document_search 'search query'\n";
    return 1;
  }

  const char* origin = std::getenv("LOOM_BASE_URL");
  const char* org = std::getenv("LOOM_ORG");
  const char* namespace_name = std::getenv("LOOM_NAMESPACE");
  const char* token = std::getenv("LOOM_VIEWER_TOKEN");
  if (!origin || !*origin || !org || !*org || !namespace_name ||
      !*namespace_name || !token || !*token) {
    std::cerr << "Set LOOM_BASE_URL, LOOM_ORG, LOOM_NAMESPACE, and "
                 "LOOM_VIEWER_TOKEN.\n";
    return 1;
  }

  auto search = ai::create_simple_tool(
      "search_documents", "Search team documents for cited passages",
      {{"query", "string"}},
      [=](const ai::JsonValue& arguments, const ai::ToolExecutionContext&) {
        httplib::Client client(origin);
        client.set_connection_timeout(5);
        client.set_read_timeout(30);
        client.set_bearer_token_auth(token);

        const ai::JsonValue body = {{"org", org},
                                    {"namespace", namespace_name},
                                    {"namespace_scope", "this"},
                                    {"query", arguments.at("query")},
                                    {"search_mode", "lexical"},
                                    {"rerank", "none"},
                                    {"top_k", 3}};
        const auto response = client.Post("/v1/knowledge.search", body.dump(),
                                          "application/json");
        if (!response) {
          throw std::runtime_error("Could not reach Loom");
        }
        if (response->status != 200) {
          throw std::runtime_error("Loom returned HTTP " +
                                   std::to_string(response->status));
        }
        return ai::JsonValue::parse(response->body).at("results");
      });

  const ai::ToolSet tools = {{"search_documents", search}};
  const auto call =
      ai::create_tool_call("search_documents", {{"query", argv[1]}});
  const auto result = ai::ToolExecutor::execute_tool(call, tools);
  if (!result.is_success()) {
    std::cerr << result.error_message() << '\n';
    return 1;
  }
  std::cout << result.result.dump(2) << '\n';
}
