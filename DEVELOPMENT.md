# Development Guide

This document provides comprehensive information about developing, building, and contributing to AI SDK C++.

## Prerequisites

### Required Tools

- **C++ Compiler**: C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake**: Version 3.16 or higher
- **vcpkg**: Package manager for C++ dependencies
- **Python**: 3.8+ with uv package manager for development scripts
- **Git**: For version control

### Optional Tools (Recommended)

- **clang-format**: For code formatting
- **clang-tidy**: For static analysis and linting
- **gdb/lldb**: For debugging

## Development Setup

### 1. Clone the Repository

```bash
git clone https://github.com/ai-sdk/ai-sdk-cpp.git
cd ai-sdk-cpp
```

### 2. Install vcpkg

If you don't have vcpkg installed:

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# or
./bootstrap-vcpkg.bat  # Windows

# Set environment variable (add to your shell profile)
export VCPKG_ROOT=$(pwd)
```

### 3. Install Dependencies

Install the base dependencies:

```bash
vcpkg install --triplet=x64-linux  # or x64-osx, x64-windows
```

To include test dependencies:

```bash
vcpkg install --x-feature=tests --triplet=x64-linux
```

This will install all dependencies including Google Test for running unit tests.

### 4. Install Python Dependencies

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
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel $(nproc)
```

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
- ✅ vcpkg integration
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
- ✅ Excludes build directories and vcpkg files
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

Tests use Google Test framework. Install test dependencies with:

```bash
vcpkg install --x-feature=tests
```

The `tests` feature in `vcpkg.json` includes:
- **Google Test**: Unit testing framework

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

## Dependencies

### Core Dependencies

Managed via `vcpkg.json`:

- **fmt**: Fast formatting library
- **nlohmann-json**: JSON parsing and generation
- **spdlog**: Fast logging library
- **cpp-httplib**: HTTP client library with OpenSSL and Brotli support
- **openssl**: Cryptographic library

### Test Dependencies

Available via the `tests` feature:

- **gtest**: Google's C++ testing framework

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
