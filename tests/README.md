# AI SDK C++ Test Suite

This directory contains a comprehensive test suite for the AI SDK C++ library, providing thorough coverage of the OpenAI provider implementation and core functionality.

## Overview

The test suite is organized into several categories:

- **Unit Tests** (`unit/`) - Test individual components in isolation
- **Integration Tests** (`integration/`) - Test end-to-end functionality with real or mock APIs
- **Performance Tests** (`performance/`) - Measure throughput, latency, and memory usage
- **Test Utilities** (`utils/`) - Shared testing infrastructure and helpers

## Test Categories

### Unit Tests

#### `openai_client_test.cpp`
Tests the core OpenAI client functionality:
- Constructor validation and configuration
- Model support verification
- Request building and JSON serialization  
- Response parsing (success and error cases)
- Error handling for network failures
- Parameter validation and edge cases

#### `types_test.cpp`
Tests the type system and data structures:
- `GenerateOptions` construction and validation
- `GenerateResult` success/error states
- `Message` and conversation handling
- `Usage` token tracking
- Error class hierarchy
- Enum value validation

#### `openai_stream_test.cpp`
Tests streaming functionality:
- `StreamOptions` configuration
- Server-Sent Events (SSE) parsing
- Streaming event processing
- Stream error handling
- Mock stream implementations

### Integration Tests

#### `openai_integration_test.cpp`
End-to-end testing with OpenAI API:
- Basic text generation
- Conversation with messages
- Different model support
- Error scenarios (invalid API key, model not found, rate limiting)
- Parameter handling (temperature, max_tokens, etc.)
- Large prompt processing
- Network timeout and failure handling

**Environment Variables:**
- `OPENAI_API_KEY` - Required for real API tests
- `AI_SDK_RUN_INTEGRATION_TESTS` - Set to enable integration tests
- `OPENAI_BASE_URL` - Optional custom base URL

#### `mock_server_test.cpp`
Tests using a mock HTTP server:
- HTTP request/response handling
- OpenAI API response simulation
- Error response generation
- Streaming response testing
- Network condition simulation
- Request history tracking

### Performance Tests

#### `throughput_test.cpp`
Measures request processing performance:
- Sequential request throughput
- Concurrent request performance
- Large prompt handling
- Response size impact
- Stress testing with many requests
- Latency distribution analysis

#### `memory_test.cpp`
Analyzes memory usage patterns:
- Client creation/destruction cycles
- Large data handling
- Memory leak detection
- Concurrent memory usage
- Error condition memory handling
- Complex response processing

### Test Utilities

#### `test_fixtures.h/.cpp`
Common test infrastructure:
- `AITestFixture` - Base test setup
- `OpenAITestFixture` - OpenAI-specific helpers
- `TestDataGenerator` - Sample data creation
- `TestAssertions` - Custom assertion helpers

#### `mock_openai_client.h/.cpp`
Controllable OpenAI client for testing:
- `ControllableOpenAIClient` - Predictable responses
- `NetworkFailureSimulator` - Network error simulation
- `ResponseBuilder` - OpenAI response generation

#### `mock_http_server.h/.cpp`
HTTP server for integration testing:
- `MockHttpServer` - Lightweight HTTP server
- `OpenAIResponseBuilder` - OpenAI-specific responses
- `NetworkConditionSimulator` - Network condition testing
- `TestServerScope` - RAII server management

## Building and Running Tests

### Prerequisites

- CMake 3.16+
- C++20 compatible compiler
- vcpkg with required packages:
  - GTest
  - GMock
  - httplib
  - nlohmann_json
  - fmt
  - spdlog

### Build Configuration

```bash
# Enable tests in CMake
cmake -B build -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake

# Build the project and tests
cmake --build build
```

### Running Tests

```bash
# Run all tests
cd build && ctest

# Run specific test categories
./ai_tests --gtest_filter="OpenAIClientTest.*"
./ai_tests --gtest_filter="*Integration*"
./ai_tests --gtest_filter="*Performance*"

# Run with verbose output
./ai_tests --gtest_verbose

# Run performance tests separately
./ai_performance_tests
```

### Integration Test Setup

For real API integration tests:

```bash
# Set environment variables
export OPENAI_API_KEY="your-api-key-here"
export AI_SDK_RUN_INTEGRATION_TESTS="1"

# Optional: Use custom base URL
export OPENAI_BASE_URL="https://custom-api.example.com"

# Run integration tests
./ai_tests --gtest_filter="*Integration*"
```

## Test Data and Fixtures

### Sample Data
The test suite includes various sample data for testing:
- Valid OpenAI API responses
- Error responses for different scenarios
- Large prompts and responses
- Streaming event sequences
- Malformed JSON responses

### Mock Responses
Pre-built response scenarios:
- Successful chat completions
- Authentication errors
- Rate limiting responses
- Model not found errors
- Server errors
- Streaming responses

## Adding New Tests

### Unit Tests
1. Create test file in `unit/` directory
2. Include necessary headers and test fixtures
3. Use `OpenAITestFixture` for OpenAI-specific tests
4. Follow existing naming conventions

Example:
```cpp
#include <gtest/gtest.h>
#include "../utils/test_fixtures.h"

class MyFeatureTest : public OpenAITestFixture {};

TEST_F(MyFeatureTest, BasicFunctionality) {
    // Test implementation
}
```

### Integration Tests
1. Create test file in `integration/` directory
2. Use `TestServerScope` for mock server tests
3. Check environment variables for real API tests
4. Handle both mock and real API scenarios

### Performance Tests
1. Add to existing performance test files or create new ones
2. Use timing and memory measurement utilities
3. Set reasonable performance thresholds
4. Include performance metrics output

## Test Configuration

### CMake Options
- `BUILD_TESTS` - Enable/disable test building
- Test discovery is automatic via `gtest_discover_tests`

### Preprocessor Definitions
- `AI_SDK_TESTING=1` - Indicates test build
- `AI_SDK_DEBUG_TESTS=1` - Enable debug test features (Debug builds only)

### Test Timeouts
- Default test timeout: 30 seconds
- Performance tests: May have longer timeouts
- Integration tests: Configurable based on API response times

## Continuous Integration

The test suite is designed to work in CI environments:

### Mock-Only Mode
When no API key is provided:
- Integration tests use mock responses
- All functionality can be tested without external dependencies
- Suitable for pull request validation

### Full Integration Mode
When API key is provided:
- Real API calls are made
- Network conditions are tested
- Rate limiting behavior is verified
- Suitable for release validation

## Troubleshooting

### Common Issues

1. **Tests fail to build**: Check that all dependencies are installed via vcpkg
2. **Integration tests fail**: Verify `OPENAI_API_KEY` is set correctly
3. **Performance tests fail**: May indicate system load or timing issues
4. **Mock server tests fail**: Check that ports are available

### Debug Tips

1. Use `--gtest_verbose` for detailed output
2. Set `AI_SDK_DEBUG_TESTS=1` for additional debug information
3. Check test logs for specific error messages
4. Use debugger with individual test cases

### Skipping Tests

Tests can be skipped conditionally:
```cpp
if (!use_real_api_) {
    GTEST_SKIP() << "Real API not available";
}
```

## Test Coverage

The test suite aims for comprehensive coverage:

- **Functional Coverage**: All public APIs and major code paths
- **Error Coverage**: All error conditions and edge cases
- **Performance Coverage**: Throughput, latency, and memory metrics
- **Integration Coverage**: End-to-end workflows with real APIs

Current coverage areas:
- ✅ OpenAI client core functionality
- ✅ Type system and data structures
- ✅ Error handling and validation
- ✅ Streaming functionality
- ✅ Network failure scenarios
- ✅ Performance characteristics
- ✅ Memory management
- ⚠️ Advanced streaming features (partial)
- ⚠️ Authentication edge cases (partial)

## Contributing

When adding new features:

1. **Add corresponding tests** for all new functionality
2. **Update existing tests** if APIs change
3. **Add performance tests** for performance-critical features
4. **Include integration tests** for user-facing features
5. **Update this documentation** for significant changes

Test quality guidelines:
- Tests should be deterministic and reliable
- Use descriptive test names that explain what is being tested
- Include both positive and negative test cases
- Test edge cases and error conditions
- Use appropriate test fixtures and utilities
- Keep test code clean and maintainable