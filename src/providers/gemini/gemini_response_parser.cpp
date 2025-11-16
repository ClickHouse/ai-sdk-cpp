#include "gemini_response_parser.h"

#include "ai/logger.h"

namespace ai {
namespace gemini {

GenerateResult GeminiResponseParser::parse_success_completion_response(const nlohmann::json& response) {
  // Attempt to parse OpenAI-like structure first
  try {
    if (response.contains("choices")) {
      auto text = response["choices"][0]["message"].value("content", std::string());
      GenerateResult result;
      result.text = text;
      result.finish_reason = kFinishReasonStop;
      return result;
    }

    // Try Google GL shape: top-level candidates with content.parts[].text
    if (response.contains("candidates")) {
      try {
        const auto& cand = response["candidates"][0];
        if (cand.contains("content")) {
          const auto& content = cand["content"];
          // If content has parts, join their text fields
          if (content.contains("parts") && content["parts"].is_array()) {
            std::string out;
            for (const auto& part : content["parts"]) {
              if (part.contains("text") && part["text"].is_string()) {
                out += part["text"].get<std::string>();
              }
            }
            GenerateResult result;
            result.text = out;
            result.finish_reason = kFinishReasonStop;
            return result;
          }
          // Fallback: if content is a string
          if (content.is_string()) {
            GenerateResult result;
            result.text = content.get<std::string>();
            result.finish_reason = kFinishReasonStop;
            return result;
          }
        }
      } catch (...) {
        // fallthrough to generic failure
      }
    }
  } catch (const std::exception& e) {
    ai::logger::log_error("Gemini parse exception: {}", e.what());
  }

  return GenerateResult("Failed to parse response");
}

GenerateResult GeminiResponseParser::parse_error_completion_response(int status_code, const std::string& body) {
  GenerateResult r;
  r.error = body;
  r.finish_reason = kFinishReasonError;
  r.provider_metadata = std::to_string(status_code);
  r.is_retryable = false;
  return r;
}

EmbeddingResult GeminiResponseParser::parse_success_embedding_response(const nlohmann::json& response) {
  EmbeddingResult r;
  // Minimal: look for data[0].embedding
  try {
    if (response.contains("data")) {
      auto emb = response["data"][0]["embedding"].get<std::vector<double>>();
      r.data = nlohmann::json(emb);
      return r;
    }
  } catch (...) {
  }
  r.error = "Failed to parse embedding response";
  return r;
}

EmbeddingResult GeminiResponseParser::parse_error_embedding_response(int status_code, const std::string& body) {
  EmbeddingResult r;
  r.error = body;
  r.provider_metadata = std::to_string(status_code);
  return r;
}

}  // namespace gemini
}  // namespace ai
