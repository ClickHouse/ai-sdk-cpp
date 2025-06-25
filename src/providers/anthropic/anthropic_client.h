#pragma once

#include "ai/types/client.h"
#include "ai/types/generate_options.h"
#include "ai/types/stream_options.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ai {
namespace anthropic {

class AnthropicClient : public Client {
 public:
  explicit AnthropicClient(
      const std::string& api_key,
      const std::string& base_url = "https://api.anthropic.com");

  GenerateResult generate_text(const GenerateOptions& options) override;
  StreamResult stream_text(const StreamOptions& options) override;
  bool is_valid() const override;
  std::string provider_name() const override;
  std::vector<std::string> supported_models() const override;
  bool supports_model(const std::string& model_name) const override;
  std::string config_info() const override;

  // Internal methods exposed for testing
#ifdef AI_SDK_TESTING
 public:
#else
 private:
#endif
  nlohmann::json build_request_json(const GenerateOptions& options);
  GenerateResult generate_text_single_step(const GenerateOptions& options);
  GenerateResult parse_messages_response(const nlohmann::json& response);
  GenerateResult parse_error_response(int status_code, const std::string& body);
  std::string message_role_to_string(MessageRole role);
  FinishReason parse_stop_reason(const std::string& reason);

  GenerateResult make_request(const std::string& json_body);

  // Member access for testing
  const std::string& get_api_key() const { return api_key_; }
  const std::string& get_base_url() const { return base_url_; }
  const std::string& get_host() const { return host_; }
  bool get_use_ssl() const { return use_ssl_; }

 private:
  std::string api_key_;
  std::string base_url_;
  std::string host_;
  bool use_ssl_ = true;
};

}  // namespace anthropic
}  // namespace ai