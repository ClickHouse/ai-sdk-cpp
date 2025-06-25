#include "anthropic_factory.h"

#include "anthropic_client.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace ai {
namespace anthropic {

Client create_client() {
  const char* api_key = std::getenv("ANTHROPIC_API_KEY");
  if (!api_key) {
    throw std::runtime_error("ANTHROPIC_API_KEY environment variable not set");
  }
  return Client(
      std::make_unique<AnthropicClient>(api_key, "https://api.anthropic.com"));
}

Client create_client(const std::string& api_key) {
  return Client(
      std::make_unique<AnthropicClient>(api_key, "https://api.anthropic.com"));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<AnthropicClient>(api_key, base_url));
}

}  // namespace anthropic
}  // namespace ai