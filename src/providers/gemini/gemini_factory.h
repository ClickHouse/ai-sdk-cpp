#pragma once

#include "ai/types/client.h"

#include <string>
#include <optional>

namespace ai {
namespace gemini {

Client create_client();
Client create_client(const std::string& api_key);
Client create_client(const std::string& api_key, const std::string& base_url);

std::optional<Client> try_create_client();

}  // namespace gemini
}  // namespace ai
