#include "bedrock_request_mapper.h"

#include "ai/logger.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

#include <aws/bedrock-runtime/model/AnyToolChoice.h>
#include <aws/bedrock-runtime/model/AutoToolChoice.h>
#include <aws/bedrock-runtime/model/SpecificToolChoice.h>
#include <aws/bedrock-runtime/model/Tool.h>
#include <aws/bedrock-runtime/model/ToolChoice.h>
#include <aws/bedrock-runtime/model/ToolInputSchema.h>
#include <aws/bedrock-runtime/model/ToolResultBlock.h>
#include <aws/bedrock-runtime/model/ToolResultContentBlock.h>
#include <aws/bedrock-runtime/model/ToolSpecification.h>
#include <aws/bedrock-runtime/model/ToolUseBlock.h>

namespace ai {
namespace bedrock {

namespace Model = Aws::BedrockRuntime::Model;

namespace {

// Claude models that reject `temperature` and `top_p`, as first-party model ID
// prefixes. Mirrors `kSamplingRejectingModelPrefixes` in
// `anthropic_request_builder.cpp`, plus Claude Haiku 5.
constexpr std::array<std::string_view, 6> kSamplingRejectingClaudePrefixes = {
    "claude-opus-5",  "claude-opus-4-8", "claude-opus-4-7",
    "claude-fable-5", "claude-sonnet-5", "claude-haiku-5"};

constexpr std::string_view kAnthropicModelPrefix = "anthropic.";

bool messages_use_tools(const Messages& messages) {
  return std::any_of(messages.begin(), messages.end(), [](const Message& m) {
    return m.has_tool_calls() || m.has_tool_results();
  });
}

}  // namespace

bool BedrockRequestMapper::rejects_sampling_parameters(
    const std::string& model_id) {
  // Bedrock IDs look like `anthropic.claude-opus-5-5`, optionally with a
  // cross-region prefix (`us.`, `global.`) or inside an inference profile ARN.
  const auto pos =
      model_id.find(std::string(kAnthropicModelPrefix) + "claude-");
  if (pos == std::string::npos) {
    return false;
  }
  const std::string_view first_party_id =
      std::string_view(model_id).substr(pos + kAnthropicModelPrefix.size());
  return std::any_of(kSamplingRejectingClaudePrefixes.begin(),
                     kSamplingRejectingClaudePrefixes.end(),
                     [&](std::string_view prefix) {
                       return first_party_id.starts_with(prefix);
                     });
}

Aws::Utils::Document BedrockRequestMapper::to_document(
    const nlohmann::json& value) {
  Aws::Utils::Document document(Aws::String(value.dump()));
  if (!document.WasParseSuccessful()) {
    throw std::invalid_argument("Failed to convert JSON to AWS document: " +
                                std::string(document.GetErrorMessage()));
  }
  return document;
}

nlohmann::json BedrockRequestMapper::from_document(
    const Aws::Utils::DocumentView& document) {
  const Aws::String compact = document.WriteCompact();
  if (compact.empty()) {
    return nlohmann::json::object();
  }
  return nlohmann::json::parse(compact);
}

Model::ConverseRequest BedrockRequestMapper::map_to_converse_request(
    const GenerateOptions& options) const {
  Model::ConverseRequest request;
  fill_request(request, options);
  return request;
}

Model::ConverseStreamRequest
BedrockRequestMapper::map_to_converse_stream_request(
    const GenerateOptions& options) const {
  Model::ConverseStreamRequest request;
  fill_request(request, options);
  return request;
}

template <typename Request>
void BedrockRequestMapper::fill_request(Request& request,
                                        const GenerateOptions& options) const {
  request.SetModelId(options.model);

  // Bedrock takes system prompts separately from the conversation: the
  // `system` option and every system message become system content blocks.
  if (!options.system.empty()) {
    Model::SystemContentBlock block;
    block.SetText(options.system);
    request.AddSystem(std::move(block));
  }
  for (const auto& message : options.messages) {
    if (message.role == kMessageRoleSystem) {
      const std::string text = message.get_text();
      if (!text.empty()) {
        Model::SystemContentBlock block;
        block.SetText(text);
        request.AddSystem(std::move(block));
      }
    }
  }

  // The conversation: the non-system messages, or the prompt if there are
  // none (e.g. only system messages were given)
  auto messages = convert_messages(options.messages);
  if (messages.empty() && !options.prompt.empty()) {
    Model::Message user_message;
    user_message.SetRole(Model::ConversationRole::user);
    Model::ContentBlock content_block;
    content_block.SetText(options.prompt);
    user_message.AddContent(std::move(content_block));
    messages.push_back(std::move(user_message));
  }
  request.SetMessages(std::move(messages));

  request.SetInferenceConfig(build_inference_config(options));

  if (auto tool_config = build_tool_config(options)) {
    request.SetToolConfig(std::move(*tool_config));
  }
}

Aws::Vector<Model::Message> BedrockRequestMapper::convert_messages(
    const Messages& messages) const {
  Aws::Vector<Model::Message> bedrock_messages;
  bedrock_messages.reserve(messages.size());

  for (const auto& message : messages) {
    // System messages are sent as system content blocks (see fill_request)
    if (message.role == kMessageRoleSystem) {
      continue;
    }
    auto converted = convert_message(message);
    if (!converted) {
      continue;
    }
    // Converse requires alternating roles; dropping system (or empty)
    // messages can leave two turns of the same role next to each other, so
    // merge such turns (as the Anthropic API does itself)
    if (!bedrock_messages.empty() &&
        bedrock_messages.back().GetRole() == converted->GetRole()) {
      for (auto& block : converted->GetContent()) {
        bedrock_messages.back().AddContent(block);
      }
    } else {
      bedrock_messages.push_back(std::move(*converted));
    }
  }

  return bedrock_messages;
}

std::optional<Model::Message> BedrockRequestMapper::convert_message(
    const Message& message) const {
  Model::Message bedrock_message;

  // Bedrock expects tool results in user messages
  if (message.role == kMessageRoleAssistant && !message.has_tool_results()) {
    bedrock_message.SetRole(Model::ConversationRole::assistant);
  } else {
    bedrock_message.SetRole(Model::ConversationRole::user);
  }

  for (const auto& part : message.content) {
    Model::ContentBlock content_block;

    if (const auto* text_part = std::get_if<TextContentPart>(&part)) {
      if (text_part->text.empty()) {
        continue;  // Bedrock rejects empty text blocks
      }
      content_block.SetText(text_part->text);
    } else if (const auto* tool_call =
                   std::get_if<ToolCallContentPart>(&part)) {
      Model::ToolUseBlock tool_use;
      tool_use.SetToolUseId(tool_call->id);
      tool_use.SetName(tool_call->tool_name);
      tool_use.SetInput(to_document(tool_call->arguments.is_null()
                                        ? nlohmann::json::object()
                                        : tool_call->arguments));
      content_block.SetToolUse(std::move(tool_use));
    } else if (const auto* tool_result =
                   std::get_if<ToolResultContentPart>(&part)) {
      Model::ToolResultContentBlock result_content;
      // Bedrock accepts only JSON objects as `json` tool results
      if (tool_result->result.is_object()) {
        result_content.SetJson(to_document(tool_result->result));
      } else if (tool_result->result.is_string()) {
        result_content.SetText(tool_result->result.get<std::string>());
      } else {
        result_content.SetText(tool_result->result.dump());
      }

      Model::ToolResultBlock result_block;
      result_block.SetToolUseId(tool_result->tool_call_id);
      result_block.AddContent(std::move(result_content));
      result_block.SetStatus(tool_result->is_error
                                 ? Model::ToolResultStatus::error
                                 : Model::ToolResultStatus::success);
      content_block.SetToolResult(std::move(result_block));
    } else {
      continue;
    }

    bedrock_message.AddContent(std::move(content_block));
  }

  if (bedrock_message.GetContent().empty()) {
    return std::nullopt;  // Bedrock rejects messages without content
  }
  return bedrock_message;
}

Model::InferenceConfiguration BedrockRequestMapper::build_inference_config(
    const GenerateOptions& options) const {
  Model::InferenceConfiguration config;

  if (options.max_tokens.has_value()) {
    config.SetMaxTokens(options.max_tokens.value());
  }

  const bool drop_sampling = rejects_sampling_parameters(options.model);
  const auto set_sampling =
      [&](const char* name, const std::optional<double>& value, auto setter) {
        if (!value) {
          return;
        }
        if (drop_sampling) {
          ai::logger::log_warn(
              "Ignoring {} for {} because the model does not support sampling "
              "parameters",
              name, options.model);
          return;
        }
        setter(static_cast<float>(*value));
      };
  set_sampling("temperature", options.temperature,
               [&](float v) { config.SetTemperature(v); });
  set_sampling("top_p", options.top_p, [&](float v) { config.SetTopP(v); });

  return config;
}

std::optional<Model::ToolConfiguration> BedrockRequestMapper::build_tool_config(
    const GenerateOptions& options) const {
  if (!options.has_tools()) {
    return std::nullopt;
  }

  // Converse has no "none" tool choice. Omit the tools, unless the
  // conversation already contains tool calls: Bedrock requires a tool
  // configuration for requests with `toolUse` / `toolResult` blocks.
  const bool tools_disabled = options.tool_choice.type == ToolChoiceType::kNone;
  if (tools_disabled && !messages_use_tools(options.messages)) {
    return std::nullopt;
  }

  Model::ToolConfiguration tool_config;
  for (const auto& tool_name : options.get_active_tool_names()) {
    const auto it = options.tools.find(tool_name);
    if (it == options.tools.end()) {
      continue;
    }

    Model::ToolInputSchema input_schema;
    input_schema.SetJson(to_document(it->second.parameters_schema.is_null()
                                         ? nlohmann::json{{"type", "object"}}
                                         : it->second.parameters_schema));

    Model::ToolSpecification spec;
    spec.SetName(tool_name);
    if (!it->second.description.empty()) {
      spec.SetDescription(it->second.description);
    }
    spec.SetInputSchema(std::move(input_schema));

    Model::Tool tool;
    tool.SetToolSpec(std::move(spec));
    tool_config.AddTools(std::move(tool));
  }

  if (tool_config.GetTools().empty()) {
    return std::nullopt;
  }

  Model::ToolChoice tool_choice;
  switch (options.tool_choice.type) {
    case ToolChoiceType::kRequired:
      tool_choice.SetAny(Model::AnyToolChoice());
      tool_config.SetToolChoice(std::move(tool_choice));
      break;
    case ToolChoiceType::kSpecific:
      if (options.tool_choice.tool_name) {
        Model::SpecificToolChoice specific;
        specific.SetName(*options.tool_choice.tool_name);
        tool_choice.SetTool(std::move(specific));
        tool_config.SetToolChoice(std::move(tool_choice));
      }
      break;
    case ToolChoiceType::kAuto:
      tool_choice.SetAuto(Model::AutoToolChoice());
      tool_config.SetToolChoice(std::move(tool_choice));
      break;
    case ToolChoiceType::kNone:
      ai::logger::log_warn(
          "Bedrock Converse has no \"none\" tool choice; sending the tools "
          "because the conversation contains tool calls");
      break;
  }

  return tool_config;
}

}  // namespace bedrock
}  // namespace ai
