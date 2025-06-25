#pragma once

#include "generate_options.h"
#include "stream_options.h"
#include "stream_result.h"

#include <memory>
#include <string>
#include <vector>

namespace ai {

/// Abstract base class for AI provider clients
class Client {
 public:
  /// Virtual destructor
  virtual ~Client() = default;

  /// Default constructor
  Client() = default;

  /// Constructor that takes ownership of implementation (for factory functions)
  explicit Client(std::unique_ptr<Client> impl) : pimpl_(std::move(impl)) {}

  /// Delete copy constructor and assignment (clients manage unique resources)
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  /// Move constructor
  Client(Client&& other) noexcept = default;

  /// Move assignment operator
  Client& operator=(Client&& other) noexcept = default;

  /// Generate text using the provider's API
  virtual GenerateResult generate_text(const GenerateOptions& options) {
    if (pimpl_)
      return pimpl_->generate_text(options);
    return GenerateResult("Client not initialized");
  }

  /// Stream text generation using the provider's API
  virtual StreamResult stream_text(const StreamOptions& options) {
    if (pimpl_)
      return pimpl_->stream_text(options);
    return StreamResult();
  }

  /// Check if client is valid and ready to use
  virtual bool is_valid() const {
    if (pimpl_)
      return pimpl_->is_valid();
    return false;
  }

  /// Get the provider name for this client
  virtual std::string provider_name() const {
    if (pimpl_)
      return pimpl_->provider_name();
    return "unknown";
  }

  /// Get list of supported models (if available from provider)
  virtual std::vector<std::string> supported_models() const {
    if (pimpl_)
      return pimpl_->supported_models();
    return {};
  }

  /// Check if a specific model is supported
  virtual bool supports_model(const std::string& model_name) const {
    if (pimpl_)
      return pimpl_->supports_model(model_name);
    return false;
  }

  /// Get provider-specific configuration info
  virtual std::string config_info() const {
    if (pimpl_)
      return pimpl_->config_info();
    return "No configuration";
  }

 private:
  std::unique_ptr<Client> pimpl_;  ///< Pointer to implementation
};

}  // namespace ai