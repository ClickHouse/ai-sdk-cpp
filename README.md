# AI SDK CPP

The AI SDK CPP is a modern C++ toolkit designed to help you build AI-powered applications with popular model providers like OpenAI and Anthropic. It provides a unified, easy-to-use API that abstracts away the complexity of different provider implementations.

## Motivation

C++ developers have long lacked a first-class, convenient way to interact with modern AI services like OpenAI, Anthropic, and others. AI SDK CPP bridges this gap by providing:

- **Unified API**: Work with multiple AI providers through a single, consistent interface
- **Modern C++**: Built with C++20 features for clean, expressive code
- **Minimal Dependencies**: Minimal external dependencies for easy integration

To learn more about how to use AI SDK CPP, check out our [Documentation](docs/).

## Installation

You will need a C++20 compatible compiler and CMake 3.16+ installed on your development machine.

## Usage

### Core API

The AI SDK CPP Core module provides a unified API to interact with model providers like OpenAI and Anthropic.

#### OpenAI Integration

```cpp
#include <ai/openai.h>
#include <ai/generate.h>
#include <iostream>

int main() {
    // Ensure OPENAI_API_KEY environment variable is set
    auto client = ai::openai::create_client();
    
    auto result = client.generate_text({
        .model = ai::openai::models::kGpt4o, // this can also be a string like "gpt-4o"
        .system = "You are a friendly assistant!",
        .prompt = "Why is the sky blue?"
    });
    
    if (result) {
        std::cout << result->text << std::endl;
    }
    
    return 0;
}
```

#### Anthropic Integration

```cpp
#include <ai/anthropic.h>
#include <ai/generate.h>
#include <iostream>

int main() {
    // Ensure ANTHROPIC_API_KEY environment variable is set
    auto client = ai::anthropic::create_client();
    auto result = client.generate_text({
        .model = ai::anthropic::models::kClaude35Sonnet,
        .system = "You are a helpful assistant.",
        .prompt = "Explain quantum computing in simple terms."
    });
    
    if (result) {
        std::cout << result->text << std::endl;
    }
    
    return 0;
}
```

#### Streaming Responses

```cpp
#include <ai/openai.h>
#include <ai/stream.h>
#include <iostream>

int main() {
    auto client = ai::openai::create_client();
    
    auto stream = client.stream_text({
        .model = ai::openai::models::kGpt4o, // this can also be a string like "gpt-4o"
        .system = "You are a helpful assistant.",
        .prompt = "Write a short story about a robot."
    });
    
    for (const auto& chunk : stream) {
        if (chunk.text) {
            std::cout << chunk.text.value() << std::flush;
        }
    }
    
    return 0;
}
```

#### Multi-turn Conversations

```cpp
#include <ai/openai.h>
#include <ai/generate.h>
#include <iostream>

int main() {
    auto client = ai::openai::create_client();
    
    ai::Messages messages = {
        {"system", "You are a helpful math tutor."},
        {"user", "What is 2 + 2?"},
        {"assistant", "2 + 2 equals 4."},
        {"user", "Now what is 4 + 4?"}
    };
    
    auto result = client.generate_text({
        .model = ai::openai::models::kGpt4o, // this can also be a string like "gpt-4o"
        .messages = messages
    });
    
    if (result) {
        std::cout << result->text << std::endl;
    }
    
    return 0;
}
```

## Features

### Currently Supported

- ✅ **Text Generation**: Generate text completions with OpenAI and Anthropic models
- ✅ **Streaming**: Real-time streaming of generated content
- ✅ **Multi-turn Conversations**: Support for conversation history
- ✅ **Error Handling**: Comprehensive error handling with optional types

### Coming Soon

- 🚧 **Tool Calling**: Function calling and tool integration
- 🚧 **Additional Providers**: Google, Cohere, and other providers
- 🚧 **Embeddings**: Text embedding support
- 🚧 **Image Generation**: Support for image generation models

## Examples

Check out our [examples directory](examples/) for more comprehensive usage examples:

- [Basic Chat Application](examples/basic_chat.cc)
- [Streaming Chat](examples/streaming_chat.cc)
- [Multi-provider Comparison](examples/multi_provider.cc)
- [Error Handling](examples/error_handling.cc)

## Documentation

Comprehensive documentation is available in the [`docs/`](docs/) directory:

- [Getting Started Guide](docs/getting-started.md)
- [API Reference](docs/api-reference.md)
- [Provider Configuration](docs/providers.md)
- [Advanced Usage](docs/advanced.md)
- [Migration Guide](docs/migration.md)

## Requirements

- **C++ Standard**: C++20 or higher
- **CMake**: 3.16 or higher

## Acknowledgments

Inspired by the excellent [Vercel AI SDK](https://github.com/vercel/ai) for TypeScript/JavaScript developers.
