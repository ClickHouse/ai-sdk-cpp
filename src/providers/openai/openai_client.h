#pragma once

#include "ai/types/client.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ai {
namespace openai {

class OpenAIClient : public Client {
 public:
  OpenAIClient(const std::string& api_key, const std::string& base_url);

  GenerateResult generate_text(const GenerateOptions& options) override;
  StreamResult stream_text(const StreamOptions& options) override;
  bool is_valid() const override;
  std::string provider_name() const override;
  std::vector<std::string> supported_models() const override;
  bool supports_model(const std::string& model_name) const override;
  std::string config_info() const override;

 private:
  nlohmann::json build_request_json(const GenerateOptions& options);
  GenerateResult parse_chat_completion_response(const nlohmann::json& response);
  GenerateResult parse_error_response(int status_code, const std::string& body);
  std::string message_role_to_string(MessageRole role);
  FinishReason parse_finish_reason(const std::string& reason);

  std::string api_key_;
  std::string base_url_;
  std::string host_;
  bool use_ssl_ = true;
};

}  // namespace openai
}  // namespace ai