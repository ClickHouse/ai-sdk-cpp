#pragma once

namespace ai {
namespace bedrock {

/// Initializes the AWS SDK for C++ once per process.
///
/// The SDK does not support `Aws::InitAPI` after `Aws::ShutdownAPI`, and
/// calling `Aws::ShutdownAPI` from a static destructor races with other static
/// objects (and with streams that may still be running on background
/// threads). So the SDK is initialized on first use and is never shut down:
/// its resources are released by the operating system at process exit.
void ensure_aws_sdk_initialized();

}  // namespace bedrock
}  // namespace ai
