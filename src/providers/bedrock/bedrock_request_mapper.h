#pragma once

#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

// Forward declare AWS types to avoid including BedrockRuntimeClient.h
// which has a broken dependency on Paginator.h in homebrew's AWS SDK
#include <aws/bedrock-runtime/model/ContentBlock.h>
#include <aws/bedrock-runtime/model/ConversationRole.h>
#include <aws/bedrock-runtime/model/ConverseRequest.h>
#include <aws/bedrock-runtime/model/ConverseStreamRequest.h>
#include <aws/bedrock-runtime/model/InferenceConfiguration.h>
#include <aws/bedrock-runtime/model/Message.h>
#include <aws/bedrock-runtime/model/SystemContentBlock.h>

namespace ai {
namespace bedrock {

/// Maps SDK types to AWS Bedrock Converse API request types
class BedrockRequestMapper {
 public:
  BedrockRequestMapper() = default;
  ~BedrockRequestMapper() = default;

  /// Convert GenerateOptions to Converse request
  /// @param options SDK generate options
  /// @return AWS Bedrock ConverseRequest
  Aws::BedrockRuntime::Model::ConverseRequest map_to_converse_request(
      const GenerateOptions& options);

  /// Convert GenerateOptions to ConverseStream request
  /// @param options SDK generate options (from StreamOptions)
  /// @return AWS Bedrock ConverseStreamRequest
  Aws::BedrockRuntime::Model::ConverseStreamRequest
  map_to_converse_stream_request(const GenerateOptions& options);

 private:
  /// Convert SDK messages to Bedrock message format
  /// @param messages SDK messages
  /// @return Vector of Bedrock messages
  Aws::Vector<Aws::BedrockRuntime::Model::Message> convert_messages(
      const Messages& messages);

  /// Convert a single SDK message to Bedrock format
  /// @param message SDK message
  /// @return Bedrock message
  Aws::BedrockRuntime::Model::Message convert_message(const Message& message);

  /// Build inference configuration from options
  /// @param options SDK generate options
  /// @return Bedrock inference configuration
  Aws::BedrockRuntime::Model::InferenceConfiguration build_inference_config(
      const GenerateOptions& options);

  /// Convert SDK MessageRole to Bedrock ConversationRole
  /// @param role SDK message role
  /// @return Bedrock conversation role
  Aws::BedrockRuntime::Model::ConversationRole convert_role(MessageRole role);
};

}  // namespace bedrock
}  // namespace ai
