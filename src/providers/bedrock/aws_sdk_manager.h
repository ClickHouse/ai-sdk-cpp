#ifndef AI_SDK_BEDROCK_AWS_SDK_MANAGER_H
#define AI_SDK_BEDROCK_AWS_SDK_MANAGER_H

#include <aws/core/Aws.h>
#include <mutex>

namespace ai {
namespace bedrock {

// AWS SDK lifecycle manager
// Thread-safe singleton with reference counting for proper initialization/shutdown
// 
// For production applications:
// - Use acquire()/release() for RAII-style management (recommended)
// - SDK is initialized on first acquire(), shutdown on last release()
// - BedrockClient automatically calls acquire/release
//
// For standalone functions (like list_available_models):
// - Use ensure_initialized() which keeps SDK alive until process exit
class AwsSdkManager {
 public:
  static AwsSdkManager& instance() {
    static AwsSdkManager instance;
    return instance;
  }

  // Acquire a reference to the SDK (increments ref count, initializes if needed)
  void acquire();

  // Release a reference to the SDK (decrements ref count, shuts down if zero)
  void release();

  // Ensure SDK is initialized (for standalone functions)
  // Does not participate in reference counting - SDK stays alive until process exit
  void ensure_initialized();

 private:
  AwsSdkManager() = default;
  ~AwsSdkManager();

  // Non-copyable
  AwsSdkManager(const AwsSdkManager&) = delete;
  AwsSdkManager& operator=(const AwsSdkManager&) = delete;

  std::mutex mutex_;
  size_t ref_count_ = 0;
  bool initialized_ = false;
  Aws::SDKOptions options_;
};

}  // namespace bedrock
}  // namespace ai

#endif  // AI_SDK_BEDROCK_AWS_SDK_MANAGER_H
