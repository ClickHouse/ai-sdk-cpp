#include "ai/langfuse.h"
#include "ai/logger.h"

#include <cstdio>
#include <ctime>
#include <httplib.h>
#include <random>
#include <uuid.h>

namespace ai {
namespace langfuse {

namespace {

constexpr const char* kIngestionPath = "/api/public/ingestion";

// Langfuse ingestion event type discriminators
// (packages/shared/src/server/ingestion/types.ts::eventTypes).
constexpr const char* kEventTraceCreate = "trace-create";
constexpr const char* kEventSpanCreate = "span-create";
constexpr const char* kEventSpanUpdate = "span-update";
constexpr const char* kEventGenerationCreate = "generation-create";

constexpr const char* kLevelDefault = "DEFAULT";
constexpr const char* kLevelError = "ERROR";
constexpr const char* kUsageUnitTokens = "TOKENS";

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
          {"total", u.total_tokens},
          {"unit", kUsageUnitTokens}};
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
  auto trace = std::make_shared<Trace>(*this, Trace::new_uuid(), name);
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

bool Tracer::send_batch(const JsonValue& events) {
  if (!is_valid()) {
    ai::logger::log_warn(
        "Langfuse tracer not configured (missing host/public_key/secret_key); "
        "dropping {} events",
        events.is_array() ? events.size() : 0);
    return false;
  }

  JsonValue body;
  body["batch"] = events;
  std::string serialized = body.dump();

  std::lock_guard<std::mutex> lock(mu_);
  auto& s = http_state();
  std::string path = s.base_path + kIngestionPath;
  auto res =
      s.client.Post(path.c_str(), s.headers, serialized, "application/json");

  if (!res) {
    ai::logger::log_error("Langfuse ingestion failed: {}",
                          httplib::to_string(res.error()));
    return false;
  }
  if (res->status >= 200 && res->status < 300) {
    ai::logger::log_debug("Langfuse ingestion accepted ({}): {}", res->status,
                          res->body);
    return true;
  }
  ai::logger::log_error("Langfuse ingestion non-2xx ({}): {}", res->status,
                        res->body);
  return false;
}

// ---------------------------------------------------------------------------
// Trace
// ---------------------------------------------------------------------------

Trace::Trace(Tracer& tracer, std::string id, std::string name)
    : tracer_(tracer),
      id_(std::move(id)),
      name_(std::move(name)),
      trace_start_(std::chrono::system_clock::now()) {}

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
  gen.id = new_uuid();
  gen.name = generation_name;
  gen.start_time = std::chrono::system_clock::now();
  gen.input = messages_input_from(options);
  gen.model = options.model;
  gen.model_parameters = model_parameters_from(options);

  {
    std::lock_guard<std::mutex> lock(mu_);
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

namespace {

// Build a span-create event body. Used for both tool-call starts (open span)
// and the synthetic span emitted in record_tool_call_finish's fallback path.
JsonValue make_span_create(const std::string& span_id,
                           const std::string& trace_id,
                           const std::string& parent_id,
                           const std::string& name,
                           const std::string& start_iso,
                           const JsonValue& input,
                           const std::string& environment) {
  JsonValue body;
  body["id"] = span_id;
  body["traceId"] = trace_id;
  body["name"] = name;
  body["startTime"] = start_iso;
  body["input"] = input;
  body["environment"] = environment;
  if (!parent_id.empty())
    body["parentObservationId"] = parent_id;
  return body;
}

JsonValue wrap_event(const char* type,
                     const std::string& timestamp,
                     JsonValue body) {
  JsonValue event;
  event["id"] = Trace::new_uuid();
  event["timestamp"] = timestamp;
  event["type"] = type;
  event["body"] = std::move(body);
  return event;
}

}  // namespace

void Trace::record_tool_call_start(const ToolCall& call) {
  if (ended_.load())
    return;

  // One clock read per event: startTime and the envelope timestamp share it.
  std::string ts = to_iso8601(std::chrono::system_clock::now());

  std::lock_guard<std::mutex> lock(mu_);
  std::string parent_id =
      active_generation_ ? active_generation_->id : std::string();
  JsonValue body =
      make_span_create(new_uuid(), id_, parent_id, call.tool_name, ts,
                       call.arguments, tracer_.config().environment);

  size_t idx = events_.size();
  events_.push_back(wrap_event(kEventSpanCreate, ts, std::move(body)));
  open_tool_spans_[call.id] = idx;
}

void Trace::record_tool_call_finish(const ToolResult& result) {
  if (ended_.load())
    return;

  std::string ts = to_iso8601(std::chrono::system_clock::now());

  std::lock_guard<std::mutex> lock(mu_);
  auto it = open_tool_spans_.find(result.tool_call_id);
  JsonValue output =
      result.is_success() ? result.result : JsonValue(result.error_message());

  if (it == open_tool_spans_.end()) {
    // No matching start (defensive). Emit a closed span with start==end.
    std::string parent_id =
        active_generation_ ? active_generation_->id : std::string();
    JsonValue body =
        make_span_create(new_uuid(), id_, parent_id, result.tool_name, ts,
                         result.arguments, tracer_.config().environment);
    body["endTime"] = ts;
    body["output"] = output;
    body["level"] = result.is_success() ? kLevelDefault : kLevelError;
    if (!result.is_success())
      body["statusMessage"] = result.error_message();
    events_.push_back(wrap_event(kEventSpanCreate, ts, std::move(body)));
    return;
  }

  // Close the open span by emitting span-update referencing the same id.
  const std::string span_id =
      events_[it->second]["body"]["id"].get<std::string>();
  JsonValue body;
  body["id"] = span_id;
  body["traceId"] = id_;
  body["endTime"] = ts;
  body["output"] = output;
  if (!result.is_success()) {
    body["level"] = kLevelError;
    body["statusMessage"] = result.error_message();
  }
  events_.push_back(wrap_event(kEventSpanUpdate, ts, std::move(body)));
  open_tool_spans_.erase(it);
}

void Trace::finish_generation(const GenerateResult& result) {
  if (ended_.load())
    return;

  std::lock_guard<std::mutex> lock(mu_);
  if (!active_generation_ || active_generation_->finalized)
    return;
  auto& gen = *active_generation_;
  gen.finalized = true;

  // Aggregate per-step usage when the multi-step coordinator didn't roll it up.
  Usage total = result.usage;
  if (!result.steps.empty() && total.total_tokens == 0) {
    int p = 0, c = 0;
    for (const auto& s : result.steps) {
      p += s.usage.prompt_tokens;
      c += s.usage.completion_tokens;
    }
    total = Usage(p, c);
  }

  std::string end_iso = to_iso8601(std::chrono::system_clock::now());

  JsonValue body;
  body["id"] = gen.id;
  body["traceId"] = id_;
  body["name"] = gen.name;
  body["startTime"] = to_iso8601(gen.start_time);
  body["endTime"] = end_iso;
  body["model"] = gen.model;
  if (!gen.model_parameters.empty())
    body["modelParameters"] = gen.model_parameters;
  body["input"] = gen.input;
  body["output"] = result.text;
  body["usage"] = usage_to_langfuse(total);
  body["environment"] = tracer_.config().environment;

  JsonValue meta = JsonValue::object();
  meta["finish_reason"] = result.finishReasonToString();
  if (!result.steps.empty())
    meta["steps"] = result.steps.size();
  if (!result.warnings.empty())
    meta["warnings"] = result.warnings;
  body["metadata"] = std::move(meta);

  if (!result.is_success() && result.error) {
    body["level"] = kLevelError;
    body["statusMessage"] = *result.error;
  }

  events_.push_back(
      wrap_event(kEventGenerationCreate, end_iso, std::move(body)));
}

JsonValue Trace::build_trace_event() const {
  // Caller must hold mu_.
  std::string start_iso = to_iso8601(trace_start_);

  JsonValue body;
  body["id"] = id_;
  body["name"] = name_;
  body["timestamp"] = start_iso;
  body["environment"] = tracer_.config().environment;
  if (!tracer_.config().release.empty())
    body["release"] = tracer_.config().release;
  if (input_)
    body["input"] = *input_;
  if (output_)
    body["output"] = *output_;
  if (user_id_)
    body["userId"] = *user_id_;
  if (session_id_)
    body["sessionId"] = *session_id_;
  if (metadata_)
    body["metadata"] = *metadata_;
  if (!tags_.empty())
    body["tags"] = tags_;

  return wrap_event(kEventTraceCreate, start_iso, std::move(body));
}

bool Trace::end() {
  if (ended_.exchange(true))
    return true;

  JsonValue batch = JsonValue::array();
  {
    std::lock_guard<std::mutex> lock(mu_);
    batch.push_back(build_trace_event());
    for (auto& ev : events_)
      batch.push_back(std::move(ev));
    events_.clear();
  }

  bool ok = tracer_.send_batch(batch);
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
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, static_cast<long long>(ms));
  return std::string(buf);
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
