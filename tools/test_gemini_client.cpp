#include <iostream>
#include <ai/ai.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: test_gemini_client <api_key>\n";
    return 2;
  }
  std::string key = argv[1];
  auto client = ai::gemini::create_client(key);
  std::cout << "client.is_valid()=" << client.is_valid() << "\n";
  std::cout << "provider_name=" << client.provider_name() << "\n";
  std::cout << "default_model=" << client.default_model() << "\n";
  std::cout << "config_info=" << client.config_info() << "\n";
  return 0;
}
