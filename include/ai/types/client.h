#pragma once

#include "generate_options.h"
#include "stream_options.h"
#include "stream_result.h"

#include <memory>
#include <string>
#include <vector>

namespace ai {

class Client {
 public:
  virtual ~Client() = default;

  Client() = default;

  explicit Client(std::unique_ptr<Client> impl) : pimpl_(std::move(impl)) {}

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  Client(Client&& other) noexcept = default;

  Client& operator=(Client&& other) noexcept = default;

  virtual GenerateResult generate_text(const GenerateOptions& options) {
    if (pimpl_)
      return pimpl_->generate_text(options);
    return GenerateResult("Client not initialized");
  }

  virtual StreamResult stream_text(const StreamOptions& options) {
    if (pimpl_)
      return pimpl_->stream_text(options);
    return StreamResult();
  }

  virtual bool is_valid() const {
    if (pimpl_)
      return pimpl_->is_valid();
    return false;
  }

  virtual std::string provider_name() const {
    if (pimpl_)
      return pimpl_->provider_name();
    return "unknown";
  }

  virtual std::vector<std::string> supported_models() const {
    if (pimpl_)
      return pimpl_->supported_models();
    return {};
  }

  virtual bool supports_model(const std::string& model_name) const {
    if (pimpl_)
      return pimpl_->supports_model(model_name);
    return false;
  }

  virtual std::string config_info() const {
    if (pimpl_)
      return pimpl_->config_info();
    return "No configuration";
  }

 private:
  std::unique_ptr<Client> pimpl_;
};

}  // namespace ai