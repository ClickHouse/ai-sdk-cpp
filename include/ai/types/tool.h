#pragma once

#include "enums.h"
#include "message.h"
#include "usage.h"

#include <functional>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ai {

using JsonValue = nlohmann::json;

/// Context provided to tool execution functions
struct ToolExecutionContext {
  std::string tool_call_id;
  Messages messages;
  std::optional<std::function<void()>> abort_signal;
};

/// Tool execution function signature
/// Parameters: (args, context) -> result
using ToolExecuteFunction =
    std::function<JsonValue(const JsonValue&, const ToolExecutionContext&)>;

/// Async tool execution function signature
/// Parameters: (args, context) -> future<result>
using AsyncToolExecuteFunction =
    std::function<std::future<JsonValue>(const JsonValue&,
                                         const ToolExecutionContext&)>;

struct Tool {
  std::string description;
  JsonValue parameters_schema;
  std::optional<ToolExecuteFunction> execute;
  std::optional<AsyncToolExecuteFunction> execute_async;

  Tool() = default;

  Tool(std::string desc, JsonValue schema, ToolExecuteFunction exec_func)
      : description(std::move(desc)),
        parameters_schema(std::move(schema)),
        execute(std::move(exec_func)) {}

  Tool(std::string desc,
       JsonValue schema,
       AsyncToolExecuteFunction async_exec_func)
      : description(std::move(desc)),
        parameters_schema(std::move(schema)),
        execute_async(std::move(async_exec_func)) {}

  Tool(std::string desc, JsonValue schema)
      : description(std::move(desc)), parameters_schema(std::move(schema)) {}

  bool has_execute() const {
    return execute.has_value() || execute_async.has_value();
  }

  bool is_async() const { return execute_async.has_value(); }
};

using ToolSet = std::map<std::string, Tool>;

/// Tool choice options
enum class ToolChoiceType {
  kAuto,      ///< Model decides whether and which tools to call (default)
  kRequired,  ///< Model must call a tool (any available tool)
  kNone,      ///< Model must not call any tools
  kSpecific   ///< Model must call a specific tool
};

struct ToolChoice {
  ToolChoiceType type = ToolChoiceType::kAuto;
  std::optional<std::string> tool_name;  ///< Required when type is kSpecific

  static ToolChoice auto_choice() {
    return ToolChoice{ToolChoiceType::kAuto, std::nullopt};
  }
  static ToolChoice required() {
    return ToolChoice{ToolChoiceType::kRequired, std::nullopt};
  }
  static ToolChoice none() {
    return ToolChoice{ToolChoiceType::kNone, std::nullopt};
  }
  static ToolChoice specific(const std::string& name) {
    return ToolChoice{ToolChoiceType::kSpecific, name};
  }

  std::string to_string() const {
    switch (type) {
      case ToolChoiceType::kAuto:
        return "auto";
      case ToolChoiceType::kRequired:
        return "required";
      case ToolChoiceType::kNone:
        return "none";
      case ToolChoiceType::kSpecific:
        return "specific(" + tool_name.value_or("") + ")";
      default:
        return "unknown";
    }
  }
};

struct ToolCall {
  std::string id;
  std::string tool_name;
  JsonValue arguments;

  ToolCall(std::string call_id, std::string name, JsonValue args)
      : id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)) {}

  ToolCall() = default;

  bool is_valid() const { return !id.empty() && !tool_name.empty(); }

  std::string to_string() const {
    return "ToolCall{id='" + id + "', tool='" + tool_name +
           "', args=" + arguments.dump() + "}";
  }
};

struct ToolResult {
  std::string tool_call_id;
  std::string tool_name;
  JsonValue arguments;
  JsonValue result;
  std::optional<std::string> error;

  ToolResult(std::string call_id,
             std::string name,
             JsonValue args,
             JsonValue res)
      : tool_call_id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)),
        result(std::move(res)) {}

  ToolResult(std::string call_id,
             std::string name,
             JsonValue args,
             std::string error_msg)
      : tool_call_id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)),
        error(std::move(error_msg)) {}

  ToolResult() = default;

  bool is_success() const { return !error.has_value(); }

  std::string error_message() const { return error.value_or(""); }

  std::string to_string() const {
    if (is_success()) {
      return "ToolResult{id='" + tool_call_id + "', tool='" + tool_name +
             "', result=" + result.dump() + "}";
    } else {
      return "ToolResult{id='" + tool_call_id + "', tool='" + tool_name +
             "', error='" + error_message() + "'}";
    }
  }
};

struct GenerateStep {
  std::string text;
  std::vector<ToolCall> tool_calls;
  std::vector<ToolResult> tool_results;
  FinishReason finish_reason = kFinishReasonError;
  Usage usage;

  bool has_tool_calls() const { return !tool_calls.empty(); }

  bool has_tool_results() const { return !tool_results.empty(); }

  bool is_success() const { return finish_reason != kFinishReasonError; }
};

class ToolError : public std::runtime_error {
 public:
  explicit ToolError(const std::string& message)
      : std::runtime_error(message) {}
};

class NoSuchToolError : public ToolError {
 public:
  std::string tool_name;

  explicit NoSuchToolError(const std::string& name)
      : ToolError("No tool named '" + name + "' is available"),
        tool_name(name) {}
};

class InvalidToolArgumentsError : public ToolError {
 public:
  std::string tool_name;
  JsonValue provided_args;

  InvalidToolArgumentsError(const std::string& name, const JsonValue& args)
      : ToolError("Invalid arguments for tool '" + name + "': " + args.dump()),
        tool_name(name),
        provided_args(args) {}
};

class ToolExecutionError : public ToolError {
 public:
  std::string tool_name;
  std::string tool_call_id;

  ToolExecutionError(const std::string& name,
                     const std::string& call_id,
                     const std::string& message)
      : ToolError("Tool '" + name + "' (call " + call_id +
                  ") execution failed: " + message),
        tool_name(name),
        tool_call_id(call_id) {}
};

/// Helper function to create a simple tool with JSON schema
/// Usage: auto weather_tool = create_tool("Get weather", weather_schema,
/// weather_function);
inline Tool create_tool(const std::string& description,
                        const JsonValue& parameters_schema,
                        ToolExecuteFunction execute_func) {
  return Tool(description, parameters_schema, std::move(execute_func));
}

/// Helper function to create an async tool
inline Tool create_async_tool(const std::string& description,
                              const JsonValue& parameters_schema,
                              AsyncToolExecuteFunction execute_func) {
  return Tool(description, parameters_schema, std::move(execute_func));
}

/// Helper function to create a tool without execution (for forwarding to
/// client)
inline Tool create_tool_schema(const std::string& description,
                               const JsonValue& parameters_schema) {
  return Tool(description, parameters_schema);
}

/// Helper to create JSON schema for simple object parameters
/// Usage: auto schema = create_object_schema({{"location", "string"}, {"units",
/// "string"}});
inline JsonValue create_object_schema(
    const std::map<std::string, std::string>& properties) {
  JsonValue schema = {{"type", "object"},
                      {"properties", JsonValue::object()},
                      {"required", JsonValue::array()}};

  for (const auto& [name, type] : properties) {
    schema["properties"][name] = {{"type", type}};
    schema["required"].push_back(name);
  }

  return schema;
}

}  // namespace ai