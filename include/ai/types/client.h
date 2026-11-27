#pragma once

#include "embedding_options.h"
#include "generate_options.h"
#include "model.h"
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

  virtual EmbeddingResult embeddings(const EmbeddingOptions& options) {
    if (pimpl_)
      return pimpl_->embeddings(options);
    return EmbeddingResult("Client not initialized");
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

  /// List the model IDs the provider serves to this API key, by querying its
  /// models endpoint (`GET /v1/models`). Makes a network request.
  virtual ModelListResult list_models() {
    if (pimpl_)
      return pimpl_->list_models();
    return ModelListResult("Client not initialized");
  }

  /// Well-known model IDs of the provider (the `models::k*` constants). This
  /// list is neither exhaustive nor a restriction: any model name is
  /// forwarded to the provider as is. Use `list_models` for the models
  /// actually available.
  virtual std::vector<std::string> supported_models() const {
    if (pimpl_)
      return pimpl_->supported_models();
    return {};
  }

  /// Whether the client can send requests for `model_name`. The SDK forwards
  /// any model name to the provider, so this is true for every non-empty
  /// name; whether the provider serves the model is known only to the
  /// provider (see `list_models`).
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

  virtual std::string default_model() const {
    if (pimpl_) {
      return pimpl_->default_model();
    }
    return "";
  }

 private:
  std::unique_ptr<Client> pimpl_;
};

}  // namespace ai