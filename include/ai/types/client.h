#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ai {

/// Forward declaration for client implementation
namespace internal {
class ClientImpl;
}

/// Handle for an AI provider client (PIMPL pattern)
class Client {
 public:
  /// Constructor (implementation will be in source file)
  Client();

  /// Destructor
  ~Client();

  /// Move constructor
  Client(Client&& other) noexcept;

  /// Move assignment operator
  Client& operator=(Client&& other) noexcept;

  /// Delete copy constructor and assignment (clients manage unique resources)
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  /// Check if client is valid and ready to use
  bool is_valid() const;

  /// Get the provider name for this client
  std::string provider_name() const;

  /// Create a model identifier for this provider
  std::string model(const std::string& model_name) const;

  /// Get list of supported models (if available from provider)
  std::vector<std::string> supported_models() const;

  /// Check if a specific model is supported
  bool supports_model(const std::string& model_name) const;

  /// Get provider-specific configuration info
  std::string config_info() const;

 private:
  std::unique_ptr<internal::ClientImpl> client_impl_;
};

}  // namespace ai