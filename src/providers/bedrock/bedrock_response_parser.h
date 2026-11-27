#pragma once

#include "ai/types/generate_options.h"

#include <aws/bedrock-runtime/BedrockRuntimeErrors.h>
#include <aws/bedrock-runtime/model/ContentBlock.h>
#include <aws/bedrock-runtime/model/ConverseResult.h>
#include <aws/bedrock-runtime/model/StopReason.h>
#include <aws/core/utils/memory/stl/AWSVector.h>

namespace ai {
namespace bedrock {

/// Parses AWS Bedrock Converse API responses to SDK types
class BedrockResponseParser {
 public:
  BedrockResponseParser() = default;
  ~BedrockResponseParser() = default;

  /// Parse successful Converse response
  /// @param result AWS Bedrock ConverseResult
  /// @return SDK GenerateResult
  GenerateResult parse_converse_response(
      const Aws::BedrockRuntime::Model::ConverseResult& result);

  /// Parse Converse error into GenerateResult
  /// @param error AWS Bedrock error
  /// @return SDK GenerateResult with error information
  GenerateResult parse_converse_error(
      const Aws::BedrockRuntime::BedrockRuntimeError& error);

  /// Determine if an error is retryable
  /// @param error AWS Bedrock error
  /// @return true if the error is retryable
  bool is_error_retryable(
      const Aws::BedrockRuntime::BedrockRuntimeError& error);

 private:
  /// Map Bedrock stop reason to SDK FinishReason
  /// @param reason Bedrock stop reason
  /// @return SDK FinishReason
  FinishReason map_stop_reason(Aws::BedrockRuntime::Model::StopReason reason);

  /// Extract text from content blocks
  /// @param blocks Vector of Bedrock content blocks
  /// @return Concatenated text from all blocks
  std::string extract_text(
      const Aws::Vector<Aws::BedrockRuntime::Model::ContentBlock>& blocks);
};

}  // namespace bedrock
}  // namespace ai
