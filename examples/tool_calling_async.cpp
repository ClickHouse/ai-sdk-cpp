/**
 * Async Tool Calling Example - AI SDK C++
 *
 * This example demonstrates asynchronous tool calling functionality.
 * It shows how to:
 * - Define async tools that return futures
 * - Handle long-running operations
 * - Execute tools in parallel
 * - Mix sync and async tools
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   export ANTHROPIC_API_KEY=your_key_here
 *   ./tool_calling_async
 */

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <ai/anthropic.h>
#include <ai/openai.h>
#include <ai/tools.h>

// Async tool: Simulate web API call
std::future<ai::JsonValue> fetch_news_async(
    const ai::JsonValue& args,
    const ai::ToolExecutionContext& context) {
  std::string category = args["category"].get<std::string>();

  return std::async(std::launch::async, [category]() {
    std::cout << "📰 Fetching news for category: " << category << " (async)..."
              << std::endl;

    // Simulate network delay
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ai::JsonValue news_articles = ai::JsonValue::array();
    if (category == "tech") {
      news_articles = {"AI breakthrough in quantum computing",
                       "New smartphone features announced",
                       "Cloud computing trends for 2024"};
    } else if (category == "sports") {
      news_articles = {"Championship game results",
                       "Olympic preparations underway",
                       "New stadium construction begins"};
    } else {
      news_articles = {"General news update 1", "General news update 2",
                       "General news update 3"};
    }

    std::cout << "📰 News fetch completed for: " << category << std::endl;

    return ai::JsonValue{{"category", category},
                         {"articles", news_articles},
                         {"fetch_time_ms", 1000}};
  });
}

// Async tool: Simulate database query
std::future<ai::JsonValue> query_database_async(
    const ai::JsonValue& args,
    const ai::ToolExecutionContext& context) {
  std::string query = args["query"].get<std::string>();

  return std::async(std::launch::async, [query]() {
    std::cout << "🗄️  Executing database query: " << query << " (async)..."
              << std::endl;

    // Simulate database processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    ai::JsonValue results = ai::JsonValue::array();
    if (query.find("users") != std::string::npos) {
      results = {{{"id", 1}, {"name", "John"}, {"active", true}},
                 {{"id", 2}, {"name", "Jane"}, {"active", true}},
                 {{"id", 3}, {"name", "Bob"}, {"active", false}}};
    } else if (query.find("orders") != std::string::npos) {
      results = {
          {{"order_id", 100}, {"amount", 99.99}, {"status", "shipped"}},
          {{"order_id", 101}, {"amount", 149.99}, {"status", "pending"}}};
    }

    std::cout << "🗄️  Database query completed: " << query << std::endl;

    return ai::JsonValue{
        {"query", query}, {"results", results}, {"execution_time_ms", 800}};
  });
}

// Sync tool for comparison: Quick calculation
ai::JsonValue calculate_stats(const ai::JsonValue& args,
                              const ai::ToolExecutionContext& context) {
  ai::JsonValue data = args["data"];

  std::cout << "🧮 Calculating statistics (sync)..." << std::endl;
  std::cout << "   Data received: " << data.dump() << std::endl;

  if (!data.is_array()) {
    return ai::JsonValue{{"error", "Data must be an array"}};
  }

  double sum = 0.0;
  int count = 0;

  for (const auto& item : data) {
    if (item.is_number()) {
      sum += item.get<double>();
      count++;
    }
  }

  double average = count > 0 ? sum / count : 0.0;

  std::cout << "   Statistics calculated: sum=" << sum << ", count=" << count
            << ", avg=" << average << std::endl;

  return ai::JsonValue{{"sum", sum}, {"count", count}, {"average", average}};
}

// Async tool: File processing simulation
std::future<ai::JsonValue> process_file_async(
    const ai::JsonValue& args,
    const ai::ToolExecutionContext& context) {
  std::string filename = args["filename"].get<std::string>();
  std::string operation = args["operation"].get<std::string>();

  return std::async(std::launch::async, [filename, operation]() {
    std::cout << "📄 Processing file: " << filename
              << " (operation: " << operation << ", async)..." << std::endl;

    // Simulate file processing time based on operation
    int delay_ms = 500;
    if (operation == "compress")
      delay_ms = 1200;
    else if (operation == "analyze")
      delay_ms = 900;
    else if (operation == "backup")
      delay_ms = 600;

    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

    ai::JsonValue result = {{"filename", filename},
                            {"operation", operation},
                            {"status", "completed"},
                            {"processing_time_ms", delay_ms}};

    if (operation == "analyze") {
      result["file_size"] = "2.5 MB";
      result["line_count"] = 1250;
    } else if (operation == "compress") {
      result["original_size"] = "5.0 MB";
      result["compressed_size"] = "1.8 MB";
      result["compression_ratio"] = "36%";
    }

    std::cout << "📄 File processing completed: " << filename << std::endl;

    return result;
  });
}

int main() {
  std::cout << "AI SDK C++ - Async Tool Calling Example\n";
  std::cout << "========================================\n\n";

  // Create OpenAI client
  auto client = ai::openai::create_client();

  // Define async tools
  ai::ToolSet tools = {
      {"fetch_news",
       ai::create_simple_async_tool(
           "fetch_news", "Fetch latest news articles for a given category",
           {{"category", "string"}}, fetch_news_async)},
      {"query_database",
       ai::create_simple_async_tool(
           "query_database", "Execute a database query and return results",
           {{"query", "string"}}, query_database_async)},
      {"calculate_stats",
       ai::create_tool(
           "Calculate statistics (sum, count, average) for an array of numbers",
           ai::JsonValue{
               {"type", "object"},
               {"properties",
                {{"data",
                  {{"type", "array"},
                   {"items", {{"type", "number"}}},
                   {"description",
                    "Array of numbers to calculate statistics for"}}}}},
               {"required", {"data"}}},
           calculate_stats)},
      {"process_file", ai::create_simple_async_tool(
                           "process_file",
                           "Process a file with the specified operation "
                           "(analyze, compress, backup)",
                           {{"filename", "string"}, {"operation", "string"}},
                           process_file_async)}};

  // Example 1: Single async tool call
  std::cout << "1. Single Async Tool Example:\n";
  std::cout << "Question: Get me the latest tech news\n\n";

  auto start_time = std::chrono::high_resolution_clock::now();

  ai::GenerateOptions options1;
  options1.model = ai::openai::models::kGpt4o;
  options1.prompt = "Get me the latest tech news articles";
  options1.tools = tools;
  options1.max_tokens = 200;

  auto result1 = client.generate_text(options1);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  if (result1) {
    std::cout << "Assistant: " << result1.text << "\n";
    std::cout << "Total execution time: " << duration.count() << "ms\n";
    std::cout << "Usage: " << result1.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result1.error_message() << "\n\n";
  }

  // Example 2: Multiple async tools in parallel
  std::cout << "2. Multiple Async Tools (Parallel Execution):\n";
  std::cout << "Question: Fetch sports news, query user database, and process "
               "a file\n\n";

  start_time = std::chrono::high_resolution_clock::now();

  ai::GenerateOptions options2;
  options2.model = ai::openai::models::kGpt4o;
  options2.prompt = R"(
    Please help me with these tasks:
    1. Fetch the latest sports news
    2. Query the database for active users
    3. Process the file 'data.csv' with analyze operation
    
    Use the available tools to get this information.
  )";
  options2.tools = tools;
  options2.max_tokens = 300;

  auto result2 = client.generate_text(options2);

  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                   start_time);

  if (result2) {
    std::cout << "Assistant: " << result2.text << "\n";
    std::cout << "Tool calls made: " << result2.tool_calls.size() << "\n";
    std::cout << "Total execution time: " << duration.count() << "ms\n";
    std::cout << "(Note: Async tools should execute in parallel, reducing "
                 "total time)\n";
    std::cout << "Usage: " << result2.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result2.error_message() << "\n\n";
  }

  // Example 3: Mix of sync and async tools
  std::cout << "3. Mixed Sync/Async Tools Example:\n";
  std::cout
      << "Question: Get news, calculate stats, and process multiple files\n\n";

  start_time = std::chrono::high_resolution_clock::now();

  ai::GenerateOptions options3;
  options3.model = ai::openai::models::kGpt4o;
  options3.prompt = R"(
    Please help me with these tasks:
    1. Fetch tech news articles
    2. Calculate statistics for these numbers: [10, 20, 30, 40, 50]
    3. Process 'report.pdf' with compress operation
    4. Process 'backup.zip' with backup operation
    
    Complete all these tasks and summarize the results.
  )";
  options3.tools = tools;
  options3.max_tokens = 400;

  auto result3 = client.generate_text(options3);

  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                   start_time);

  if (result3) {
    std::cout << "Assistant: " << result3.text << "\n";
    std::cout << "Tool calls made: " << result3.tool_calls.size() << "\n";
    std::cout << "Total execution time: " << duration.count() << "ms\n";

    // Show tool execution breakdown
    if (result3.has_tool_results()) {
      std::cout << "\nTool Execution Results:\n";
      for (const auto& result : result3.tool_results) {
        std::cout << "  - " << result.tool_name << ": ";
        if (result.is_success()) {
          if (result.result.contains("processing_time_ms")) {
            std::cout << "completed in " << result.result["processing_time_ms"]
                      << "ms";
          } else if (result.result.contains("fetch_time_ms")) {
            std::cout << "completed in " << result.result["fetch_time_ms"]
                      << "ms";
          } else {
            std::cout << "completed (sync)";
          }
        } else {
          std::cout << "failed - " << result.error_message();
        }
        std::cout << "\n";
      }
    }

    std::cout << "Usage: " << result3.usage.total_tokens << " tokens\n\n";
  } else {
    std::cout << "Error: " << result3.error_message() << "\n\n";
  }

  std::cout << "Async tool calling examples completed!\n";
  std::cout << "\nKey features demonstrated:\n";
  std::cout << "  ✓ Asynchronous tool execution with std::future\n";
  std::cout << "  ✓ Parallel execution of multiple async tools\n";
  std::cout << "  ✓ Mix of synchronous and asynchronous tools\n";
  std::cout << "  ✓ Performance benefits of parallel execution\n";
  std::cout << "  ✓ Long-running operation handling\n";
  std::cout << "  ✓ Execution time monitoring\n";

  // Test with Anthropic
  std::cout << "\n\n4. Testing with Anthropic:\n";
  std::cout << "========================================\n";

  auto anthropic_client = ai::anthropic::create_client();

  std::cout << "Testing async tools with Claude Sonnet 4.5\n\n";

  start_time = std::chrono::high_resolution_clock::now();

  ai::GenerateOptions anthropic_options;
  anthropic_options.model = ai::anthropic::models::kClaudeSonnet45;
  anthropic_options.prompt = R"(
    Please help me with these THREE tasks. You MUST use the tools to complete ALL of them:
    1. Use the fetch_news tool to get tech news articles  
    2. Use the calculate_stats tool to calculate statistics for these numbers: [15, 25, 35, 45, 55]
    3. Use the query_database tool to query for orders
    
    Important: Call all three tools to complete all three tasks.
  )";
  anthropic_options.tools = tools;
  anthropic_options.max_tokens = 600;
  anthropic_options.max_steps = 3;  // Enable multi-step for Anthropic

  auto anthropic_result = anthropic_client.generate_text(anthropic_options);

  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                   start_time);

  if (anthropic_result) {
    std::cout << "Assistant (Claude): " << anthropic_result.text << "\n";
    std::cout << "Tool calls made: " << anthropic_result.tool_calls.size()
              << "\n";
    std::cout << "Total execution time: " << duration.count() << "ms\n";

    // Show multi-step information if available
    if (anthropic_result.is_multi_step()) {
      std::cout << "\nMulti-step execution details:\n";
      std::cout << "Total steps: " << anthropic_result.steps.size() << "\n";

      for (size_t i = 0; i < anthropic_result.steps.size(); ++i) {
        const auto& step = anthropic_result.steps[i];
        std::cout << "\nStep " << (i + 1) << ":\n";
        if (!step.text.empty()) {
          std::cout << "  Text: " << step.text.substr(0, 100) << "...\n";
        }
        std::cout << "  Tool calls in step: " << step.tool_calls.size() << "\n";
        for (const auto& call : step.tool_calls) {
          std::cout << "    - " << call.tool_name << " (id: " << call.id
                    << ")\n";
        }
      }

      std::cout << "\nAll tool calls across steps:\n";
      auto all_calls = anthropic_result.get_all_tool_calls();
      for (const auto& call : all_calls) {
        std::cout << "  - " << call.tool_name
                  << " with args: " << call.arguments.dump() << "\n";
      }
    }

    if (anthropic_result.has_tool_results()) {
      std::cout << "\nTool Execution Results:\n";
      auto all_results = anthropic_result.get_all_tool_results();
      for (const auto& result : all_results) {
        std::cout << "  - " << result.tool_name << ": ";
        if (result.is_success()) {
          if (result.result.contains("processing_time_ms")) {
            std::cout << "completed in " << result.result["processing_time_ms"]
                      << "ms";
          } else if (result.result.contains("fetch_time_ms")) {
            std::cout << "completed in " << result.result["fetch_time_ms"]
                      << "ms";
          } else if (result.result.contains("execution_time_ms")) {
            std::cout << "completed in " << result.result["execution_time_ms"]
                      << "ms";
          } else {
            std::cout << "completed (sync)";
          }
        } else {
          std::cout << "failed - " << result.error_message();
        }
        std::cout << "\n";
      }
    }

    std::cout << "Usage: " << anthropic_result.usage.total_tokens
              << " tokens\n";
  } else {
    std::cout << "Error: " << anthropic_result.error_message() << "\n";
  }

  std::cout << "\nAnthropic async tool calling test completed!\n";

  return 0;
}