#include "openai_factory.h"

#include "openai_client.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace ai {
namespace openai {

Client create_client() {
  const char* api_key = std::getenv("OPENAI_API_KEY");
  if (!api_key) {
    throw std::runtime_error("OPENAI_API_KEY environment variable not set");
  }
  return Client(
      std::make_unique<OpenAIClient>(api_key, "https://api.openai.com"));
}

Client create_client(const std::string& api_key) {
  return Client(
      std::make_unique<OpenAIClient>(api_key, "https://api.openai.com"));
}

Client create_client(const std::string& api_key, const std::string& base_url) {
  return Client(std::make_unique<OpenAIClient>(api_key, base_url));
}

}  // namespace openai
}  // namespace ai