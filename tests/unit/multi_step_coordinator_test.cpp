#include "ai/tools.h"
#include "ai/types/enums.h"
#include "ai/types/generate_options.h"

#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace ai {
namespace test {

namespace {

GenerateResult make_tool_call_step() {
  GenerateResult result;
  result.finish_reason = kFinishReasonToolCalls;
  result.tool_calls.emplace_back("call_1", "add",
                                 JsonValue{{"a", 3}, {"b", 5}});
  result.tool_results.emplace_back(
      "call_1", "add", JsonValue{{"a", 3}, {"b", 5}}, JsonValue{{"sum", 8}});
  return result;
}

GenerateResult make_stop_step(const std::string& text) {
  GenerateResult result;
  result.text = text;
  result.finish_reason = kFinishReasonStop;
  return result;
}

GenerateOptions make_options() {
  GenerateOptions options("test-model", "What is 3 + 5?");
  options.max_steps = 4;
  return options;
}

}  // namespace

// Regression test: the terminal assistant reply used to be dropped from
// response_messages because only the tool-call feedback path appended to the
// accumulator, so callers continuing the conversation lost the final answer.
TEST(MultiStepCoordinatorTest, TerminalReplyRecordedAfterToolStep) {
  std::vector<GenerateResult> scripted = {make_tool_call_step(),
                                          make_stop_step("The result is 8")};
  std::size_t call = 0;

  auto result = MultiStepCoordinator::execute_multi_step(
      make_options(), [&](const GenerateOptions&) { return scripted[call++]; });

  ASSERT_EQ(result.steps.size(), 2u);
  ASSERT_EQ(result.response_messages.size(), 3u);
  const auto& reply = result.response_messages.back();
  EXPECT_EQ(reply.role, kMessageRoleAssistant);
  EXPECT_EQ(reply.get_text(), "The result is 8");
}

TEST(MultiStepCoordinatorTest, ImmediateStopRecordsAssistantReply) {
  auto result = MultiStepCoordinator::execute_multi_step(
      make_options(),
      [](const GenerateOptions&) { return make_stop_step("Hello"); });

  ASSERT_EQ(result.response_messages.size(), 1u);
  EXPECT_EQ(result.response_messages.back().role, kMessageRoleAssistant);
  EXPECT_EQ(result.response_messages.back().get_text(), "Hello");
}

TEST(MultiStepCoordinatorTest, EmptyTerminalTextAppendsNoMessage) {
  std::vector<GenerateResult> scripted = {make_tool_call_step(),
                                          make_stop_step("")};
  std::size_t call = 0;

  auto result = MultiStepCoordinator::execute_multi_step(
      make_options(), [&](const GenerateOptions&) { return scripted[call++]; });

  // The assistant tool-call turn and the tool results stay, but no empty
  // assistant reply is appended.
  ASSERT_EQ(result.response_messages.size(), 2u);
  EXPECT_NE(result.response_messages.back().role, kMessageRoleAssistant);
}

}  // namespace test
}  // namespace ai
