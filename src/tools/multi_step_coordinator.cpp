#include <map>

#include <ai/logger.h>
#include <ai/tools.h>
#include <ai/types/enums.h>
#include <ai/types/tool.h>

namespace ai {

GenerateResult MultiStepCoordinator::execute_multi_step(
    const GenerateOptions& initial_options,
    const std::function<GenerateResult(const GenerateOptions&)>&
        generate_func) {
  if (initial_options.max_steps <= 1) {
    // Single step - just execute normally
    return generate_func(initial_options);
  }

  GenerateResult final_result;
  GenerateOptions current_options = initial_options;

  for (int step = 0; step < initial_options.max_steps; ++step) {
    ai::logger::log_debug("Executing step {} of {}", step + 1,
                          initial_options.max_steps);
    ai::logger::log_debug("Current messages count: {}", 
                          current_options.messages.size());
    ai::logger::log_debug("System prompt: {}", 
                          current_options.system.empty() ? "empty" : current_options.system.substr(0, 100) + "...");

    // Execute the current step
    GenerateResult step_result = generate_func(current_options);
    
    ai::logger::log_debug("Step {} result - text: '{}', tool_calls: {}, finish_reason: {}",
                          step + 1, step_result.text, step_result.tool_calls.size(), 
                          static_cast<int>(step_result.finish_reason));

    // Check for errors
    if (!step_result.is_success()) {
      // If this is the first step, return the error
      if (step == 0) {
        return step_result;
      }
      // Otherwise, return the final result with the error noted
      final_result.error = step_result.error;
      break;
    }

    // Create a step record
    GenerateStep generate_step;
    generate_step.text = step_result.text;
    generate_step.tool_calls = step_result.tool_calls;
    generate_step.tool_results = step_result.tool_results;
    generate_step.finish_reason = step_result.finish_reason;
    generate_step.usage = step_result.usage;

    // Add step to final result
    final_result.steps.push_back(generate_step);

    // Call on_step_finish callback if provided
    if (initial_options.on_step_finish) {
      initial_options.on_step_finish.value()(generate_step);
    }

    // Accumulate results in final result
    final_result.text += step_result.text;
    final_result.tool_calls.insert(final_result.tool_calls.end(),
                                   step_result.tool_calls.begin(),
                                   step_result.tool_calls.end());
    final_result.tool_results.insert(final_result.tool_results.end(),
                                     step_result.tool_results.begin(),
                                     step_result.tool_results.end());

    // Update usage (accumulate tokens)
    final_result.usage.prompt_tokens += step_result.usage.prompt_tokens;
    final_result.usage.completion_tokens += step_result.usage.completion_tokens;
    final_result.usage.total_tokens += step_result.usage.total_tokens;

    // Update metadata from latest step
    final_result.finish_reason = step_result.finish_reason;
    final_result.id = step_result.id;
    final_result.model = step_result.model;
    final_result.created = step_result.created;
    final_result.system_fingerprint = step_result.system_fingerprint;
    final_result.provider_metadata = step_result.provider_metadata;

    // Accumulate response messages
    final_result.response_messages.insert(final_result.response_messages.end(),
                                          step_result.response_messages.begin(),
                                          step_result.response_messages.end());

    // Check if we should continue
    if (step_result.finish_reason == kFinishReasonStop ||
        step_result.finish_reason == kFinishReasonLength ||
        step_result.finish_reason == kFinishReasonContentFilter ||
        step_result.finish_reason == kFinishReasonError) {
      // Generation is complete
      break;
    }

    if (step_result.finish_reason == kFinishReasonToolCalls &&
        step_result.has_tool_calls()) {
      // Execute tools and prepare for next step
      // Use sequential execution to avoid thread-safety issues
      std::vector<ToolResult> tool_results =
          ToolExecutor::execute_tools_with_options(step_result.tool_calls,
                                                   initial_options, false);

      // Store tool results in the step
      final_result.steps.back().tool_results = tool_results;

      // Check if all tools failed
      bool all_failed = true;
      for (const auto& result : tool_results) {
        if (result.is_success()) {
          all_failed = false;
          break;
        }
      }

      if (all_failed && !tool_results.empty()) {
        // All tools failed, we might want to stop here
        final_result.finish_reason = kFinishReasonError;
        break;
      }

      // Create next step options with tool results (including errors)
      ai::logger::log_debug("Creating next step options with {} tool results", 
                            tool_results.size());
      current_options =
          create_next_step_options(initial_options, step_result, tool_results);
    } else {
      // No tool calls to execute, we're done
      break;
    }
  }

  // Log if we hit the max steps limit
  if (final_result.steps.size() ==
          static_cast<size_t>(initial_options.max_steps) &&
      final_result.finish_reason != kFinishReasonStop) {
    ai::logger::log_debug("Reached max steps limit ({}) without completion",
                          initial_options.max_steps);
  }

  return final_result;
}

GenerateOptions MultiStepCoordinator::create_next_step_options(
    const GenerateOptions& base_options,
    const GenerateResult& previous_result,
    const std::vector<ToolResult>& tool_results) {
  ai::logger::log_debug("create_next_step_options: base messages count={}, tool_results count={}",
                        base_options.messages.size(), tool_results.size());
  
  GenerateOptions next_options = base_options;

  // Build the messages for the next step
  Messages next_messages = next_options.messages;

  // If we started with a simple prompt, convert to messages
  if (!base_options.prompt.empty() && next_messages.empty()) {
    next_messages.push_back(Message::user(base_options.prompt));
  }

  // Add assistant's response with tool calls
  if (previous_result.has_tool_calls()) {
    // Convert ToolCall to ToolCallContent
    std::vector<ToolCallContentPart> tool_call_contents;
    tool_call_contents.reserve(previous_result.tool_calls.size());
    for (const auto& tc : previous_result.tool_calls) {
      tool_call_contents.emplace_back(tc.id, tc.tool_name, tc.arguments);
    }

    // Create assistant message with tool calls
    next_messages.push_back(Message::assistant_with_tools(previous_result.text,
                                                          tool_call_contents));

    // Add tool results as messages
    Messages tool_messages =
        tool_results_to_messages(previous_result.tool_calls, tool_results);
    ai::logger::log_debug("Adding {} tool result messages", tool_messages.size());
    next_messages.insert(next_messages.end(), tool_messages.begin(),
                         tool_messages.end());
  }

  next_options.messages = next_messages;
  next_options.prompt = "";  // Clear prompt since we're using messages
  
  ai::logger::log_debug("Final next_options: messages count={}, system prompt length={}",
                        next_options.messages.size(), next_options.system.length());

  return next_options;
}

Messages MultiStepCoordinator::tool_results_to_messages(
    const std::vector<ToolCall>& tool_calls,
    const std::vector<ToolResult>& tool_results) {
  Messages messages;

  // Create a map for quick lookup
  std::map<std::string, ToolResult> results_by_id;
  for (const auto& result : tool_results) {
    results_by_id[result.tool_call_id] = result;
  }

  // Convert tool results to ToolResultContent
  std::vector<ToolResultContentPart> tool_result_contents;
  for (const auto& tool_call : tool_calls) {
    auto result_it = results_by_id.find(tool_call.id);
    if (result_it != results_by_id.end()) {
      const ToolResult& result = result_it->second;

      if (result.is_success()) {
        tool_result_contents.emplace_back(tool_call.id, result.result, false);
      } else {
        // For errors, create a simple error object
        JsonValue error_json = {{"error", result.error_message()}};
        tool_result_contents.emplace_back(tool_call.id, error_json, true);
      }
    }
  }

  // Create a single user message with all tool results
  if (!tool_result_contents.empty()) {
    messages.push_back(Message::tool_results(tool_result_contents));
  }

  return messages;
}

}  // namespace ai