#pragma once

#include "ai/types/client.h"

#include <string>

namespace ai {
namespace anthropic {

Client create_client();
Client create_client(const std::string& api_key);
Client create_client(const std::string& api_key, const std::string& base_url);

}  // namespace anthropic
}  // namespace ai