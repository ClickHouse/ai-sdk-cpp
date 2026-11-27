#include "google_response_parser.h"

#include "ai/logger.h"
#include "google_request_builder.h"
#include "utils/response_utils.h"

#include <utility>

namespace ai {
namespace google {

namespace {

constexpr const char* kEmbeddingsNotSupported =
    "Embeddings are not supported by the Google Gemini client";

std::string string_field(const nlohmann::json& object, const char* key) {
  if (!object.is_object()) {
    return "";
  }
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return "";
  }
  return it->get<std::string>();
}

int int_field(const nlohmann::json& object, const char* key) {
  const auto it = object.find(key);
  return it != object.end() && it->is_number_integer() ? it->get<int>() : 0;
}

}  // namespace

std::optional<FinishReason> parse_finish_reason(const std::string& reason,
                                                bool has_function_calls) {
  if (reason == "STOP") {
    return has_function_calls ? kFinishReasonToolCalls : kFinishReasonStop;
  }
  if (reason == "MAX_TOKENS" || reason == "CONTINUATION") {
    // `CONTINUATION` (long decode on Gemini 4) stops at the per-request
    // output limit; the client does not resume with `continuationToken`.
    return has_function_calls ? kFinishReasonToolCalls : kFinishReasonLength;
  }
  if (reason == "SAFETY" || reason == "RECITATION" || reason == "LANGUAGE" ||
      reason == "BLOCKLIST" || reason == "PROHIBITED_CONTENT" ||
      reason == "SPII" || reason == "IMAGE_SAFETY" ||
      reason == "IMAGE_PROHIBITED_CONTENT" || reason == "IMAGE_RECITATION" ||
      reason == "ESCALATION") {
    return kFinishReasonContentFilter;
  }
  // MALFORMED_FUNCTION_CALL, UNEXPECTED_TOOL_CALL, TOO_MANY_TOOL_CALLS,
  // MISSING_THOUGHT_SIGNATURE, MALFORMED_RESPONSE, OTHER, ... and reasons
  // added after this client was written.
  return std::nullopt;
}

Usage parse_usage(const nlohmann::json& usage_metadata) {
  Usage usage;
  if (!usage_metadata.is_object()) {
    return usage;
  }
  usage.prompt_tokens = int_field(usage_metadata, "promptTokenCount");
  // Thinking tokens are billed as output tokens.
  usage.completion_tokens = int_field(usage_metadata, "candidatesTokenCount") +
                            int_field(usage_metadata, "thoughtsTokenCount");
  usage.total_tokens = int_field(usage_metadata, "totalTokenCount");
  if (usage.total_tokens == 0) {
    usage.total_tokens = usage.prompt_tokens + usage.completion_tokens;
  }
  return usage;
}

std::optional<std::string> prompt_block_error(const nlohmann::json& response) {
  const auto it = response.find("promptFeedback");
  if (it == response.end() || !it->is_object()) {
    return std::nullopt;
  }
  const auto reason = string_field(*it, "blockReason");
  if (reason.empty() || reason == "BLOCK_REASON_UNSPECIFIED") {
    return std::nullopt;
  }
  return "Gemini blocked the prompt (" + reason + ")";
}

std::string synthetic_call_id(std::size_t index) {
  return kSyntheticCallIdPrefix + std::to_string(index);
}

GenerateResult GoogleResponseParser::parse_success_completion_response(
    const nlohmann::json& response) {
  ai::logger::log_debug("Parsing Gemini generateContent response");

  GenerateResult result;
  if (auto id = string_field(response, "responseId"); !id.empty()) {
    result.id = std::move(id);
  }
  if (auto model = string_field(response, "modelVersion"); !model.empty()) {
    result.model = std::move(model);
  }
  if (const auto it = response.find("usageMetadata"); it != response.end()) {
    result.usage = parse_usage(*it);
  }
  result.provider_metadata = response.dump();

  if (auto blocked = prompt_block_error(response)) {
    ai::logger::log_error("{}", *blocked);
    result.error = std::move(blocked);
    result.finish_reason = kFinishReasonContentFilter;
    return result;
  }

  const auto candidates = response.find("candidates");
  if (candidates == response.end() || !candidates->is_array() ||
      candidates->empty() || !(*candidates)[0].is_object()) {
    result.error = "Gemini response has no candidates";
    result.finish_reason = kFinishReasonError;
    return result;
  }
  const auto& candidate = (*candidates)[0];

  // The model turn with its parts in their original order. Thought
  // signatures, thoughts, and parts without a provider-neutral form are kept
  // as `ProviderContentPart`s so the turn can be replayed exactly as the API
  // requires.
  MessageContent turn;
  std::size_t call_index = 0;

  const auto content = candidate.find("content");
  if (content != candidate.end() && content->is_object()) {
    const auto parts = content->find("parts");
    if (parts != content->end() && parts->is_array()) {
      for (const auto& part : *parts) {
        if (!part.is_object()) {
          continue;
        }
        const auto raw_part = [&part]() {
          return ProviderContentPart{kGoogleProviderName,
                                     {{"type", kRawPartType}, {"part", part}}};
        };

        if (const auto thought = part.find("thought"); thought != part.end() &&
                                                       thought->is_boolean() &&
                                                       thought->get<bool>()) {
          // Thought summaries (`includeThoughts`) are not answer text.
          turn.emplace_back(raw_part());
          continue;
        }

        const auto signature = string_field(part, "thoughtSignature");
        const auto add_signature = [&]() {
          if (!signature.empty()) {
            turn.emplace_back(
                ProviderContentPart{kGoogleProviderName,
                                    {{"type", kThoughtSignaturePartType},
                                     {"thoughtSignature", signature}}});
          }
        };

        if (const auto call = part.find("functionCall");
            call != part.end() && call->is_object()) {
          auto id = string_field(*call, "id");
          if (id.empty()) {
            id = synthetic_call_id(call_index);
          }
          ++call_index;
          const auto name = string_field(*call, "name");
          JsonValue arguments = JsonValue::object();
          if (const auto args = call->find("args");
              args != call->end() && args->is_object()) {
            arguments = *args;
          }
          add_signature();
          turn.emplace_back(ToolCallContentPart{id, name, arguments});
          result.tool_calls.emplace_back(id, name, std::move(arguments));
          ai::logger::log_debug("Parsed Gemini function call: {}", name);
        } else if (auto text = string_field(part, "text"); !text.empty()) {
          add_signature();
          result.text += text;
          turn.emplace_back(TextContentPart{std::move(text)});
        } else {
          // An empty text part that only carries the signature, or a part
          // kind this client does not interpret.
          turn.emplace_back(raw_part());
        }
      }
    }
  }

  const auto reason = string_field(candidate, "finishReason");
  const auto finish_reason =
      parse_finish_reason(reason, !result.tool_calls.empty());
  if (!finish_reason) {
    std::string error =
        "Gemini response finished with " +
        (reason.empty() ? std::string("no finishReason") : reason);
    if (const auto message = string_field(candidate, "finishMessage");
        !message.empty()) {
      error += ": " + message;
    }
    ai::logger::log_error("{}", error);
    result.error = std::move(error);
    result.finish_reason = kFinishReasonError;
    return result;
  }
  result.finish_reason = *finish_reason;

  // A turn of thoughts alone is not kept: there is nothing to answer.
  Message message(kMessageRoleAssistant, std::move(turn));
  if (message.has_text() || message.has_tool_calls()) {
    result.response_messages.push_back(std::move(message));
  }

  ai::logger::log_debug(
      "Parsed Gemini response - text length: {}, function calls: {}, "
      "finish reason: {}",
      result.text.length(), result.tool_calls.size(), reason);
  return result;
}

GenerateResult GoogleResponseParser::parse_error_completion_response(
    int status_code,
    const std::string& body) {
  return utils::parse_standard_error_response("Google", status_code, body);
}

EmbeddingResult GoogleResponseParser::parse_success_embedding_response(
    const nlohmann::json& response) {
  (void)response;
  return EmbeddingResult(std::string(kEmbeddingsNotSupported));
}

EmbeddingResult GoogleResponseParser::parse_error_embedding_response(
    int status_code,
    const std::string& body) {
  auto generate_result =
      utils::parse_standard_error_response("Google", status_code, body);
  return EmbeddingResult(generate_result.error);
}

}  // namespace google
}  // namespace ai
