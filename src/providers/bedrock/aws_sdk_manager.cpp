#include "aws_sdk_manager.h"

#include "secure_logger.h"

#include <mutex>

#include <aws/core/Aws.h>

namespace ai {
namespace bedrock {

void ensure_aws_sdk_initialized() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Intentionally leaked: see the comment in the header.
    auto* options = new Aws::SDKOptions();
    Aws::InitAPI(*options);
    SecureLogger::log_debug("AWS SDK initialized");
  });
}

}  // namespace bedrock
}  // namespace ai
