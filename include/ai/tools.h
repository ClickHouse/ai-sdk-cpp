#pragma once

// Include all tool-related headers for convenience
#include "types/generate_options.h"
#include "types/message.h"
#include "types/tool.h"

namespace ai {

/// Tool execution engine for running tools and managing multi-step workflows
class ToolExecutor {
 public:
  /// Execute a single tool call
  /// @param tool_call The tool call to execute
  /// @param tools Available tools
  /// @param messages Context messages
  /// @return Tool execution result
  static ToolResult execute_tool(const ToolCall& tool_call,
                                 const ToolSet& tools,
                                 const Messages& messages = {});

  /// Execute multiple tool calls (potentially in parallel)
  /// @param tool_calls Vector of tool calls to execute
  /// @param tools Available tools
  /// @param messages Context messages
  /// @param parallel Whether to execute in parallel (default: true)
  /// @return Vector of tool execution results
  static std::vector<ToolResult> execute_tools(
      const std::vector<ToolCall>& tool_calls,
      const ToolSet& tools,
      const Messages& messages = {},
      bool parallel = true);

  /// Validate tool call arguments against tool schema
  /// @param tool_call The tool call to validate
  /// @param tool The tool definition
  /// @return true if valid, false otherwise
  static bool validate_tool_call(const ToolCall& tool_call, const Tool& tool);

  /// Validate that a tool exists in the tool set
  /// @param tool_name Name of the tool
  /// @param tools Available tools
  /// @return true if tool exists, false otherwise
  static bool tool_exists(const std::string& tool_name, const ToolSet& tools);

 private:
  /// Execute a single tool synchronously
  static ToolResult execute_sync_tool(const ToolCall& tool_call,
                                      const Tool& tool,
                                      const ToolExecutionContext& context);

  /// Execute a single tool asynchronously
  static ToolResult execute_async_tool(const ToolCall& tool_call,
                                       const Tool& tool,
                                       const ToolExecutionContext& context);

  /// Validate JSON against a schema (basic validation)
  static bool validate_json_schema(const JsonValue& data,
                                   const JsonValue& schema);
};

/// Multi-step tool calling coordinator
class MultiStepCoordinator {
 public:
  /// Execute a multi-step tool calling workflow
  /// @param initial_options The initial generation options
  /// @param generate_func Function to call the model (provided by client)
  /// @return Final generation result with all steps
  static GenerateResult execute_multi_step(
      const GenerateOptions& initial_options,
      std::function<GenerateResult(const GenerateOptions&)> generate_func);

 private:
  /// Create the next generation options based on previous step
  static GenerateOptions create_next_step_options(
      const GenerateOptions& base_options,
      const GenerateResult& previous_result,
      const std::vector<ToolResult>& tool_results);

  /// Convert tool results to messages for the next step
  static Messages tool_results_to_messages(
      const std::vector<ToolCall>& tool_calls,
      const std::vector<ToolResult>& tool_results);
};

/// Helper functions for tool creation and management

/// Create a simple tool with basic parameter validation
/// Usage: auto tool = create_simple_tool("weather", "Get weather info",
///                                       {{"location", "string"}},
///                                       weather_func);
Tool create_simple_tool(const std::string& name,
                        const std::string& description,
                        const std::map<std::string, std::string>& parameters,
                        ToolExecuteFunction execute_func);

/// Create an async tool
Tool create_simple_async_tool(
    const std::string& name,
    const std::string& description,
    const std::map<std::string, std::string>& parameters,
    AsyncToolExecuteFunction execute_func);

/// Create a tool set from a list of tools
/// Usage: auto tools = create_tool_set({{"weather", weather_tool}, {"search",
/// search_tool}});
ToolSet create_tool_set(
    const std::vector<std::pair<std::string, Tool>>& tool_list);

/// Helper to create a tool call (useful for testing)
ToolCall create_tool_call(const std::string& tool_name,
                          const JsonValue& arguments,
                          const std::string& call_id = "");

/// Generate a unique tool call ID
std::string generate_tool_call_id();

}  // namespace ai