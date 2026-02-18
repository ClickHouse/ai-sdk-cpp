#include "bedrock_request_mapper.h"

#include "ai/logger.h"

namespace ai {
namespace bedrock {

Aws::BedrockRuntime::Model::ConverseRequest
BedrockRequestMapper::map_to_converse_request(const GenerateOptions& options) {
  Aws::BedrockRuntime::Model::ConverseRequest request;

  // Set model ID
  request.SetModelId(options.model);

  // Handle system prompt
  if (!options.system.empty()) {
    Aws::BedrockRuntime::Model::SystemContentBlock system_block;
    system_block.SetText(options.system);
    request.AddSystem(std::move(system_block));
  }

  // Handle messages vs prompt
  if (!options.messages.empty()) {
    // Use provided messages
    request.SetMessages(convert_messages(options.messages));
  } else if (!options.prompt.empty()) {
    // Convert prompt to single user message
    Aws::BedrockRuntime::Model::Message user_message;
    user_message.SetRole(Aws::BedrockRuntime::Model::ConversationRole::user);

    Aws::BedrockRuntime::Model::ContentBlock content_block;
    content_block.SetText(options.prompt);
    user_message.AddContent(std::move(content_block));

    request.AddMessages(std::move(user_message));
  }

  // Build and set inference configuration
  auto inference_config = build_inference_config(options);
  request.SetInferenceConfig(std::move(inference_config));

  return request;
}

Aws::BedrockRuntime::Model::ConverseStreamRequest
BedrockRequestMapper::map_to_converse_stream_request(
    const GenerateOptions& options) {
  Aws::BedrockRuntime::Model::ConverseStreamRequest request;

  // Set model ID
  request.SetModelId(options.model);

  // Handle system prompt
  if (!options.system.empty()) {
    Aws::BedrockRuntime::Model::SystemContentBlock system_block;
    system_block.SetText(options.system);
    request.AddSystem(std::move(system_block));
  }

  // Handle messages vs prompt
  if (!options.messages.empty()) {
    // Use provided messages
    request.SetMessages(convert_messages(options.messages));
  } else if (!options.prompt.empty()) {
    // Convert prompt to single user message
    Aws::BedrockRuntime::Model::Message user_message;
    user_message.SetRole(Aws::BedrockRuntime::Model::ConversationRole::user);

    Aws::BedrockRuntime::Model::ContentBlock content_block;
    content_block.SetText(options.prompt);
    user_message.AddContent(std::move(content_block));

    request.AddMessages(std::move(user_message));
  }

  // Build and set inference configuration
  auto inference_config = build_inference_config(options);
  request.SetInferenceConfig(std::move(inference_config));

  return request;
}

Aws::Vector<Aws::BedrockRuntime::Model::Message>
BedrockRequestMapper::convert_messages(const Messages& messages) {
  Aws::Vector<Aws::BedrockRuntime::Model::Message> bedrock_messages;
  bedrock_messages.reserve(messages.size());

  for (const auto& msg : messages) {
    // Skip system messages - they should be handled via the system parameter
    if (msg.role == kMessageRoleSystem) {
      continue;
    }
    bedrock_messages.push_back(convert_message(msg));
  }

  return bedrock_messages;
}

Aws::BedrockRuntime::Model::Message BedrockRequestMapper::convert_message(
    const Message& message) {
  Aws::BedrockRuntime::Model::Message bedrock_message;

  // Set role
  bedrock_message.SetRole(convert_role(message.role));

  // Convert content parts to Bedrock content blocks
  for (const auto& part : message.content) {
    if (const auto* text_part = std::get_if<TextContentPart>(&part)) {
      Aws::BedrockRuntime::Model::ContentBlock content_block;
      content_block.SetText(text_part->text);
      bedrock_message.AddContent(std::move(content_block));
    }
    // TODO: Tool calls and tool results are not supported in V1
  }

  return bedrock_message;
}

Aws::BedrockRuntime::Model::InferenceConfiguration
BedrockRequestMapper::build_inference_config(const GenerateOptions& options) {
  Aws::BedrockRuntime::Model::InferenceConfiguration config;

  // Set max_tokens
  if (options.max_tokens.has_value()) {
    config.SetMaxTokens(options.max_tokens.value());
  }

  // Set temperature (range 0.0-1.0)
  if (options.temperature.has_value()) {
    config.SetTemperature(static_cast<float>(options.temperature.value()));
  }

  // Set top_p (range 0.0-1.0)
  if (options.top_p.has_value()) {
    config.SetTopP(static_cast<float>(options.top_p.value()));
  }

  return config;
}

Aws::BedrockRuntime::Model::ConversationRole BedrockRequestMapper::convert_role(
    MessageRole role) {
  switch (role) {
    case kMessageRoleUser:
      return Aws::BedrockRuntime::Model::ConversationRole::user;
    case kMessageRoleAssistant:
      return Aws::BedrockRuntime::Model::ConversationRole::assistant;
    case kMessageRoleSystem:
      // System messages should be handled separately via the system parameter
      // If we get here, treat as user (shouldn't happen in normal flow)
      return Aws::BedrockRuntime::Model::ConversationRole::user;
    default:
      return Aws::BedrockRuntime::Model::ConversationRole::user;
  }
}

}  // namespace bedrock
}  // namespace ai
