#include "aws_sdk_manager.h"
#include "secure_logger.h"

namespace ai {
namespace bedrock {

void AwsSdkManager::acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_) {
    Aws::InitAPI(options_);
    initialized_ = true;
    SecureLogger::log_debug("AWS SDK initialized");
  }
  ++ref_count_;
}

void AwsSdkManager::release() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ref_count_ > 0) {
    --ref_count_;
    if (ref_count_ == 0 && initialized_) {
      Aws::ShutdownAPI(options_);
      initialized_ = false;
      SecureLogger::log_debug("AWS SDK shutdown");
    }
  }
}

void AwsSdkManager::ensure_initialized() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_) {
    Aws::InitAPI(options_);
    initialized_ = true;
    SecureLogger::log_debug("AWS SDK initialized (standalone)");
  }
  // Note: Does not increment ref_count_ - SDK stays alive until process exit
}

AwsSdkManager::~AwsSdkManager() {
  // Shutdown if still initialized (handles standalone usage)
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_) {
    Aws::ShutdownAPI(options_);
  }
}

}  // namespace bedrock
}  // namespace ai
