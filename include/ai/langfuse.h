#pragma once

/// Langfuse tracing for ai-sdk-cpp.
///
/// Records traces, generations, and tool spans for `ai::Client::generate_text`
/// calls and exports them as OpenTelemetry spans (OTLP/HTTP with JSON
/// encoding) to Langfuse's `/api/public/otel/v1/traces` endpoint. This is the
/// ingestion API of Langfuse v4 (Langfuse Cloud) and is also accepted by
/// self-hosted Langfuse v3.22.0 and later.
///
/// Each trace is exported as a root span carrying the trace-level attributes
/// (name, user, session, tags, metadata, input, output); generations and tool
/// calls are its descendants. Trace-level attributes are propagated to every
/// span, as Langfuse v4 requires for filtering.
///
/// Usage:
/// ```cpp
/// #include <ai/ai.h>
/// #include <ai/langfuse.h>
///
/// ai::langfuse::Tracer tracer({
///     .host = "https://cloud.langfuse.com",
///     .public_key = std::getenv("LANGFUSE_PUBLIC_KEY"),
///     .secret_key = std::getenv("LANGFUSE_SECRET_KEY"),
/// });
///
/// auto trace = tracer.start_trace("sql-generation");
/// trace->set_input(user_prompt);
///
/// ai::GenerateOptions options{...};
/// auto result = ai::langfuse::generate_text(client, std::move(options),
/// *trace);
///
/// trace->set_output(result.text);
/// trace->end();  // synchronous flush
/// ```

#include "types/client.h"
#include "types/generate_options.h"
#include "types/tool.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace ai {
namespace langfuse {

using JsonValue = nlohmann::json;

struct Config {
  /// Base URL of the Langfuse instance, e.g. "https://cloud.langfuse.com" or
  /// "https://us.cloud.langfuse.com" or a self-hosted address.
  std::string host = "https://cloud.langfuse.com";

  /// Public API key (`pk-lf-...`).
  std::string public_key;

  /// Secret API key (`sk-lf-...`).
  std::string secret_key;

  /// Optional release identifier to attach to all traces (commit sha, etc.).
  std::string release;

  /// Environment to attach to all traces (Langfuse default is "default").
  std::string environment = "default";

  /// HTTP timeouts for the OTLP export request.
  int connection_timeout_sec = 10;
  int read_timeout_sec = 30;

  /// Error policy for `Trace::end()`. Strict (the default) reports HTTP/JSON
  /// failures via the return value so misconfigurations surface immediately;
  /// kBestEffort swallows them (failures are still logged).
  enum class ErrorPolicy { kStrict, kBestEffort };
  ErrorPolicy error_policy = ErrorPolicy::kStrict;
};

class Trace;

/// Optional fields settable at trace-creation time. Mirrors the
/// struct-of-options style used elsewhere in the SDK (GenerateOptions, Config).
struct TraceOptions {
  std::optional<JsonValue> input;
  std::optional<JsonValue> output;
  std::optional<std::string> user_id;
  std::optional<std::string> session_id;
  std::optional<JsonValue> metadata;
  std::vector<std::string> tags;
};

/// Tracer holds the Langfuse credentials and is the entry point for creating
/// traces. Construct one per process; it is safe to create multiple traces
/// concurrently from a single Tracer.
class Tracer {
 public:
  explicit Tracer(Config config);
  ~Tracer();  // Out-of-line: HttpState is forward-declared; its destructor
              // needs the complete type, which lives in tracer.cpp.

  /// True iff host, public_key and secret_key are set.
  bool is_valid() const;

  const Config& config() const { return config_; }

  /// Start a new trace. Returns a shared handle so callbacks attached via
  /// `Trace::instrument()` can keep it alive until generate_text returns.
  std::shared_ptr<Trace> start_trace(const std::string& name,
                                     TraceOptions opts = {});

  /// POST a JSON array of OTLP spans (the JSON encoding of
  /// `opentelemetry.proto.trace.v1.Span`) to Langfuse's OTLP endpoint, wrapped
  /// in a single `resourceSpans` / `scopeSpans` entry. Returns true on 2xx
  /// without rejected spans. Public so advanced callers can export their own
  /// spans; most users should rely on `Trace::end()`.
  bool export_spans(const JsonValue& spans);

 private:
  // Lazily constructed httplib client + base path + Basic-auth header. Cached
  // to avoid TLS handshake per export_spans call. Guarded by mu_.
  struct HttpState;
  HttpState& http_state();

  Config config_;
  std::mutex mu_;
  std::unique_ptr<HttpState> http_;
};

/// A Trace owns a list of pending spans and exports them all to Langfuse on
/// `end()`. Use one Trace per logical request (per call to a
/// user-facing API like `generateSQL`).
///
/// Methods are thread-safe: callbacks fired from generate_text's worker
/// threads can record into the same Trace concurrently.
class Trace : public std::enable_shared_from_this<Trace> {
 public:
  /// `id` is the OpenTelemetry trace id: 32 lowercase hex characters. It is
  /// also the trace id shown by Langfuse.
  Trace(Tracer& tracer, std::string id, std::string name);

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }

  void set_input(JsonValue input);
  void set_output(JsonValue output);
  void set_user_id(std::string user_id);
  void set_session_id(std::string session_id);
  /// Trace metadata; should be a JSON object (Langfuse ignores other values).
  void set_metadata(JsonValue metadata);
  void add_tag(std::string tag);

  /// Attach Langfuse callbacks to `options`. Records:
  ///  - a generation observation (model, model parameters, input) as a child
  ///    of the trace's root span
  ///  - one tool span per tool call (input=args, output=result)
  ///
  /// Existing callbacks on `options` are preserved (chained, called after the
  /// tracer's bookkeeping). Caller must invoke `finish_generation()` after
  /// `generate_text` returns to fill in end_time, output, and usage.
  void instrument(GenerateOptions& options,
                  const std::string& generation_name = "generate_text");

  /// Record the final output, usage, and finish_reason of the most recent
  /// `instrument()`-ed generation. Safe to call once per `instrument()`.
  void finish_generation(const GenerateResult& result);

  /// Export all accumulated spans to Langfuse. Idempotent; subsequent calls
  /// are no-ops. After end(), further set_* / instrument / finish_generation
  /// calls become no-ops to prevent silently dropping spans. A generation
  /// without `finish_generation()` and tool spans without a finish callback
  /// are closed at the time of `end()`. Returns true on
  /// success (or when Config::error_policy is kBestEffort and the request
  /// failed).
  bool end();

  /// Generate a new UUID v4. Trace and span ids are OpenTelemetry ids; mint
  /// them with `new_trace_id` and `new_span_id`.
  static std::string new_uuid();

  /// Generate a random OpenTelemetry trace id (32 lowercase hex characters).
  /// `Tracer::start_trace` uses it for `Trace::id`.
  static std::string new_trace_id();

  /// Generate a random OpenTelemetry span id (16 lowercase hex characters).
  static std::string new_span_id();

  /// Format a system_clock time_point as unix nanoseconds in a decimal string,
  /// the OTLP/JSON encoding of `startTimeUnixNano` / `endTimeUnixNano`.
  static std::string to_unix_nanos(std::chrono::system_clock::time_point t);

  /// Format a system_clock time_point as RFC 3339 / ISO 8601 with millisecond
  /// precision and trailing 'Z'. Public so callers can stamp custom events.
  static std::string to_iso8601(std::chrono::system_clock::time_point t);

 private:
  struct PendingGeneration {
    std::string id;  // OpenTelemetry span id
    std::string name;
    std::chrono::system_clock::time_point start_time;
    JsonValue input;
    std::string model;
    JsonValue model_parameters;
    bool finalized = false;
  };

  // Records a tool-call span. Called from on_tool_call_start.
  void record_tool_call_start(const ToolCall& call);
  // Closes a tool-call span. Called from on_tool_call_finish.
  void record_tool_call_finish(const ToolResult& result);

  // Builds the span of `active_generation_` ending at `end_nanos` and marks
  // the generation finalized. `result` (output, usage, status) may be null
  // for a generation that was never finished. Caller must hold mu_.
  JsonValue build_generation_span(const std::string& end_nanos,
                                  const GenerateResult* result);

  // Builds the root span that represents the trace itself, with the current
  // input/output. Caller must hold mu_.
  JsonValue build_root_span(const std::string& end_nanos) const;

  // Adds the attributes shared by all spans of the trace (name, user,
  // session, tags, metadata, release, environment). Caller must hold mu_.
  void add_trace_level_attributes(JsonValue& span) const;

  Tracer& tracer_;
  std::string id_;
  std::string name_;

  mutable std::mutex mu_;
  std::chrono::system_clock::time_point trace_start_;
  // Span id of the root span that represents the trace itself.
  std::string root_span_id_;
  std::optional<JsonValue> input_;
  std::optional<JsonValue> output_;
  std::optional<std::string> user_id_;
  std::optional<std::string> session_id_;
  std::optional<JsonValue> metadata_;
  std::vector<std::string> tags_;

  // Pending OTLP spans accumulated until `end()` exports them.
  std::vector<JsonValue> events_;

  // Active generation, set by instrument() and closed by finish_generation().
  std::optional<PendingGeneration> active_generation_;

  // Map tool_call_id -> open span index in events_, used to set the span's
  // end time / output when the tool finishes.
  std::unordered_map<std::string, size_t> open_tool_spans_;

  std::atomic<bool> ended_{false};
};

/// Convenience wrapper around `Trace::instrument` + `Client::generate_text` +
/// `Trace::finish_generation`. Does not call `Trace::end()` so the caller can
/// still attach output/metadata/tags to the trace.
GenerateResult generate_text(
    Client& client,
    GenerateOptions options,
    Trace& trace,
    const std::string& generation_name = "generate_text");

}  // namespace langfuse
}  // namespace ai
