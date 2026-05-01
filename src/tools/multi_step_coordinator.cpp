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

  // initial_messages is the user's original input, kept immutable across
  // steps; response_messages accumulates assistant turns and tool-result
  // turns. Each step's input is [initial_messages..., response_messages...].
  // system and other top-level options stay on initial_options.
  Messages initial_messages = initial_options.messages;
  if (initial_messages.empty() && !initial_options.prompt.empty()) {
    initial_messages.push_back(Message::user(initial_options.prompt));
  }
  const size_t initial_count = initial_messages.size();
  Messages response_messages;

  // step_messages is grown in place each iteration (truncate to the immutable
  // prefix, then append response_messages) so we don't re-copy
  // initial_messages every step.
  Messages step_messages = std::move(initial_messages);
  GenerateOptions step_options = initial_options;
  step_options.prompt.clear();

  GenerateResult final_result;

  for (int step = 0; step < initial_options.max_steps; ++step) {
    // Truncate to the immutable prefix and re-append the running accumulator.
    // (vector::resize would require Message to be default-constructible.)
    step_messages.erase(std::next(step_messages.begin(), initial_count),
                        step_messages.end());
    step_messages.insert(step_messages.end(), response_messages.begin(),
                         response_messages.end());
    step_options.messages = step_messages;

    ai::logger::log_debug("Executing step {} of {}, messages={}", step + 1,
                          initial_options.max_steps,
                          step_options.messages.size());

    GenerateResult step_result = generate_func(step_options);

    ai::logger::log_debug(
        "Step {} result - text: '{}', tool_calls: {}, finish_reason: {}",
        step + 1, step_result.text, step_result.tool_calls.size(),
        static_cast<int>(step_result.finish_reason));

    if (!step_result.is_success()) {
      if (step == 0) {
        return step_result;
      }
      final_result.error = step_result.error;
      break;
    }

    // Record the per-step view exposed via `final_result.steps` and the
    // `on_step_finish` callback.
    GenerateStep generate_step;
    generate_step.text = step_result.text;
    generate_step.tool_calls = step_result.tool_calls;
    generate_step.tool_results = step_result.tool_results;
    generate_step.finish_reason = step_result.finish_reason;
    generate_step.usage = step_result.usage;
    final_result.steps.push_back(generate_step);

    if (initial_options.on_step_finish) {
      initial_options.on_step_finish.value()(generate_step);
    }

    // Roll up cumulative state on the final result.
    final_result.text += step_result.text;
    final_result.tool_calls.insert(final_result.tool_calls.end(),
                                   step_result.tool_calls.begin(),
                                   step_result.tool_calls.end());
    final_result.tool_results.insert(final_result.tool_results.end(),
                                     step_result.tool_results.begin(),
                                     step_result.tool_results.end());
    final_result.usage.prompt_tokens += step_result.usage.prompt_tokens;
    final_result.usage.completion_tokens += step_result.usage.completion_tokens;
    final_result.usage.total_tokens += step_result.usage.total_tokens;
    final_result.finish_reason = step_result.finish_reason;
    final_result.id = step_result.id;
    final_result.model = step_result.model;
    final_result.created = step_result.created;
    final_result.system_fingerprint = step_result.system_fingerprint;
    final_result.provider_metadata = step_result.provider_metadata;

    // Termination conditions identical to before.
    if (step_result.finish_reason == kFinishReasonStop ||
        step_result.finish_reason == kFinishReasonLength ||
        step_result.finish_reason == kFinishReasonContentFilter ||
        step_result.finish_reason == kFinishReasonError) {
      break;
    }

    if (step_result.finish_reason == kFinishReasonToolCalls &&
        step_result.has_tool_calls()) {
      const std::vector<ToolResult>& tool_results = step_result.tool_results;

      bool all_failed = true;
      for (const auto& result : tool_results) {
        if (result.is_success()) {
          all_failed = false;
          break;
        }
      }
      if (all_failed && !tool_results.empty()) {
        final_result.finish_reason = kFinishReasonError;
        break;
      }

      // Append assistant turn (text + tool calls) and tool result messages to
      // the running accumulator. Next iteration's input messages are rebuilt
      // from `initial_messages + response_messages` at the top of the loop.
      std::vector<ToolCallContentPart> tool_call_contents;
      tool_call_contents.reserve(step_result.tool_calls.size());
      for (const auto& tc : step_result.tool_calls) {
        tool_call_contents.emplace_back(tc.id, tc.tool_name, tc.arguments);
      }
      response_messages.push_back(
          Message::assistant_with_tools(step_result.text, tool_call_contents));

      Messages tool_messages =
          tool_results_to_messages(step_result.tool_calls, tool_results);
      response_messages.insert(response_messages.end(), tool_messages.begin(),
                               tool_messages.end());
    } else {
      // No tool calls and a non-terminal finish reason: nothing to feed back,
      // so we're done.
      break;
    }
  }

  // Surface the accumulated assistant/tool messages on the result for callers
  // that want to continue the conversation.
  final_result.response_messages.insert(final_result.response_messages.end(),
                                        response_messages.begin(),
                                        response_messages.end());

  if (final_result.steps.size() ==
          static_cast<size_t>(initial_options.max_steps) &&
      final_result.finish_reason != kFinishReasonStop) {
    ai::logger::log_debug("Reached max steps limit ({}) without completion",
                          initial_options.max_steps);
  }

  return final_result;
}

Messages MultiStepCoordinator::tool_results_to_messages(
    const std::vector<ToolCall>& tool_calls,
    const std::vector<ToolResult>& tool_results) {
  Messages messages;

  std::map<std::string, ToolResult> results_by_id;
  for (const auto& result : tool_results) {
    results_by_id[result.tool_call_id] = result;
  }

  std::vector<ToolResultContentPart> tool_result_contents;
  for (const auto& tool_call : tool_calls) {
    auto result_it = results_by_id.find(tool_call.id);
    if (result_it != results_by_id.end()) {
      const ToolResult& result = result_it->second;
      if (result.is_success()) {
        tool_result_contents.emplace_back(tool_call.id, result.result, false);
      } else {
        JsonValue error_json = {{"error", result.error_message()}};
        tool_result_contents.emplace_back(tool_call.id, error_json, true);
      }
    }
  }

  if (!tool_result_contents.empty()) {
    messages.push_back(Message::tool_results(tool_result_contents));
  }

  return messages;
}

}  // namespace ai
