# AI SDK CPP

The AI SDK CPP is a modern C++ toolkit designed to help you build AI-powered applications with popular model providers like OpenAI and Anthropic. It provides a unified, easy-to-use API that abstracts away the complexity of different provider implementations.

## Motivation

C++ developers have long lacked a first-class, convenient way to interact with modern AI services like OpenAI, Anthropic, and others. AI SDK CPP bridges this gap by providing:

- **Unified API**: Work with multiple AI providers through a single, consistent interface
- **Modern C++**: Built with C++20 features for clean, expressive code
- **Minimal Dependencies**: Minimal external dependencies for easy integration

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
        .model = ai::anthropic::models::kClaudeSonnet45,
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

#### Tool Calling

The AI SDK CPP supports function calling, allowing models to interact with external systems and APIs.

```cpp
#include <ai/openai.h>
#include <ai/generate.h>
#include <ai/tools.h>
#include <iostream>

// Define a tool function
ai::JsonValue get_weather(const ai::JsonValue& args, const ai::ToolExecutionContext& context) {
    std::string location = args["location"].get<std::string>();
    
    // Your weather API logic here
    return ai::JsonValue{
        {"location", location},
        {"temperature", 72},
        {"condition", "Sunny"}
    };
}

int main() {
    auto client = ai::openai::create_client();
    
    // Create tools
    ai::ToolSet tools = {
        {"weather", ai::create_simple_tool(
            "weather",
            "Get current weather for a location", 
            {{"location", "string"}},
            get_weather
        )}
    };
    
    auto result = client.generate_text({
        .model = ai::openai::models::kGpt4o,
        .prompt = "What's the weather like in San Francisco?",
        .tools = tools,
        .max_steps = 3  // Enable multi-step tool calling
    });
    
    if (result) {
        std::cout << result->text << std::endl;
        
        // Inspect tool calls and results
        for (const auto& call : result->tool_calls) {
            std::cout << "Tool: " << call.tool_name 
                      << ", Args: " << call.arguments.dump() << std::endl;
        }
    }
    
    return 0;
}
```

#### Async Tool Calling

For long-running operations, you can define asynchronous tools:

```cpp
#include <future>
#include <thread>
#include <chrono>

// Async tool that returns a future
std::future<ai::JsonValue> fetch_data_async(const ai::JsonValue& args, const ai::ToolExecutionContext& context) {
    return std::async(std::launch::async, [args]() {
        // Simulate async operation
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        return ai::JsonValue{
            {"data", "Fetched from API"},
            {"timestamp", std::time(nullptr)}
        };
    });
}

int main() {
    auto client = ai::openai::create_client();
    
    ai::ToolSet tools = {
        {"fetch_data", ai::create_simple_async_tool(
            "fetch_data",
            "Fetch data from external API",
            {{"endpoint", "string"}},
            fetch_data_async
        )}
    };
    
    // Multiple async tools will execute in parallel
    auto result = client.generate_text({
        .model = ai::openai::models::kGpt4o,
        .prompt = "Fetch data from the user and product APIs",
        .tools = tools
    });
    
    return 0;
}
```

#### Custom Retry Configuration

Configure retry behavior for handling transient failures:

```cpp
#include <ai/openai.h>
#include <ai/retry/retry_policy.h>

int main() {
    // Configure custom retry behavior
    ai::retry::RetryConfig retry_config;
    retry_config.max_retries = 5;        // More retries for unreliable networks
    retry_config.initial_delay = std::chrono::milliseconds(1000);
    retry_config.backoff_factor = 1.5;   // Gentler backoff
    
    // Create client with custom retry configuration
    auto client = ai::openai::create_client(
        "your-api-key",
        "https://api.openai.com",
        retry_config
    );
    
    // The client will automatically retry on transient failures:
    // - Network errors
    // - HTTP 408, 409, 429 (rate limits), and 5xx errors
    auto result = client.generate_text({
        .model = ai::openai::models::kGpt4o,
        .prompt = "Hello, world!"
    });
    
    return 0;
}
```

#### Using OpenAI-Compatible APIs (OpenRouter, etc.)

The OpenAI client can be used with any OpenAI-compatible API by specifying a custom base URL. This allows you to use alternative providers like OpenRouter, which offers access to multiple models through a unified API.

```cpp
#include <ai/openai.h>
#include <ai/generate.h>
#include <iostream>
#include <cstdlib>

int main() {
    // Get API key from environment variable
    const char* api_key = std::getenv("OPENROUTER_API_KEY");
    if (!api_key) {
        std::cerr << "Please set OPENROUTER_API_KEY environment variable\n";
        return 1;
    }
    
    // Create client with OpenRouter's base URL
    auto client = ai::openai::create_client(
        api_key,
        "https://openrouter.ai/api"  // OpenRouter's OpenAI-compatible endpoint
    );
    
    // Use any model available on OpenRouter
    auto result = client.generate_text({
        .model = "anthropic/claude-3.5-sonnet",  // or "meta-llama/llama-3.1-8b-instruct", etc.
        .system = "You are a helpful assistant.",
        .prompt = "What are the benefits of using OpenRouter?"
    });
    
    if (result) {
        std::cout << result->text << std::endl;
    }
    
    return 0;
}
```

This approach works with any OpenAI-compatible API provider. Simply provide:
1. Your provider's API key
2. The provider's base URL endpoint
3. Model names as specified by your provider

See the [OpenRouter example](examples/openrouter_example.cpp) for a complete demonstration.

## Features

### Currently Supported

- ✅ **Text Generation**: Generate text completions with OpenAI and Anthropic models
- ✅ **Streaming**: Real-time streaming of generated content
- ✅ **Multi-turn Conversations**: Support for conversation history
- ✅ **Error Handling**: Comprehensive error handling with optional types

### Recently Added

- ✅ **Tool Calling**: Function calling and tool integration with multi-step support
- ✅ **Async Tools**: Asynchronous tool execution with parallel processing
- ✅ **Configurable Retries**: Customizable retry behavior with exponential backoff

### Coming Soon

- 🚧 **Additional Providers**: Google, Cohere, and other providers
- 🚧 **Embeddings**: Text embedding support
- 🚧 **Image Generation**: Support for image generation models

## Examples

Check out our [examples directory](examples/) for more comprehensive usage examples:

- [Basic Chat Application](examples/basic_chat.cpp)
- [Streaming Chat](examples/streaming_chat.cpp)
- [Multi-provider Comparison](examples/multi_provider.cpp)
- [Error Handling](examples/error_handling.cpp)
- [Retry Configuration](examples/retry_config_example.cpp)
- [Basic Tool Calling](examples/tool_calling_basic.cpp)
- [Multi-Step Tool Workflows](examples/tool_calling_multistep.cpp)
- [Async Tool Execution](examples/tool_calling_async.cpp)
- [OpenRouter Integration](examples/openrouter_example.cpp) - Using OpenAI-compatible APIs


## Requirements

- **C++ Standard**: C++20 or higher
- **CMake**: 3.16 or higher

## Dependencies and Modifications

### nlohmann/json (Patched)

This project uses a patched version of nlohmann/json to remove the dependency on `localeconv()`, which is not thread-safe. The patch ensures:

- **Thread Safety**: Eliminates calls to the non-thread-safe `localeconv()` function, allowing downstream users to safely use the library in multi-threaded environments without worrying about locale-related race conditions
- **Consistent Behavior**: Always uses '.' as the decimal point separator regardless of system locale
- **Simplified Integration**: Downstream users don't need to implement locale synchronization or worry about thread safety issues

This modification improves both safety and portability of the JSON library in concurrent applications.

## Acknowledgments

Inspired by the excellent [Vercel AI SDK](https://github.com/vercel/ai) for TypeScript/JavaScript developers.
