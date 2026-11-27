#pragma once

#include "ai/types/generate_options.h"

#include <optional>
#include <string>

#include <aws/bedrock-runtime/model/ContentBlock.h>
#include <aws/bedrock-runtime/model/ConversationRole.h>
#include <aws/bedrock-runtime/model/ConverseRequest.h>
#include <aws/bedrock-runtime/model/ConverseStreamRequest.h>
#include <aws/bedrock-runtime/model/InferenceConfiguration.h>
#include <aws/bedrock-runtime/model/Message.h>
#include <aws/bedrock-runtime/model/SystemContentBlock.h>
#include <aws/bedrock-runtime/model/ToolConfiguration.h>
#include <aws/core/utils/Document.h>

namespace ai {
namespace bedrock {

/// Maps SDK options to Bedrock Converse / ConverseStream requests
class BedrockRequestMapper {
 public:
  Aws::BedrockRuntime::Model::ConverseRequest map_to_converse_request(
      const GenerateOptions& options) const;

  Aws::BedrockRuntime::Model::ConverseStreamRequest
  map_to_converse_stream_request(const GenerateOptions& options) const;

  /// Whether the model rejects sampling parameters (`temperature`, `top_p`).
  /// Recognizes model IDs with or without a cross-region inference profile
  /// prefix (e.g. `us.anthropic.claude-opus-5-5`).
  static bool rejects_sampling_parameters(const std::string& model_id);

  /// Convert JSON to an AWS document (throws std::invalid_argument on failure)
  static Aws::Utils::Document to_document(const nlohmann::json& value);

  /// Convert an AWS document to JSON
  static nlohmann::json from_document(const Aws::Utils::DocumentView& document);
  static nlohmann::json from_document(const Aws::Utils::Document& document) {
    return from_document(document.View());
  }

 private:
  template <typename Request>
  void fill_request(Request& request, const GenerateOptions& options) const;

  Aws::Vector<Aws::BedrockRuntime::Model::Message> convert_messages(
      const Messages& messages) const;

  std::optional<Aws::BedrockRuntime::Model::Message> convert_message(
      const Message& message) const;

  Aws::BedrockRuntime::Model::InferenceConfiguration build_inference_config(
      const GenerateOptions& options) const;

  std::optional<Aws::BedrockRuntime::Model::ToolConfiguration>
  build_tool_config(const GenerateOptions& options) const;
};

}  // namespace bedrock
}  // namespace ai
