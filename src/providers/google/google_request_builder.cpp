#include "google_request_builder.h"

#include "ai/logger.h"

#include <cctype>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace ai {
namespace google {

namespace {

constexpr std::string_view kModelsPrefix = "models/";

bool is_synthetic_call_id(const std::string& id) {
  return id.starts_with(kSyntheticCallIdPrefix);
}

/// Parse the leading "<major>[.<minor>]" of a "gemini-…" model name.
std::optional<std::pair<int, int>> gemini_version(std::string_view model) {
  if (model.starts_with(kModelsPrefix)) {
    model.remove_prefix(kModelsPrefix.size());
  }
  constexpr std::string_view kGeminiPrefix = "gemini-";
  if (!model.starts_with(kGeminiPrefix)) {
    return std::nullopt;
  }
  model.remove_prefix(kGeminiPrefix.size());

  const auto read_number = [&model]() -> std::optional<int> {
    int value = 0;
    std::size_t digits = 0;
    while (digits < model.size() &&
           std::isdigit(static_cast<unsigned char>(model[digits]))) {
      value = value * 10 + (model[digits] - '0');
      ++digits;
    }
    if (digits == 0) {
      return std::nullopt;
    }
    model.remove_prefix(digits);
    return value;
  };

  const auto major = read_number();
  if (!major) {
    return std::nullopt;
  }
  int minor = 0;
  if (model.starts_with('.')) {
    model.remove_prefix(1);
    minor = read_number().value_or(0);
  }
  return std::make_pair(*major, minor);
}

/// Gemini 4 and Gemini 3.6+ ignore custom `temperature`, `topP` and `topK`
/// values, answer them with deprecation warnings, and are announced to reject
/// them with HTTP 400 in a future release.
bool ignores_sampling_parameters(const std::string& model) {
  const auto version = gemini_version(model);
  if (!version) {
    return false;
  }
  const auto [major, minor] = *version;
  return major >= 4 || (major == 3 && minor >= 6);
}

/// The `thinkingConfig` with the least thinking the model accepts, or
/// std::nullopt for models with no known thinking settings. Like the lowest
/// reasoning effort of the OpenAI builders, it keeps the provider-neutral
/// options within their latency and token budget: thinking tokens count
/// against `maxOutputTokens`, and the default levels (`high` on Gemini 4 Neon
/// and Hydrogen) can use up a small limit before any text is produced.
std::optional<nlohmann::json> lowest_thinking_config(std::string_view model) {
  if (model.starts_with(kModelsPrefix)) {
    model.remove_prefix(kModelsPrefix.size());
  }
  // Gemini 4 requires `thinkingLevel` (`thinkingBudget` is deprecated), and
  // every Gemini 4 model supports `low`; `minimal` is not supported.
  if (const auto version = gemini_version(model);
      version && version->first >= 4) {
    return nlohmann::json{{"thinkingLevel", "low"}};
  }
  // Gemini 3 levels per model, from the thinking guide.
  for (const auto* low_model :
       {"gemini-3.8-flash", "gemini-3.1-pro-preview", "gemini-3-pro-preview"}) {
    if (model == low_model) {
      return nlohmann::json{{"thinkingLevel", "low"}};
    }
  }
  for (const auto* minimal_model :
       {"gemini-3.6-flash", "gemini-3.5-flash-lite", "gemini-3.1-flash-lite",
        "gemini-3-flash-preview"}) {
    if (model == minimal_model) {
      return nlohmann::json{{"thinkingLevel", "minimal"}};
    }
  }
  // Gemini 2.5 takes a token budget: 0 disables thinking on Flash and
  // Flash-Lite, Pro cannot disable it and accepts at least 128.
  if (model == "gemini-2.5-flash" || model == "gemini-2.5-flash-lite") {
    return nlohmann::json{{"thinkingBudget", 0}};
  }
  if (model == "gemini-2.5-pro") {
    return nlohmann::json{{"thinkingBudget", 128}};
  }
  return std::nullopt;
}

nlohmann::json text_part(const std::string& text) {
  return {{"text", text}};
}

/// `functionResponse.response` must be an object; the API documents an
/// `error` key for failed calls and `output` for the function output.
nlohmann::json function_response_object(const ToolResultContentPart& result) {
  if (result.is_error) {
    if (result.result.is_object() && result.result.contains("error")) {
      return result.result;
    }
    return {{"error", result.result}};
  }
  if (result.result.is_object()) {
    return result.result;
  }
  return {{"output", result.result}};
}

class ContentsBuilder {
 public:
  void add_message(const Message& message) {
    switch (message.role) {
      case kMessageRoleSystem:
        // Gemini has no system role in `contents`; system messages join the
        // `systemInstruction`.
        for (const auto& part : message.content) {
          if (const auto* text = std::get_if<TextContentPart>(&part);
              text != nullptr && !text->text.empty()) {
            system_parts_.push_back(text_part(text->text));
          }
        }
        return;
      case kMessageRoleAssistant:
        add_model_content(message);
        return;
      case kMessageRoleUser:
      default:
        add_user_content(message);
        return;
    }
  }

  void add_system_text(const std::string& text) {
    if (!text.empty()) {
      system_parts_.push_back(text_part(text));
    }
  }

  void add_user_text(const std::string& text) {
    append_content("user", nlohmann::json::array({text_part(text)}));
  }

  nlohmann::json& contents() { return contents_; }
  nlohmann::json& system_parts() { return system_parts_; }

 private:
  /// Gemini expects alternating `user` and `model` turns: parts of
  /// consecutive messages with the same role (two user messages in a row, or
  /// tool results followed by a user message) join one content entry.
  void append_content(const char* role, nlohmann::json parts) {
    if (!contents_.empty() && contents_.back()["role"] == role) {
      auto& previous = contents_.back()["parts"];
      for (auto& part : parts) {
        previous.push_back(std::move(part));
      }
      return;
    }
    contents_.push_back({{"role", role}, {"parts", std::move(parts)}});
  }

  void add_user_content(const Message& message) {
    nlohmann::json parts = nlohmann::json::array();
    for (const auto& part : message.content) {
      if (const auto* text = std::get_if<TextContentPart>(&part)) {
        if (!text->text.empty()) {
          parts.push_back(text_part(text->text));
        }
      } else if (const auto* result =
                     std::get_if<ToolResultContentPart>(&part)) {
        parts.push_back(function_response_part(*result));
      }
    }
    if (!parts.empty()) {
      append_content("user", std::move(parts));
    }
  }

  nlohmann::json function_response_part(const ToolResultContentPart& result) {
    // `functionResponse.name` is required, and tool results carry only the
    // call ID: take the name from the call they answer.
    const auto name_it = call_names_.find(result.tool_call_id);
    if (name_it == call_names_.end()) {
      throw std::invalid_argument(
          "Gemini needs the function name of every tool result, but the "
          "result for tool call '" +
          result.tool_call_id +
          "' does not follow an assistant message with that tool call");
    }
    nlohmann::json response{{"name", name_it->second},
                            {"response", function_response_object(result)}};
    if (!result.tool_call_id.empty() &&
        !is_synthetic_call_id(result.tool_call_id)) {
      response["id"] = result.tool_call_id;
    }
    return {{"functionResponse", std::move(response)}};
  }

  void add_model_content(const Message& message) {
    nlohmann::json parts = nlohmann::json::array();
    std::optional<std::string> pending_signature;
    bool has_signature = false;
    std::optional<std::size_t> first_function_call;

    const auto push_part = [&](nlohmann::json part) {
      if (pending_signature) {
        part["thoughtSignature"] = std::move(*pending_signature);
        pending_signature.reset();
        has_signature = true;
      }
      parts.push_back(std::move(part));
    };

    for (const auto& part : message.content) {
      if (const auto* text = std::get_if<TextContentPart>(&part)) {
        if (!text->text.empty()) {
          push_part(text_part(text->text));
        }
      } else if (const auto* call = std::get_if<ToolCallContentPart>(&part)) {
        nlohmann::json function_call{
            {"name", call->tool_name},
            {"args", call->arguments.is_null() ? nlohmann::json::object()
                                               : call->arguments}};
        if (!call->id.empty() && !is_synthetic_call_id(call->id)) {
          function_call["id"] = call->id;
        }
        call_names_[call->id] = call->tool_name;
        if (!first_function_call) {
          first_function_call = parts.size();
        }
        push_part({{"functionCall", std::move(function_call)}});
      } else if (const auto* provider = std::get_if<ProviderContentPart>(&part);
                 provider != nullptr &&
                 provider->provider == kGoogleProviderName &&
                 provider->data.is_object()) {
        const auto type = provider->data.value("type", "");
        if (type == kThoughtSignaturePartType &&
            provider->data.contains("thoughtSignature") &&
            provider->data["thoughtSignature"].is_string()) {
          pending_signature =
              provider->data["thoughtSignature"].get<std::string>();
        } else if (type == kRawPartType && provider->data.contains("part") &&
                   provider->data["part"].is_object()) {
          const auto& raw = provider->data["part"];
          has_signature = has_signature || raw.contains("thoughtSignature");
          parts.push_back(raw);
        }
      }
      // Parts of other providers and tool results are not part of a model
      // turn.
    }

    if (parts.empty()) {
      return;
    }

    // Gemini 3 and later reject a turn whose function calls come without the
    // thought signature the model attached to them. Calls the model did not
    // produce in this form (streamed, hand-built, or from another provider)
    // carry the signature the API documents for that case.
    if (!has_signature && first_function_call) {
      parts[*first_function_call]["thoughtSignature"] =
          kSkipThoughtSignatureValidator;
    }

    append_content("model", std::move(parts));
  }

  nlohmann::json contents_ = nlohmann::json::array();
  nlohmann::json system_parts_ = nlohmann::json::array();
  std::map<std::string, std::string> call_names_;
};

/// Map the OpenAI-style `response_format` of `GenerateOptions` (Chat
/// Completions `json_schema` / `json_object` / `text`, or the flat Responses
/// `json_schema` shape) to Gemini's structured-output settings.
void apply_response_format(const nlohmann::json& response_format,
                           nlohmann::json& generation_config) {
  const auto type = response_format.is_object()
                        ? response_format.value("type", std::string{})
                        : std::string{};
  if (type == "text") {
    return;
  }
  if (type == "json_object") {
    generation_config["responseMimeType"] = "application/json";
    return;
  }
  if (type == "json_schema") {
    const nlohmann::json* schema = nullptr;
    if (const auto it = response_format.find("json_schema");
        it != response_format.end() && it->is_object() &&
        it->contains("schema")) {
      schema = &(*it)["schema"];
    } else if (const auto flat = response_format.find("schema");
               flat != response_format.end()) {
      schema = &*flat;
    }
    if (schema == nullptr || !schema->is_object()) {
      throw std::invalid_argument(
          "response_format of type json_schema has no schema object");
    }
    generation_config["responseMimeType"] = "application/json";
    generation_config["responseJsonSchema"] = *schema;
    return;
  }
  throw std::invalid_argument(
      "Unsupported response_format for Gemini: " + response_format.dump() +
      " (expected type text, json_object, or json_schema)");
}

}  // namespace

std::string model_resource_name(const std::string& model) {
  if (std::string_view(model).starts_with(kModelsPrefix)) {
    return model;
  }
  return std::string(kModelsPrefix) + model;
}

void include_thought_summaries(nlohmann::json& request) {
  const auto config = request.find("generationConfig");
  if (config == request.end() || !config->is_object()) {
    return;
  }
  const auto thinking = config->find("thinkingConfig");
  if (thinking == config->end() || !thinking->is_object()) {
    return;
  }
  if (const auto budget = thinking->find("thinkingBudget");
      budget != thinking->end() && budget->is_number() &&
      budget->get<int>() == 0) {
    return;
  }
  (*thinking)["includeThoughts"] = true;
}

nlohmann::json GoogleRequestBuilder::build_request_json(
    const GenerateOptions& options) {
  ContentsBuilder builder;
  builder.add_system_text(options.system);

  if (options.messages.empty()) {
    if (!options.prompt.empty()) {
      builder.add_user_text(options.prompt);
    }
  } else {
    for (const auto& message : options.messages) {
      builder.add_message(message);
    }
  }

  nlohmann::json request{{"contents", std::move(builder.contents())}};
  if (!builder.system_parts().empty()) {
    request["systemInstruction"] = {
        {"parts", std::move(builder.system_parts())}};
  }

  nlohmann::json generation_config = nlohmann::json::object();
  if (options.max_tokens) {
    generation_config["maxOutputTokens"] = *options.max_tokens;
  }

  const bool sampling_ignored = ignores_sampling_parameters(options.model);
  const auto add_sampling_parameter = [&](const char* option, const char* key,
                                          const std::optional<double>& value) {
    if (!value) {
      return;
    }
    if (sampling_ignored) {
      ai::logger::log_warn(
          "Ignoring {} for {} because the model does not support sampling "
          "parameters",
          option, options.model);
    } else {
      generation_config[key] = *value;
    }
  };
  add_sampling_parameter("temperature", "temperature", options.temperature);
  add_sampling_parameter("top_p", "topP", options.top_p);

  if (options.seed) {
    generation_config["seed"] = *options.seed;
  }
  if (options.presence_penalty) {
    generation_config["presencePenalty"] = *options.presence_penalty;
  }
  if (options.frequency_penalty) {
    generation_config["frequencyPenalty"] = *options.frequency_penalty;
  }
  if (options.response_format) {
    apply_response_format(*options.response_format, generation_config);
  }
  if (auto thinking_config = lowest_thinking_config(options.model)) {
    generation_config["thinkingConfig"] = std::move(*thinking_config);
  }
  if (!generation_config.empty()) {
    request["generationConfig"] = std::move(generation_config);
  }

  if (options.has_tools()) {
    nlohmann::json declarations = nlohmann::json::array();
    for (const auto& tool_name : options.get_active_tool_names()) {
      const auto it = options.tools.find(tool_name);
      if (it == options.tools.end()) {
        continue;
      }
      nlohmann::json declaration{{"name", tool_name},
                                 {"description", it->second.description}};
      // `parametersJsonSchema` takes JSON Schema as is; the alternative
      // `parameters` field only accepts an OpenAPI 3.0 subset.
      if (!it->second.parameters_schema.is_null()) {
        declaration["parametersJsonSchema"] = it->second.parameters_schema;
      }
      declarations.push_back(std::move(declaration));
    }

    if (!declarations.empty()) {
      request["tools"] = nlohmann::json::array(
          {{{"functionDeclarations", std::move(declarations)}}});

      nlohmann::json function_calling_config;
      switch (options.tool_choice.type) {
        case ToolChoiceType::kAuto:
          function_calling_config = {{"mode", "AUTO"}};
          break;
        case ToolChoiceType::kRequired:
          function_calling_config = {{"mode", "ANY"}};
          break;
        case ToolChoiceType::kNone:
          function_calling_config = {{"mode", "NONE"}};
          break;
        case ToolChoiceType::kSpecific:
          if (options.tool_choice.tool_name) {
            function_calling_config = {
                {"mode", "ANY"},
                {"allowedFunctionNames",
                 nlohmann::json::array({*options.tool_choice.tool_name})}};
          }
          break;
      }
      if (!function_calling_config.is_null()) {
        request["toolConfig"] = {
            {"functionCallingConfig", std::move(function_calling_config)}};
      }

      ai::logger::log_debug("Added {} function declarations with choice: {}",
                            request["tools"][0]["functionDeclarations"].size(),
                            options.tool_choice.to_string());
    }
  }

  return request;
}

nlohmann::json GoogleRequestBuilder::build_request_json(
    const EmbeddingOptions& options) {
  // Gemini embeddings use a separate `embedContent` API that the client does
  // not implement (see `GoogleClient::embeddings`).
  return {{"model", model_resource_name(options.model)},
          {"input", options.input}};
}

httplib::Headers GoogleRequestBuilder::build_headers(
    const providers::ProviderConfig& config) {
  httplib::Headers headers = {
      {config.auth_header_name, config.auth_header_prefix + config.api_key}};
  for (const auto& [key, value] : config.extra_headers) {
    headers.emplace(key, value);
  }
  return headers;
}

}  // namespace google
}  // namespace ai
