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

// Forward declarations
struct ToolCall;
struct ToolExecutionContext;

/// JSON value type for tool parameters and results
using JsonValue = nlohmann::json;

/// Tool execution function signature
/// Parameters: (args, context) -> result
using ToolExecuteFunction =
    std::function<JsonValue(const JsonValue&, const ToolExecutionContext&)>;

/// Async tool execution function signature
/// Parameters: (args, context) -> future<result>
using AsyncToolExecuteFunction =
    std::function<std::future<JsonValue>(const JsonValue&,
                                         const ToolExecutionContext&)>;

/// Context provided to tool execution functions
struct ToolExecutionContext {
  std::string tool_call_id;  ///< Unique ID for this tool call
  Messages messages;         ///< Messages that led to this tool call
  std::optional<std::function<void()>>
      abort_signal;  ///< Abort signal for cancellation
};

/// Tool definition - describes a function the model can call
struct Tool {
  std::string
      description;  ///< Human-readable description of what the tool does
  JsonValue
      parameters_schema;  ///< JSON schema describing the tool's parameters
  std::optional<ToolExecuteFunction>
      execute;  ///< Synchronous execution function (optional)
  std::optional<AsyncToolExecuteFunction>
      execute_async;  ///< Asynchronous execution function (optional)

  /// Default constructor
  Tool() = default;

  /// Constructor for sync execution
  Tool(std::string desc, JsonValue schema, ToolExecuteFunction exec_func)
      : description(std::move(desc)),
        parameters_schema(std::move(schema)),
        execute(std::move(exec_func)) {}

  /// Constructor for async execution
  Tool(std::string desc,
       JsonValue schema,
       AsyncToolExecuteFunction async_exec_func)
      : description(std::move(desc)),
        parameters_schema(std::move(schema)),
        execute_async(std::move(async_exec_func)) {}

  /// Constructor without execution (tool calls forwarded to client)
  Tool(std::string desc, JsonValue schema)
      : description(std::move(desc)), parameters_schema(std::move(schema)) {}

  /// Check if tool has an execution function
  bool has_execute() const {
    return execute.has_value() || execute_async.has_value();
  }

  /// Check if tool uses async execution
  bool is_async() const { return execute_async.has_value(); }
};

/// Collection of tools keyed by name
using ToolSet = std::map<std::string, Tool>;

/// Tool choice options
enum class ToolChoiceType {
  kAuto,      ///< Model decides whether and which tools to call (default)
  kRequired,  ///< Model must call a tool (any available tool)
  kNone,      ///< Model must not call any tools
  kSpecific   ///< Model must call a specific tool
};

/// Tool choice configuration
struct ToolChoice {
  ToolChoiceType type = ToolChoiceType::kAuto;
  std::optional<std::string> tool_name;  ///< Required when type is kSpecific

  /// Factory methods for common choices
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

  /// Convert to string for debugging
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

/// A tool call made by the model
struct ToolCall {
  std::string id;         ///< Unique identifier for this tool call
  std::string tool_name;  ///< Name of the tool being called
  JsonValue arguments;    ///< Arguments passed to the tool

  /// Constructor
  ToolCall(std::string call_id, std::string name, JsonValue args)
      : id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)) {}

  /// Default constructor
  ToolCall() = default;

  /// Check if tool call is valid
  bool is_valid() const { return !id.empty() && !tool_name.empty(); }

  /// Convert to string for debugging
  std::string to_string() const {
    return "ToolCall{id='" + id + "', tool='" + tool_name +
           "', args=" + arguments.dump() + "}";
  }
};

/// Result of executing a tool
struct ToolResult {
  std::string tool_call_id;  ///< ID of the tool call this result corresponds to
  std::string tool_name;     ///< Name of the tool that was executed
  JsonValue arguments;       ///< Arguments that were passed to the tool
  JsonValue result;          ///< Result returned by the tool execution
  std::optional<std::string> error;  ///< Error message if execution failed

  /// Constructor for successful result
  ToolResult(std::string call_id,
             std::string name,
             JsonValue args,
             JsonValue res)
      : tool_call_id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)),
        result(std::move(res)) {}

  /// Constructor for error result
  ToolResult(std::string call_id,
             std::string name,
             JsonValue args,
             std::string error_msg)
      : tool_call_id(std::move(call_id)),
        tool_name(std::move(name)),
        arguments(std::move(args)),
        error(std::move(error_msg)) {}

  /// Default constructor
  ToolResult() = default;

  /// Check if execution was successful
  bool is_success() const { return !error.has_value(); }

  /// Get error message or empty string if successful
  std::string error_message() const { return error.value_or(""); }

  /// Convert to string for debugging
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

/// A single step in multi-step tool calling
struct GenerateStep {
  std::string text;                      ///< Text generated in this step
  std::vector<ToolCall> tool_calls;      ///< Tool calls made in this step
  std::vector<ToolResult> tool_results;  ///< Tool results from this step
  FinishReason finish_reason = kFinishReasonError;  ///< Why this step finished
  Usage usage;  ///< Token usage for this step

  /// Check if step has tool calls
  bool has_tool_calls() const { return !tool_calls.empty(); }

  /// Check if step has tool results
  bool has_tool_results() const { return !tool_results.empty(); }

  /// Check if step completed successfully
  bool is_success() const { return finish_reason != kFinishReasonError; }
};

/// Tool-related error types
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