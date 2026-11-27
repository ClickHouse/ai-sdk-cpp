#include "ai/langfuse.h"
#include "ai/logger.h"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <httplib.h>
#include <random>
#include <uuid.h>

namespace ai {
namespace langfuse {

namespace {

// OTLP/HTTP traces endpoint of Langfuse: the only ingestion API of Langfuse v4
// (Langfuse Cloud) for traces and observations; also available in self-hosted
// Langfuse since v3.22.0.
constexpr const char* kOtlpTracesPath = "/api/public/otel/v1/traces";

// Tells Langfuse v4 that every span carries the trace-level attributes
// (propagated client side), so spans are visible without the delay of the
// v3-to-v4 transition pipeline.
constexpr const char* kIngestionVersionHeader = "x-langfuse-ingestion-version";
constexpr const char* kIngestionVersion = "4";

constexpr const char* kInstrumentationScope = "ai-sdk-cpp";

// Langfuse OpenTelemetry attribute names
// (packages/shared/src/server/otel/attributes.ts).
constexpr const char* kAttrTraceName = "langfuse.trace.name";
constexpr const char* kAttrTraceInput = "langfuse.trace.input";
constexpr const char* kAttrTraceOutput = "langfuse.trace.output";
constexpr const char* kAttrTraceTags = "langfuse.trace.tags";
constexpr const char* kAttrTraceMetadata = "langfuse.trace.metadata";
constexpr const char* kAttrUserId = "langfuse.user.id";
constexpr const char* kAttrSessionId = "langfuse.session.id";
constexpr const char* kAttrRelease = "langfuse.release";
constexpr const char* kAttrEnvironment = "langfuse.environment";
constexpr const char* kAttrObservationType = "langfuse.observation.type";
constexpr const char* kAttrObservationInput = "langfuse.observation.input";
constexpr const char* kAttrObservationOutput = "langfuse.observation.output";
constexpr const char* kAttrObservationMetadata =
    "langfuse.observation.metadata";
constexpr const char* kAttrObservationLevel = "langfuse.observation.level";
constexpr const char* kAttrObservationStatusMessage =
    "langfuse.observation.status_message";
constexpr const char* kAttrModelName = "langfuse.observation.model.name";
constexpr const char* kAttrModelParameters =
    "langfuse.observation.model.parameters";
constexpr const char* kAttrUsageDetails = "langfuse.observation.usage_details";
// OpenTelemetry GenAI semantic conventions, for other OTLP consumers.
constexpr const char* kAttrGenAiRequestModel = "gen_ai.request.model";
constexpr const char* kAttrGenAiInputTokens = "gen_ai.usage.input_tokens";
constexpr const char* kAttrGenAiOutputTokens = "gen_ai.usage.output_tokens";

constexpr const char* kObservationTypeSpan = "span";
constexpr const char* kObservationTypeGeneration = "generation";
constexpr const char* kLevelError = "ERROR";

// opentelemetry.proto.trace.v1.Span.SpanKind / Status.StatusCode.
constexpr int kSpanKindInternal = 1;
constexpr int kSpanKindClient = 3;
constexpr int kStatusCodeError = 2;

// Strip the scheme prefix off a Langfuse host URL and return the leading
// sub-path (everything after the host[:port], e.g. "/langfuse" if hosted under
// a sub-path). httplib::Client's URL constructor handles host/port/TLS itself,
// so we only need to extract the optional base path.
std::string extract_base_path(const std::string& url) {
  std::string s = url;
  if (s.starts_with("https://"))
    s = s.substr(8);
  else if (s.starts_with("http://"))
    s = s.substr(7);
  auto slash = s.find('/');
  if (slash == std::string::npos)
    return {};
  std::string p = s.substr(slash);
  if (!p.empty() && p.back() == '/')
    p.pop_back();
  return p;
}

// Strip any trailing slash from a scheme+host[:port] URL so httplib's URL ctor
// gets exactly what it expects.
std::string strip_path_suffix(const std::string& url) {
  std::string s = url;
  std::string scheme;
  if (s.starts_with("https://")) {
    scheme = "https://";
    s = s.substr(8);
  } else if (s.starts_with("http://")) {
    scheme = "http://";
    s = s.substr(7);
  }
  auto slash = s.find('/');
  if (slash != std::string::npos)
    s = s.substr(0, slash);
  return scheme + s;
}

std::string base64_encode(const std::string& in) {
  static const char* kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((in.size() + 2) / 3) * 4);
  int val = 0;
  int valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(kAlphabet[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    out.push_back(kAlphabet[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (out.size() % 4)
    out.push_back('=');
  return out;
}

JsonValue model_parameters_from(const GenerateOptions& options) {
  JsonValue params = JsonValue::object();
  if (options.temperature)
    params["temperature"] = *options.temperature;
  if (options.max_tokens)
    params["max_tokens"] = *options.max_tokens;
  if (options.top_p)
    params["top_p"] = *options.top_p;
  if (options.seed)
    params["seed"] = *options.seed;
  if (options.frequency_penalty)
    params["frequency_penalty"] = *options.frequency_penalty;
  if (options.presence_penalty)
    params["presence_penalty"] = *options.presence_penalty;
  if (options.max_steps > 1)
    params["max_steps"] = options.max_steps;
  return params;
}

JsonValue messages_input_from(const GenerateOptions& options) {
  JsonValue arr = JsonValue::array();
  if (!options.system.empty()) {
    arr.push_back({{"role", "system"}, {"content", options.system}});
  }
  if (!options.messages.empty()) {
    for (const auto& m : options.messages) {
      JsonValue msg;
      msg["role"] = m.roleToString();
      msg["content"] = m.get_text();
      arr.push_back(std::move(msg));
    }
  } else if (!options.prompt.empty()) {
    arr.push_back({{"role", "user"}, {"content", options.prompt}});
  }
  return arr;
}

JsonValue usage_to_langfuse(const Usage& u) {
  return {{"input", u.prompt_tokens},
          {"output", u.completion_tokens},
          {"total", u.total_tokens}};
}

// Langfuse expects input/output/metadata attributes as strings holding JSON;
// plain strings are sent as is.
std::string to_attribute_string(const JsonValue& value) {
  if (value.is_string())
    return value.get<std::string>();
  return value.dump();
}

void add_attribute(JsonValue& span, const char* key, std::string value) {
  span["attributes"].push_back(
      {{"key", key}, {"value", {{"stringValue", std::move(value)}}}});
}

void add_attribute(JsonValue& span, const char* key, const JsonValue& value) {
  add_attribute(span, key, to_attribute_string(value));
}

void add_attribute(JsonValue& span, const char* key, int64_t value) {
  // OTLP/JSON encodes 64-bit integers as decimal strings.
  span["attributes"].push_back(
      {{"key", key}, {"value", {{"intValue", std::to_string(value)}}}});
}

void add_attribute(JsonValue& span,
                   const char* key,
                   const std::vector<std::string>& values) {
  JsonValue array = JsonValue::array();
  for (const auto& v : values)
    array.push_back({{"stringValue", v}});
  span["attributes"].push_back(
      {{"key", key}, {"value", {{"arrayValue", {{"values", array}}}}}});
}

JsonValue make_span(const std::string& trace_id,
                    const std::string& span_id,
                    const std::string& parent_span_id,
                    const std::string& name,
                    int kind,
                    const char* observation_type,
                    const std::string& start_nanos) {
  JsonValue span;
  span["traceId"] = trace_id;
  span["spanId"] = span_id;
  if (!parent_span_id.empty())
    span["parentSpanId"] = parent_span_id;
  span["name"] = name;
  span["kind"] = kind;
  span["startTimeUnixNano"] = start_nanos;
  span["attributes"] = JsonValue::array();
  add_attribute(span, kAttrObservationType, std::string(observation_type));
  return span;
}

void mark_error(JsonValue& span, const std::string& message) {
  add_attribute(span, kAttrObservationLevel, std::string(kLevelError));
  add_attribute(span, kAttrObservationStatusMessage, message);
  span["status"] = {{"code", kStatusCodeError}, {"message", message}};
}

std::string random_hex_id(size_t bytes) {
  static thread_local std::mt19937_64 engine{[] {
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) ^ rd();
  }()};
  static const char* kHex = "0123456789abcdef";
  std::string out(bytes * 2, '0');
  bool all_zero = true;
  do {
    for (size_t i = 0; i < bytes; i += 8) {
      uint64_t r = engine();
      for (size_t j = 0; j < 8 && i + j < bytes; ++j) {
        unsigned char b = static_cast<unsigned char>(r >> (j * 8));
        out[(i + j) * 2] = kHex[b >> 4];
        out[(i + j) * 2 + 1] = kHex[b & 0xF];
        if (b != 0)
          all_zero = false;
      }
    }
    // An all-zero id is invalid in OpenTelemetry.
  } while (all_zero);
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tracer
// ---------------------------------------------------------------------------

struct Tracer::HttpState {
  httplib::Client client;
  std::string base_path;  // e.g. "/langfuse" or "" for the standard host
  httplib::Headers headers;

  HttpState(const std::string& host_url,
            const std::string& public_key,
            const std::string& secret_key,
            int connection_timeout_sec,
            int read_timeout_sec)
      : client(strip_path_suffix(host_url)),
        base_path(extract_base_path(host_url)) {
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(connection_timeout_sec, 0);
    client.set_read_timeout(read_timeout_sec, 0);
    headers = {
        {"Authorization",
         "Basic " + base64_encode(public_key + ":" + secret_key)},
        {"User-Agent", "ai-sdk-cpp-langfuse/0.1"},
        {"X-Langfuse-Sdk-Name", "ai-sdk-cpp"},
        {"X-Langfuse-Sdk-Variant", "ai-sdk-cpp"},
        {kIngestionVersionHeader, kIngestionVersion},
    };
  }
};

Tracer::Tracer(Config config) : config_(std::move(config)) {}

Tracer::~Tracer() = default;

bool Tracer::is_valid() const {
  return !config_.host.empty() && !config_.public_key.empty() &&
         !config_.secret_key.empty();
}

Tracer::HttpState& Tracer::http_state() {
  // Caller must hold mu_.
  if (!http_) {
    http_ = std::make_unique<HttpState>(
        config_.host, config_.public_key, config_.secret_key,
        config_.connection_timeout_sec, config_.read_timeout_sec);
  }
  return *http_;
}

std::shared_ptr<Trace> Tracer::start_trace(const std::string& name,
                                           TraceOptions opts) {
  auto trace = std::make_shared<Trace>(*this, Trace::new_trace_id(), name);
  if (opts.input)
    trace->set_input(std::move(*opts.input));
  if (opts.output)
    trace->set_output(std::move(*opts.output));
  if (opts.user_id)
    trace->set_user_id(std::move(*opts.user_id));
  if (opts.session_id)
    trace->set_session_id(std::move(*opts.session_id));
  if (opts.metadata)
    trace->set_metadata(std::move(*opts.metadata));
  for (auto& t : opts.tags)
    trace->add_tag(std::move(t));
  return trace;
}

bool Tracer::export_spans(const JsonValue& spans) {
  if (!is_valid()) {
    ai::logger::log_warn(
        "Langfuse tracer not configured (missing host/public_key/secret_key); "
        "dropping {} spans",
        spans.is_array() ? spans.size() : 0);
    return false;
  }

  // ExportTraceServiceRequest
  // (opentelemetry/proto/collector/trace/v1/trace_service.proto).
  JsonValue resource_attributes = JsonValue::array();
  resource_attributes.push_back(
      {{"key", "service.name"},
       {"value", {{"stringValue", kInstrumentationScope}}}});
  JsonValue body;
  body["resourceSpans"] = JsonValue::array();
  body["resourceSpans"].push_back(
      {{"resource", {{"attributes", std::move(resource_attributes)}}},
       {"scopeSpans",
        JsonValue::array({{{"scope", {{"name", kInstrumentationScope}}},
                           {"spans", spans}}})}});
  std::string serialized = body.dump();

  std::lock_guard<std::mutex> lock(mu_);
  auto& s = http_state();
  std::string path = s.base_path + kOtlpTracesPath;
  auto res =
      s.client.Post(path.c_str(), s.headers, serialized, "application/json");

  if (!res) {
    ai::logger::log_error("Langfuse OTLP export failed: {}",
                          httplib::to_string(res.error()));
    return false;
  }
  if (res->status < 200 || res->status >= 300) {
    ai::logger::log_error("Langfuse OTLP export non-2xx ({}): {}", res->status,
                          res->body);
    return false;
  }

  // ExportTraceServiceResponse may report spans the server rejected.
  auto response = JsonValue::parse(res->body, nullptr, false);
  if (response.is_object() && response.contains("partialSuccess")) {
    const auto& partial = response["partialSuccess"];
    // int64 rejectedSpans may be encoded as a number or a decimal string.
    const auto rejected = partial.value("rejectedSpans", JsonValue(0));
    if ((rejected.is_number() && rejected.get<int64_t>() != 0) ||
        (rejected.is_string() && rejected.get<std::string>() != "0")) {
      ai::logger::log_error("Langfuse OTLP export rejected spans ({}): {}",
                            res->status, res->body);
      return false;
    }
  }
  ai::logger::log_debug("Langfuse OTLP export accepted ({}): {}", res->status,
                        res->body);
  return true;
}

// ---------------------------------------------------------------------------
// Trace
// ---------------------------------------------------------------------------

Trace::Trace(Tracer& tracer, std::string id, std::string name)
    : tracer_(tracer),
      id_(std::move(id)),
      name_(std::move(name)),
      trace_start_(std::chrono::system_clock::now()),
      root_span_id_(new_span_id()) {}

void Trace::set_input(JsonValue input) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  input_ = std::move(input);
}

void Trace::set_output(JsonValue output) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  output_ = std::move(output);
}

void Trace::set_user_id(std::string user_id) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  user_id_ = std::move(user_id);
}

void Trace::set_session_id(std::string session_id) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  session_id_ = std::move(session_id);
}

void Trace::set_metadata(JsonValue metadata) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  metadata_ = std::move(metadata);
}

void Trace::add_tag(std::string tag) {
  if (ended_.load())
    return;
  std::lock_guard<std::mutex> lock(mu_);
  tags_.push_back(std::move(tag));
}

void Trace::instrument(GenerateOptions& options,
                       const std::string& generation_name) {
  if (ended_.load())
    return;

  PendingGeneration gen;
  gen.id = new_span_id();
  gen.name = generation_name;
  gen.start_time = std::chrono::system_clock::now();
  gen.input = messages_input_from(options);
  gen.model = options.model;
  gen.model_parameters = model_parameters_from(options);

  {
    std::lock_guard<std::mutex> lock(mu_);
    // A previous generation that was never finished is closed now, so that
    // its tool spans keep their parent.
    if (active_generation_ && !active_generation_->finalized)
      events_.push_back(build_generation_span(
          to_unix_nanos(std::chrono::system_clock::now()), nullptr));
    active_generation_ = std::move(gen);
  }

  // Chain tool callbacks so we can record per-tool spans, preserving any
  // user-installed callbacks.
  std::weak_ptr<Trace> self = shared_from_this();

  auto user_tool_start = options.on_tool_call_start;
  options.on_tool_call_start = [self, user_tool_start](const ToolCall& call) {
    if (auto sp = self.lock())
      sp->record_tool_call_start(call);
    if (user_tool_start)
      (*user_tool_start)(call);
  };

  auto user_tool_finish = options.on_tool_call_finish;
  options.on_tool_call_finish = [self,
                                 user_tool_finish](const ToolResult& result) {
    if (auto sp = self.lock())
      sp->record_tool_call_finish(result);
    if (user_tool_finish)
      (*user_tool_finish)(result);
  };
}

void Trace::record_tool_call_start(const ToolCall& call) {
  if (ended_.load())
    return;

  std::string now = to_unix_nanos(std::chrono::system_clock::now());

  std::lock_guard<std::mutex> lock(mu_);
  std::string parent_id =
      active_generation_ ? active_generation_->id : root_span_id_;
  JsonValue span = make_span(id_, new_span_id(), parent_id, call.tool_name,
                             kSpanKindInternal, kObservationTypeSpan, now);
  add_attribute(span, kAttrObservationInput, call.arguments);

  size_t idx = events_.size();
  events_.push_back(std::move(span));
  open_tool_spans_[call.id] = idx;
}

void Trace::record_tool_call_finish(const ToolResult& result) {
  if (ended_.load())
    return;

  std::string now = to_unix_nanos(std::chrono::system_clock::now());

  std::lock_guard<std::mutex> lock(mu_);
  auto it = open_tool_spans_.find(result.tool_call_id);
  JsonValue* span = nullptr;
  if (it == open_tool_spans_.end()) {
    // No matching start (defensive). Emit a closed span with start==end.
    std::string parent_id =
        active_generation_ ? active_generation_->id : root_span_id_;
    JsonValue created =
        make_span(id_, new_span_id(), parent_id, result.tool_name,
                  kSpanKindInternal, kObservationTypeSpan, now);
    add_attribute(created, kAttrObservationInput, result.arguments);
    events_.push_back(std::move(created));
    span = &events_.back();
  } else {
    span = &events_[it->second];
    open_tool_spans_.erase(it);
  }

  (*span)["endTimeUnixNano"] = now;
  if (result.is_success()) {
    add_attribute(*span, kAttrObservationOutput, result.result);
  } else {
    add_attribute(*span, kAttrObservationOutput, result.error_message());
    mark_error(*span, result.error_message());
  }
}

void Trace::finish_generation(const GenerateResult& result) {
  if (ended_.load())
    return;

  std::lock_guard<std::mutex> lock(mu_);
  if (!active_generation_ || active_generation_->finalized)
    return;

  events_.push_back(build_generation_span(
      to_unix_nanos(std::chrono::system_clock::now()), &result));
}

JsonValue Trace::build_generation_span(const std::string& end_nanos,
                                       const GenerateResult* result) {
  // Caller must hold mu_.
  auto& gen = *active_generation_;
  gen.finalized = true;

  JsonValue span =
      make_span(id_, gen.id, root_span_id_, gen.name, kSpanKindClient,
                kObservationTypeGeneration, to_unix_nanos(gen.start_time));
  span["endTimeUnixNano"] = end_nanos;
  add_attribute(span, kAttrModelName, gen.model);
  add_attribute(span, kAttrGenAiRequestModel, gen.model);
  if (!gen.model_parameters.empty())
    add_attribute(span, kAttrModelParameters, gen.model_parameters);
  add_attribute(span, kAttrObservationInput, gen.input);

  if (!result)
    return span;

  // Aggregate per-step usage when the multi-step coordinator didn't roll it up.
  Usage total = result->usage;
  if (!result->steps.empty() && total.total_tokens == 0) {
    int p = 0, c = 0;
    for (const auto& s : result->steps) {
      p += s.usage.prompt_tokens;
      c += s.usage.completion_tokens;
    }
    total = Usage(p, c);
  }

  add_attribute(span, kAttrObservationOutput, result->text);
  add_attribute(span, kAttrUsageDetails, usage_to_langfuse(total));
  add_attribute(span, kAttrGenAiInputTokens,
                static_cast<int64_t>(total.prompt_tokens));
  add_attribute(span, kAttrGenAiOutputTokens,
                static_cast<int64_t>(total.completion_tokens));

  JsonValue meta = JsonValue::object();
  meta["finish_reason"] = result->finishReasonToString();
  if (!result->steps.empty())
    meta["steps"] = result->steps.size();
  if (!result->warnings.empty())
    meta["warnings"] = result->warnings;
  add_attribute(span, kAttrObservationMetadata, meta);

  if (!result->is_success() && result->error)
    mark_error(span, *result->error);

  return span;
}

JsonValue Trace::build_root_span(const std::string& end_nanos) const {
  // Caller must hold mu_.
  JsonValue span =
      make_span(id_, root_span_id_, std::string(), name_, kSpanKindInternal,
                kObservationTypeSpan, to_unix_nanos(trace_start_));
  span["endTimeUnixNano"] = end_nanos;
  // Langfuse v4 takes the trace input/output from the root observation;
  // the langfuse.trace.* attributes set them for Langfuse v3.
  if (input_) {
    add_attribute(span, kAttrObservationInput, *input_);
    add_attribute(span, kAttrTraceInput, *input_);
  }
  if (output_) {
    add_attribute(span, kAttrObservationOutput, *output_);
    add_attribute(span, kAttrTraceOutput, *output_);
  }
  return span;
}

void Trace::add_trace_level_attributes(JsonValue& span) const {
  // Caller must hold mu_.
  const auto& config = tracer_.config();
  add_attribute(span, kAttrTraceName, name_);
  add_attribute(span, kAttrEnvironment, config.environment);
  if (!config.release.empty())
    add_attribute(span, kAttrRelease, config.release);
  if (user_id_)
    add_attribute(span, kAttrUserId, *user_id_);
  if (session_id_)
    add_attribute(span, kAttrSessionId, *session_id_);
  if (metadata_)
    add_attribute(span, kAttrTraceMetadata, *metadata_);
  if (!tags_.empty())
    add_attribute(span, kAttrTraceTags, tags_);
}

bool Trace::end() {
  if (ended_.exchange(true))
    return true;

  JsonValue spans = JsonValue::array();
  {
    std::lock_guard<std::mutex> lock(mu_);
    std::string now = to_unix_nanos(std::chrono::system_clock::now());

    if (active_generation_ && !active_generation_->finalized)
      events_.push_back(build_generation_span(now, nullptr));

    spans.push_back(build_root_span(now));
    for (auto& span : events_) {
      // Tool calls that never finished are closed at the end of the trace.
      if (!span.contains("endTimeUnixNano"))
        span["endTimeUnixNano"] = now;
      spans.push_back(std::move(span));
    }
    events_.clear();
    open_tool_spans_.clear();

    // Langfuse v4 requires trace-level attributes on every span.
    for (auto& span : spans)
      add_trace_level_attributes(span);
  }

  bool ok = tracer_.export_spans(spans);
  return ok ||
         tracer_.config().error_policy == Config::ErrorPolicy::kBestEffort;
}

std::string Trace::to_iso8601(std::chrono::system_clock::time_point t) {
  auto tt = std::chrono::system_clock::to_time_t(t);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                t.time_since_epoch())
                .count() %
            1000;
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  char buf[80];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, static_cast<long long>(ms));
  return std::string(buf);
}

std::string Trace::to_unix_nanos(std::chrono::system_clock::time_point t) {
  return std::to_string(
      std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch())
          .count());
}

std::string Trace::new_trace_id() {
  return random_hex_id(16);
}

std::string Trace::new_span_id() {
  return random_hex_id(8);
}

std::string Trace::new_uuid() {
  // RFC 4122 v4 UUID via stduuid. The generator is thread-local to avoid
  // contention; each thread seeds its own mt19937 from std::random_device.
  static thread_local std::mt19937 engine{std::random_device{}()};
  static thread_local uuids::uuid_random_generator gen{engine};
  return uuids::to_string(gen());
}

// ---------------------------------------------------------------------------
// Free function helper
// ---------------------------------------------------------------------------

GenerateResult generate_text(Client& client,
                             GenerateOptions options,
                             Trace& trace,
                             const std::string& generation_name) {
  trace.instrument(options, generation_name);
  GenerateResult result = client.generate_text(options);
  trace.finish_generation(result);
  return result;
}

}  // namespace langfuse
}  // namespace ai
