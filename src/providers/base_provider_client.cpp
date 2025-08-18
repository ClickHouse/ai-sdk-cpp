#include "base_provider_client.h"

#include "ai/logger.h"
#include "ai/tools.h"

namespace ai {
namespace providers {

BaseProviderClient::BaseProviderClient(
    const ProviderConfig& config,
    std::unique_ptr<RequestBuilder> request_builder,
    std::unique_ptr<ResponseParser> response_parser)
    : config_(config),
      request_builder_(std::move(request_builder)),
      response_parser_(std::move(response_parser)) {
  // Initialize HTTP handler with parsed config
  auto http_config = http::HttpRequestHandler::parse_base_url(config.base_url);

  // Apply custom retry config if provided
  if (config.retry_config.has_value()) {
    http_config.retry_config = config.retry_config.value();
  }

  http_handler_ = std::make_unique<http::HttpRequestHandler>(http_config);

  ai::logger::log_debug(
      "BaseProviderClient initialized - base_url: {}, endpoint: {}",
      config.base_url, config.endpoint_path);
}

GenerateResult BaseProviderClient::generate_text(
    const GenerateOptions& options) {
  ai::logger::log_debug(
      "Starting text generation - model: {}, prompt length: {}, tools: {}, "
      "max_steps: {}",
      options.model, options.prompt.length(), options.tools.size(),
      options.max_steps);

  // Check if multi-step tool calling is enabled
  if (options.has_tools() && options.is_multi_step()) {
    ai::logger::log_debug("Using multi-step tool calling with {} tools",
                          options.tools.size());

    // Use MultiStepCoordinator for complex workflows
    return MultiStepCoordinator::execute_multi_step(
        options, [this](const GenerateOptions& step_options) {
          return this->generate_text_single_step(step_options);
        });
  } else {
    // Single step generation
    return generate_text_single_step(options);
  }
}

GenerateResult BaseProviderClient::generate_text_single_step(
    const GenerateOptions& options) {
  try {
    // Build request JSON using the provider-specific builder
    auto request_json = request_builder_->build_request_json(options);
    std::string json_body = request_json.dump();
    ai::logger::log_debug("Request JSON built: {}", json_body);

    // Build headers
    auto headers = request_builder_->build_headers(config_);

    // Make the request
    auto result =
        http_handler_->post(config_.endpoint_path, headers, json_body);

    if (!result.is_success()) {
      // Parse error response using provider-specific parser
      if (result.provider_metadata.has_value()) {
        int status_code = std::stoi(result.provider_metadata.value());
        return response_parser_->parse_error_completion_response(
            status_code, result.error.value_or(""));
      }
      return result;
    }

    // Parse the response JSON from result.text
    nlohmann::json json_response;
    try {
      json_response = nlohmann::json::parse(result.text);
    } catch (const nlohmann::json::exception& e) {
      ai::logger::log_error("Failed to parse response JSON: {}", e.what());
      ai::logger::log_debug("Raw response text: {}", result.text);
      return GenerateResult("Failed to parse response: " +
                            std::string(e.what()));
    }

    ai::logger::log_info(
        "Text generation successful - model: {}, response_id: {}",
        options.model, json_response.value("id", "unknown"));

    // Parse using provider-specific parser
    auto parsed_result =
        response_parser_->parse_success_completion_response(json_response);

    if (parsed_result.has_tool_calls()) {
      ai::logger::log_debug("Model made {} tool calls",
                            parsed_result.tool_calls.size());
    }

    // Execute tools if the model made tool calls
    if (parsed_result.has_tool_calls() && options.has_tools()) {
      ai::logger::log_debug("Model made {} tool calls, executing them",
                            parsed_result.tool_calls.size());

      auto tool_results = ToolExecutor::execute_tools_with_options(
          parsed_result.tool_calls, options, false);

      parsed_result.tool_results = tool_results;
      ai::logger::log_debug("Executed {} tools", tool_results.size());

      // Check if any tool execution failed
      int failed_count = 0;
      for (const auto& result : tool_results) {
        if (!result.is_success()) {
          failed_count++;
          ai::logger::log_warn("Tool '{}' execution failed: {}",
                               result.tool_name, result.error_message());
        }
      }

      if (failed_count > 0) {
        ai::logger::log_info(
            "Some tools failed ({}/{}), but overall result is still successful",
            failed_count, tool_results.size());
      }
    }

    return parsed_result;

  } catch (const std::exception& e) {
    ai::logger::log_error("Exception during text generation: {}", e.what());
    return GenerateResult(std::string("Exception: ") + e.what());
  }
}

StreamResult BaseProviderClient::stream_text(const StreamOptions& options) {
  // This needs to be implemented with provider-specific stream implementations
  // For now, return an error
  ai::logger::log_error("Streaming not yet implemented in BaseProviderClient");
  return StreamResult();
}

EmbeddingResult BaseProviderClient::embedding(const EmbeddingOptions& options) {
  // This needs to be implemented with provider-specific stream implementations
  // For now, return an error
  ai::logger::log_error("Embedding not yet implemented in BaseProviderClient");
  return EmbeddingResult();
}

}  // namespace providers
}  // namespace ai