#include "ai/tools.h"
#include "ai/types/enums.h"
#include "ai/types/generate_options.h"
#include "ai/types/message.h"
#include "ai/types/tool.h"

#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace ai {
namespace test {

namespace {

// Collect the ids of all tool calls contained in a message list.
std::vector<std::string> collect_tool_call_ids(const Messages& messages) {
  std::vector<std::string> ids;
  for (const auto& message : messages) {
    for (const auto& part : message.content) {
      if (const auto* tool_call = std::get_if<ToolCallContentPart>(&part)) {
        ids.push_back(tool_call->id);
      }
    }
  }
  return ids;
}

int count_tool_result_messages(const Messages& messages) {
  int count = 0;
  for (const auto& message : messages) {
    if (message.has_tool_results()) {
      ++count;
    }
  }
  return count;
}

}  // namespace

// Regression test: every step of a multi-step generation must see the tool
// calls and tool results of ALL earlier steps, not only of the latest one.
// When earlier exchanges were dropped, the model had no way to remember what
// its tools had already returned, so it kept re-requesting the same tool
// calls until max_steps was exhausted, without producing a final answer.
TEST(MultiStepCoordinatorTest, ConversationAccumulatesAcrossSteps) {
  GenerateOptions options;
  options.model = "test-model";
  options.system = "system prompt";
  options.prompt = "user prompt";
  options.max_steps = 5;

  // The messages of every request the coordinator sends to the model.
  std::vector<Messages> request_messages;

  auto generate_func = [&](const GenerateOptions& opts) -> GenerateResult {
    request_messages.push_back(opts.messages);
    int step = static_cast<int>(request_messages.size());

    GenerateResult result;
    if (step <= 2) {
      // Two steps of tool calling with distinct tools.
      std::string call_id = "call_" + std::to_string(step);
      std::string tool_name = step == 1 ? "get_a" : "get_b";
      result.finish_reason = kFinishReasonToolCalls;
      result.tool_calls.emplace_back(call_id, tool_name,
                                     JsonValue{{"step", step}});
      result.tool_results.emplace_back(call_id, tool_name,
                                       JsonValue{{"step", step}},
                                       JsonValue{{"value", step}});
      return result;
    }
    result.text = "final answer";
    result.finish_reason = kFinishReasonStop;
    return result;
  };

  GenerateResult result =
      MultiStepCoordinator::execute_multi_step(options, generate_func);

  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.finish_reason, kFinishReasonStop);
  EXPECT_EQ(result.text, "final answer");
  ASSERT_EQ(request_messages.size(), 3u);

  // The third request must still contain the tool exchange of the first
  // step, not only the one of the second step.
  const auto ids = collect_tool_call_ids(request_messages[2]);
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], "call_1");
  EXPECT_EQ(ids[1], "call_2");
  EXPECT_EQ(count_tool_result_messages(request_messages[2]), 2);

  // The conversation grows with every step.
  EXPECT_LT(request_messages[0].size(), request_messages[1].size());
  EXPECT_LT(request_messages[1].size(), request_messages[2].size());
}

}  // namespace test
}  // namespace ai
