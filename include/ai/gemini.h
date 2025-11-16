#pragma once

#include <optional>
#include <string>
#include "types/client.h"

namespace ai {
namespace gemini {

// Factory functions implemented in the SDK library (src/providers/gemini)
Client create_client();
Client create_client(const std::string& api_key);
Client create_client(const std::string& api_key, const std::string& base_url);

std::optional<Client> try_create_client();

}  // namespace gemini
}  // namespace ai

