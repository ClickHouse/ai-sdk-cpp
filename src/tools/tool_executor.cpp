#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <thread>

#include <ai/tools.h>
#include <ai/types/tool.h>

namespace ai {

ToolResult ToolExecutor::execute_tool(const ToolCall& tool_call,
                                      const ToolSet& tools,
                                      const Messages& messages) {
  // Validate tool call
  if (!tool_call.is_valid()) {
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Invalid tool call: missing required fields"));
  }

  // Check if tool exists
  auto tool_it = tools.find(tool_call.tool_name);
  if (tool_it == tools.end()) {
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Tool not found: '" + tool_call.tool_name + "'"));
  }

  const Tool& tool = tool_it->second;

  // Validate arguments against schema
  if (!validate_tool_call(tool_call, tool)) {
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Invalid arguments for tool '" + tool_call.tool_name +
                    "': " + tool_call.arguments.dump()));
  }

  // Check if tool has execution function
  if (!tool.has_execute()) {
    // Tool has no execution function - return the call for forwarding to client
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        JsonValue(std::string(
            "Tool call forwarded to client (no execute function)")));
  }

  // Create execution context
  ToolExecutionContext context;
  context.tool_call_id = tool_call.id;
  context.messages = messages;

  try {
    if (tool.is_async()) {
      return execute_async_tool(tool_call, tool, context);
    } else {
      return execute_sync_tool(tool_call, tool, context);
    }
  } catch (const std::exception& e) {
    // Return error result instead of throwing to allow graceful handling
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Tool execution failed: " + std::string(e.what())));
  }
}

std::vector<ToolResult> ToolExecutor::execute_tools(
    const std::vector<ToolCall>& tool_calls,
    const ToolSet& tools,
    const Messages& messages,
    bool parallel) {
  std::vector<ToolResult> results;
  results.reserve(tool_calls.size());

  if (!parallel) {
    // Execute sequentially
    for (const auto& tool_call : tool_calls) {
      results.push_back(execute_tool(tool_call, tools, messages));
    }
  } else {
    // Execute in parallel using futures
    std::vector<std::future<ToolResult>> futures;
    futures.reserve(tool_calls.size());

    for (const auto& tool_call : tool_calls) {
      futures.emplace_back(
          std::async(std::launch::async, [&tool_call, &tools, &messages]() {
            return execute_tool(tool_call, tools, messages);
          }));
    }

    // Collect results
    for (auto& future : futures) {
      results.push_back(future.get());
    }
  }

  return results;
}

bool ToolExecutor::validate_tool_call(const ToolCall& tool_call,
                                      const Tool& tool) {
  // Basic validation - check if the arguments match the expected schema
  return validate_json_schema(tool_call.arguments, tool.parameters_schema);
}

bool ToolExecutor::tool_exists(const std::string& tool_name,
                               const ToolSet& tools) {
  return tools.find(tool_name) != tools.end();
}

ToolResult ToolExecutor::execute_sync_tool(
    const ToolCall& tool_call,
    const Tool& tool,
    const ToolExecutionContext& context) {
  if (!tool.execute) {
    return ToolResult(tool_call.id, tool_call.tool_name, tool_call.arguments,
                      std::string("Tool has no synchronous execute function"));
  }

  try {
    JsonValue result = tool.execute.value()(tool_call.arguments, context);
    return ToolResult(tool_call.id, tool_call.tool_name, tool_call.arguments,
                      result);
  } catch (const std::exception& e) {
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Tool execution failed: " + std::string(e.what())));
  }
}

ToolResult ToolExecutor::execute_async_tool(
    const ToolCall& tool_call,
    const Tool& tool,
    const ToolExecutionContext& context) {
  if (!tool.execute_async) {
    return ToolResult(tool_call.id, tool_call.tool_name, tool_call.arguments,
                      std::string("Tool has no asynchronous execute function"));
  }

  try {
    auto future = tool.execute_async.value()(tool_call.arguments, context);
    JsonValue result = future.get();  // Wait for completion
    return ToolResult(tool_call.id, tool_call.tool_name, tool_call.arguments,
                      result);
  } catch (const std::exception& e) {
    return ToolResult(
        tool_call.id, tool_call.tool_name, tool_call.arguments,
        std::string("Async tool execution failed: " + std::string(e.what())));
  }
}

bool ToolExecutor::validate_json_schema(const JsonValue& data,
                                        const JsonValue& schema) {
  // Basic JSON schema validation
  // This is a simplified version - a full implementation would use a proper
  // JSON schema validator

  if (!schema.contains("type")) {
    return true;  // No type constraint
  }

  std::string expected_type = schema["type"];

  if (expected_type == "object") {
    if (!data.is_object())
      return false;

    // Check required properties
    if (schema.contains("required") && schema["required"].is_array()) {
      for (const auto& required_prop : schema["required"]) {
        if (!data.contains(required_prop.get<std::string>())) {
          return false;
        }
      }
    }

    // Validate properties (basic check)
    if (schema.contains("properties") && schema["properties"].is_object()) {
      for (const auto& [prop_name, prop_schema] :
           schema["properties"].items()) {
        if (data.contains(prop_name)) {
          if (!validate_json_schema(data[prop_name], prop_schema)) {
            return false;
          }
        }
      }
    }

    return true;
  } else if (expected_type == "string") {
    return data.is_string();
  } else if (expected_type == "number") {
    return data.is_number();
  } else if (expected_type == "integer") {
    return data.is_number_integer();
  } else if (expected_type == "boolean") {
    return data.is_boolean();
  } else if (expected_type == "array") {
    return data.is_array();
  }

  return true;  // Unknown type, accept
}

// Helper functions implementation

Tool create_simple_tool(const std::string& name,
                        const std::string& description,
                        const std::map<std::string, std::string>& parameters,
                        ToolExecuteFunction execute_func) {
  JsonValue schema = create_object_schema(parameters);
  return create_tool(description, schema, std::move(execute_func));
}

Tool create_simple_async_tool(
    const std::string& name,
    const std::string& description,
    const std::map<std::string, std::string>& parameters,
    AsyncToolExecuteFunction execute_func) {
  JsonValue schema = create_object_schema(parameters);
  return create_async_tool(description, schema, std::move(execute_func));
}

ToolSet create_tool_set(
    const std::vector<std::pair<std::string, Tool>>& tool_list) {
  ToolSet tools;
  for (const auto& [name, tool] : tool_list) {
    tools[name] = tool;
  }
  return tools;
}

ToolCall create_tool_call(const std::string& tool_name,
                          const JsonValue& arguments,
                          const std::string& call_id) {
  std::string id = call_id.empty() ? generate_tool_call_id() : call_id;
  return ToolCall(id, tool_name, arguments);
}

std::string generate_tool_call_id() {
  // Generate a unique ID for tool calls
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);

  std::string id = "call_";
  for (int i = 0; i < 24; ++i) {
    int val = dis(gen);
    if (val < 10) {
      id += char('0' + val);
    } else {
      id += char('a' + val - 10);
    }
  }

  return id;
}

}  // namespace ai