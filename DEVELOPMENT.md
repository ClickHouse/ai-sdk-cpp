# Development Guide

This document provides comprehensive information about developing, building, and contributing to AI SDK C++.

## Prerequisites

### Required Tools

- **C++ Compiler**: C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake**: Version 3.16 or higher
- **Python**: 3.8+ with uv package manager for development scripts
- **Git**: For version control

### Optional Tools (Recommended)

- **clang-format**: For code formatting
- **clang-tidy**: For static analysis and linting
- **gdb/lldb**: For debugging

## Development Setup

### 1. Clone the Repository

```bash
git clone --recursive https://github.com/ClickHouse/ai-sdk-cpp.git
cd ai-sdk-cpp
```

### 2. Install Python Dependencies

We use `uv` for managing Python development tools:

```bash
# Install uv if you haven't already
curl -LsSf https://astral.sh/uv/install.sh | sh

# Development scripts will automatically install their dependencies
# when run with 'uv run'
```

## Building the Project

We provide a modern Python-based build script that handles all CMake configuration and building.

### Basic Build

```bash
# Debug build (default)
uv run scripts/build.py

# Or explicitly specify debug mode
uv run scripts/build.py --mode debug
```

### Release Build

```bash
uv run scripts/build.py --mode release
```

### Build with Tests

```bash
uv run scripts/build.py --mode debug --tests
```

### Clean Build

```bash
uv run scripts/build.py --mode release --clean
```

### Export Compile Commands (for IDE integration)

```bash
uv run scripts/build.py --mode debug --export-compile-commands
```

This generates `compile_commands.json` in both the build directory and project root, enabling excellent IDE support in VS Code, CLion, and other editors.

### Advanced Build Options

```bash
# Custom parallel jobs
uv run scripts/build.py --mode release --jobs 8

# Verbose output
uv run scripts/build.py --mode debug --verbose

# All options combined
uv run scripts/build.py --mode release --tests --clean --export-compile-commands --jobs 12
```

### Manual CMake Build

If you prefer using CMake directly:

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel $(nproc)
```

Note: Dependencies will be automatically fetched and built using CPM.cmake during the configuration step.

## Development Scripts

We provide several Python scripts to streamline development. All scripts use modern CLI tools (Click) and rich terminal output.

### 🔨 build.py - Build System

**Location**: `scripts/build.py`  
**Purpose**: Build the project with CMake

```bash
# Show all options
uv run scripts/build.py --help

# Common usage patterns
uv run scripts/build.py --mode debug                    # Debug build
uv run scripts/build.py --mode release --tests         # Release with tests
uv run scripts/build.py --clean --export-compile-commands  # Clean + IDE support
```

**Features**:
- ✅ Debug/Release builds
- ✅ Test building toggle
- ✅ Clean builds
- ✅ Export compile commands for IDEs
- ✅ Parallel building
- ✅ Automatic dependency management via CPM.cmake
- ✅ Rich terminal output with progress indicators

### 🎨 format.py - Code Formatting

**Location**: `scripts/format.py`  
**Purpose**: Format C++ code using clang-format

```bash
# Format all files
uv run scripts/format.py

# Check formatting without modifying files
uv run scripts/format.py --check
```

**Features**:
- ✅ Formats all C++ files (`.h`, `.hpp`, `.cc`, `.cpp`, `.cxx`)
- ✅ Excludes build directories and dependency files
- ✅ Check mode for CI/CD
- ✅ Progress indicators

### 🔍 lint.py - Static Analysis

**Location**: `scripts/lint.py`  
**Purpose**: Run clang-tidy for static analysis and linting

```bash
# Lint all files
uv run scripts/lint.py

# Auto-fix issues where possible
uv run scripts/lint.py --fix

# Custom parallel jobs
uv run scripts/lint.py --jobs 8
```

**Features**:
- ✅ Parallel linting for faster execution
- ✅ Auto-fix capability
- ✅ Requires `compile_commands.json` (build with `--export-compile-commands`)
- ✅ macOS system include path handling
- ✅ Rich progress reporting

## Testing

### Running Tests

After building with tests enabled:

```bash
cd build

# Run all tests
ctest

# Verbose output
ctest --verbose

# Run specific test
ctest -R "test_types"

# Parallel test execution
ctest --parallel $(nproc)
```

### Test Dependencies

Tests use Google Test framework, which is automatically fetched when you build with tests enabled:

```bash
uv run scripts/build.py --tests
```

The build system will automatically download and configure Google Test via CPM.cmake.

### Writing Tests

Create test files in the `tests/` directory following the pattern:

```cpp
#include <gtest/gtest.h>
#include <ai/types.h>

TEST(TypesTest, BasicFunctionality) {
    // Your test code here
    EXPECT_EQ(expected, actual);
}
```

## Code Quality

### Formatting Standards

We use clang-format with Google C++ style. Key guidelines:

- **Indentation**: 2 spaces
- **Line Length**: 80 characters
- **Naming**: CamelCase for types, snake_case for variables/functions
- **Headers**: Include guards and proper ordering

### Linting Rules

We use clang-tidy with these key checks:

- **Modernize**: Use modern C++ features (auto, nullptr, etc.)
- **Performance**: Avoid unnecessary copies, use move semantics
- **Readability**: Clear naming and structure
- **Safety**: Avoid common pitfalls and unsafe patterns

### Pre-commit Workflow

Before committing code:

```bash
# 1. Format code
uv run scripts/format.py

# 2. Build and test
uv run scripts/build.py --mode debug --tests

# 3. Run linting
uv run scripts/build.py --export-compile-commands  # Ensure compile_commands.json exists
uv run scripts/lint.py

# 4. Run tests
cd build && ctest
```

## Logging

### Configuring Log Levels

AI SDK C++ uses a built-in logging system defined in `ai/logger.h`.

### Setting Log Levels

You can control the logging verbosity by configuring the logger in your application:

```cpp
#include "ai/logger.h"

// In your main() or initialization code:

// Enable debug logging (most verbose)
ai::logger::install_logger(
    std::make_shared<ai::logger::ConsoleLogger>(ai::logger::LogLevel::kLogLevelDebug)
);

// Enable info logging (operational information)
ai::logger::install_logger(
    std::make_shared<ai::logger::ConsoleLogger>(ai::logger::LogLevel::kLogLevelInfo)
);

// Enable warning logging (default)
ai::logger::install_logger(
    std::make_shared<ai::logger::ConsoleLogger>(ai::logger::LogLevel::kLogLevelWarn)
);

// Enable error logging only
ai::logger::install_logger(
    std::make_shared<ai::logger::ConsoleLogger>(ai::logger::LogLevel::kLogLevelError)
);
```

### Available Log Levels

From most to least verbose:

1. **debug**: Detailed flow information, request/response bodies, connection details
2. **info**: Important operational events (successful completions, stream events)
3. **warn**: Warning conditions that don't prevent operation
4. **error**: Error conditions and exceptions

### Null Logger

To disable all logging, you can install a null logger:

```cpp
ai::logger::install_logger(
    std::make_shared<ai::logger::NullLogger>()
);
```

### Example Log Output

With **debug** level enabled:

```
[2024-01-01 12:00:00.123] [debug] Initializing OpenAI client with base_url: https://api.openai.com
[2024-01-01 12:00:00.124] [debug] OpenAI client configured - host: api.openai.com, use_ssl: true
[2024-01-01 12:00:00.125] [debug] Starting text generation - model: gpt-4o, prompt length: 42
[2024-01-01 12:00:00.126] [debug] Request JSON built: {"model":"gpt-4o","messages":[...]}
[2024-01-01 12:00:00.127] [debug] Creating SSL client for host: api.openai.com
[2024-01-01 12:00:01.234] [debug] Received response - status: 200, body length: 1234
[2024-01-01 12:00:01.235] [info] Text generation successful - model: gpt-4o, response_id: chatcmpl-abc123
```

With **info** level enabled:

```
[2024-01-01 12:00:01.235] [info] Text generation successful - model: gpt-4o, response_id: chatcmpl-abc123
[2024-01-01 12:00:02.456] [info] Text streaming started - model: gpt-4o-mini
[2024-01-01 12:00:03.789] [info] Stream completed - tokens used: 150 prompt, 350 completion, 500 total
```

### Custom Logger Configuration

You can create your own logger by implementing the `ai::logger::Logger` interface:

```cpp
#include "ai/logger.h"
#include <fstream>

class FileLogger : public ai::logger::Logger {
public:
    FileLogger(const std::string& filename)
        : file_(filename, std::ios::app) {}
    
    void log(ai::logger::LogLevel level, std::string_view message) override {
        if (is_enabled(level)) {
            file_ << "[" << level_to_string(level) << "] " << message << std::endl;
        }
    }
    
    bool is_enabled(ai::logger::LogLevel level) const override {
        return level >= min_level_;
    }
    
private:
    std::ofstream file_;
    ai::logger::LogLevel min_level_ = ai::logger::LogLevel::kLogLevelInfo;
    
    static std::string_view level_to_string(ai::logger::LogLevel level) {
        switch (level) {
            case ai::logger::LogLevel::kLogLevelDebug: return "DEBUG";
            case ai::logger::LogLevel::kLogLevelInfo: return "INFO";
            case ai::logger::LogLevel::kLogLevelWarn: return "WARN";
            case ai::logger::LogLevel::kLogLevelError: return "ERROR";
        }
        return "UNKNOWN";
    }
};

// Install custom logger
ai::logger::install_logger(std::make_shared<FileLogger>("ai_sdk.log"));
```

### Development Recommendations

- **Development**: Use `debug` level to see detailed API interactions
- **Testing**: Use `info` level to track key operations
- **Production**: Use `warn` or `error` level for performance

### Logging in Tests

When running tests, you might want to enable debug logging:

```bash
# Run tests with debug logging
SPDLOG_LEVEL=debug ctest --verbose
```

Or programmatically in your test fixtures:

```cpp
class AITestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable debug logging for tests
        ai::logger::install_logger(
            std::make_shared<ai::logger::ConsoleLogger>(ai::logger::LogLevel::kLogLevelDebug)
        );
    }
};
```

## Dependencies

### Core Dependencies

Managed automatically via CPM.cmake:

- **nlohmann-json**: JSON parsing and generation (fetched from GitHub)
- **cpp-httplib**: HTTP client library with OpenSSL support (fetched from GitHub)
- **concurrentqueue**: Lock-free concurrent queue (fetched from GitHub)
- **openssl**: Cryptographic library (system-provided)
- **Built-in logging**: ai::logger provides flexible logging capabilities

### Test Dependencies

Automatically fetched when tests are enabled:

- **gtest**: Google's C++ testing framework (fetched from GitHub via CPM.cmake)

### Development Dependencies

Python tools (automatically managed by uv):

- **click**: Modern CLI framework
- **rich**: Rich terminal output
- **asyncio**: Async support for parallel operations

## Contributing

### Code Style

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these key principles:

1. **One class per file**: Keep classes focused and maintainable
2. **Immutable by default**: Prefer const and immutable designs
3. **Modern C++**: Use C++20 features, auto, smart pointers
4. **RAII**: Resource management through object lifetime
5. **Move semantics**: Prefer move over copy for performance

### Modern C++ Guidelines

- Use `auto` for obvious types
- Use `nullptr` instead of `NULL` or `0`
- Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers
- Use move semantics with `std::move`
- Mark functions `const`, `constexpr`, `noexcept` when appropriate
- Use brace initialization `{}` instead of parentheses
- Capture lambda variables explicitly, avoid `[=]` and `[&]`
- Use `std::atomic` for thread-safe operations, not `volatile`
- Prefer `emplace` over `insert`/`push_back`

### Pull Request Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes following our code style
4. Run the pre-commit workflow (format, build, test, lint)
5. Commit with descriptive messages
6. Push to your fork: `git push origin feature/amazing-feature`
7. Create a Pull Request
