#include "ai/langfuse.h"
#include "ai/logger.h"

#include <cstdio>
#include <ctime>
#include <httplib.h>
#include <random>

namespace ai {
namespace langfuse {

namespace {

constexpr const char* kIngestionPath = "/api/public/ingestion";

struct ParsedHost {
  std::string host;
  int port = -1;  // -1 means "default for scheme"
  bool use_ssl = true;
  std::string base_path;  // e.g. "/langfuse" if hosted under a sub-path
};

ParsedHost parse_host(const std::string& url) {
  ParsedHost out;
  std::string s = url;
  if (s.starts_with("https://")) {
    s = s.substr(8);
    out.use_ssl = true;
  } else if (s.starts_with("http://")) {
    s = s.substr(7);
    out.use_ssl = false;
  }

  auto slash = s.find('/');
  std::string host_port = (slash == std::string::npos) ? s : s.substr(0, slash);
  out.base_path = (slash == std::string::npos) ? "" : s.substr(slash);
  if (!out.base_path.empty() && out.base_path.back() == '/')
    out.base_path.pop_back();

  auto colon = host_port.find(':');
  if (colon != std::string::npos) {
    out.host = host_port.substr(0, colon);
    try {
      out.port = std::stoi(host_port.substr(colon + 1));
    } catch (...) {
      out.port = -1;
    }
  } else {
    out.host = host_port;
  }

  return out;
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
  while (out.size() % 4) {
    out.push_back('=');
  }
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
  // Prefer explicit messages; otherwise synthesise from system+prompt.
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
  // Langfuse accepts both legacy `usage` and `usageDetails`. Send legacy form
  // for broad compatibility.
  return {
      {"input", u.prompt_tokens},
      {"output", u.completion_tokens},
      {"total", u.total_tokens},
      {"unit", "TOKENS"},
  };
}

}  // namespace

// ---------------------------------------------------------------------------
// Tracer
// ---------------------------------------------------------------------------

Tracer::Tracer(Config config) : config_(std::move(config)) {}

bool Tracer::is_valid() const {
  return !config_.host.empty() && !config_.public_key.empty() &&
         !config_.secret_key.empty();
}

std::shared_ptr<Trace> Tracer::start_trace(
    const std::string& name,
    std::optional<JsonValue> input,
    std::optional<std::string> user_id,
    std::optional<std::string> session_id,
    std::optional<JsonValue> metadata,
    std::vector<std::string> tags) {
  auto trace = std::make_shared<Trace>(*this, Trace::new_uuid(), name);
  if (input)
    trace->set_input(std::move(*input));
  if (user_id)
    trace->set_user_id(std::move(*user_id));
  if (session_id)
    trace->set_session_id(std::move(*session_id));
  if (metadata)
    trace->set_metadata(std::move(*metadata));
  for (auto& t : tags)
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

  ParsedHost p = parse_host(config_.host);
  // httplib::Client(scheme_host_port) handles http/https + port automatically.
  std::string scheme_host_port =
      std::string(p.use_ssl ? "https://" : "http://") + p.host;
  if (p.port > 0)
    scheme_host_port += ":" + std::to_string(p.port);
  httplib::Client client(scheme_host_port);
  client.enable_server_certificate_verification(true);
  client.set_connection_timeout(config_.connection_timeout_sec, 0);
  client.set_read_timeout(config_.read_timeout_sec, 0);

  std::string auth =
      "Basic " + base64_encode(config_.public_key + ":" + config_.secret_key);

  JsonValue body;
  body["batch"] = events;
  std::string serialized = body.dump();

  std::string path = p.base_path + kIngestionPath;
  httplib::Headers headers = {
      {"Authorization", auth},
      {"User-Agent", "ai-sdk-cpp-langfuse/0.1"},
      {"X-Langfuse-Sdk-Name", "ai-sdk-cpp"},
      {"X-Langfuse-Sdk-Variant", "ai-sdk-cpp"},
  };

  auto res = client.Post(path.c_str(), headers, serialized, "application/json");
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
  std::lock_guard<std::mutex> lock(mu_);
  input_ = std::move(input);
}

void Trace::set_output(JsonValue output) {
  std::lock_guard<std::mutex> lock(mu_);
  output_ = std::move(output);
}

void Trace::set_user_id(std::string user_id) {
  std::lock_guard<std::mutex> lock(mu_);
  user_id_ = std::move(user_id);
}

void Trace::set_session_id(std::string session_id) {
  std::lock_guard<std::mutex> lock(mu_);
  session_id_ = std::move(session_id);
}

void Trace::set_metadata(JsonValue metadata) {
  std::lock_guard<std::mutex> lock(mu_);
  metadata_ = std::move(metadata);
}

void Trace::add_tag(std::string tag) {
  std::lock_guard<std::mutex> lock(mu_);
  tags_.push_back(std::move(tag));
}

void Trace::instrument(GenerateOptions& options,
                       const std::string& generation_name) {
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
  // user-installed callbacks. Capture a weak_ptr so we do not extend the
  // Trace's lifetime beyond the caller's intent.
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
  JsonValue body;
  body["id"] = new_uuid();
  body["traceId"] = id_;
  body["name"] = call.tool_name;
  body["startTime"] = now_iso8601();
  body["input"] = call.arguments;
  body["environment"] = tracer_.config().environment;

  JsonValue event;
  event["id"] = new_uuid();
  event["timestamp"] = now_iso8601();
  event["type"] = "span-create";
  event["body"] = body;

  std::lock_guard<std::mutex> lock(mu_);
  if (active_generation_) {
    event["body"]["parentObservationId"] = active_generation_->id;
  }
  size_t idx = events_.size();
  events_.push_back(std::move(event));
  open_tool_spans_[call.id] = idx;
}

void Trace::record_tool_call_finish(const ToolResult& result) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = open_tool_spans_.find(result.tool_call_id);
  if (it == open_tool_spans_.end()) {
    // No matching start (shouldn't happen, but be defensive).
    JsonValue body;
    body["id"] = new_uuid();
    body["traceId"] = id_;
    body["name"] = result.tool_name;
    body["startTime"] = now_iso8601();
    body["endTime"] = now_iso8601();
    body["input"] = result.arguments;
    body["output"] =
        result.is_success() ? result.result : JsonValue(result.error_message());
    body["level"] = result.is_success() ? "DEFAULT" : "ERROR";
    if (active_generation_)
      body["parentObservationId"] = active_generation_->id;
    body["environment"] = tracer_.config().environment;
    JsonValue event;
    event["id"] = new_uuid();
    event["timestamp"] = now_iso8601();
    event["type"] = "span-create";
    event["body"] = body;
    events_.push_back(std::move(event));
    return;
  }

  // Close the open span by emitting a span-update event referencing the same
  // id.
  JsonValue& open = events_[it->second];
  std::string span_id = open["body"]["id"].get<std::string>();

  JsonValue body;
  body["id"] = span_id;
  body["traceId"] = id_;
  body["endTime"] = now_iso8601();
  body["output"] =
      result.is_success() ? result.result : JsonValue(result.error_message());
  if (!result.is_success()) {
    body["level"] = "ERROR";
    body["statusMessage"] = result.error_message();
  }

  JsonValue update;
  update["id"] = new_uuid();
  update["timestamp"] = now_iso8601();
  update["type"] = "span-update";
  update["body"] = body;
  events_.push_back(std::move(update));
  open_tool_spans_.erase(it);
}

void Trace::finish_generation(const GenerateResult& result) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!active_generation_ || active_generation_->finalized)
    return;
  auto& gen = *active_generation_;
  gen.finalized = true;

  // Aggregate usage across multi-step results when present.
  Usage total = result.usage;
  if (!result.steps.empty() && total.total_tokens == 0) {
    int p = 0, c = 0;
    for (const auto& s : result.steps) {
      p += s.usage.prompt_tokens;
      c += s.usage.completion_tokens;
    }
    total = Usage(p, c);
  }

  auto fmt = [](std::chrono::system_clock::time_point t) {
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
  };

  JsonValue body;
  body["id"] = gen.id;
  body["traceId"] = id_;
  body["name"] = gen.name;
  body["startTime"] = fmt(gen.start_time);
  body["endTime"] = fmt(std::chrono::system_clock::now());
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
    body["level"] = "ERROR";
    body["statusMessage"] = *result.error;
  }

  JsonValue event;
  event["id"] = new_uuid();
  event["timestamp"] = now_iso8601();
  event["type"] = "generation-create";
  event["body"] = std::move(body);
  events_.push_back(std::move(event));
}

JsonValue Trace::build_trace_event() const {
  // Caller must hold mu_.
  JsonValue body;
  body["id"] = id_;
  body["name"] = name_;
  body["timestamp"] = [this] {
    auto tt = std::chrono::system_clock::to_time_t(trace_start_);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  trace_start_.time_since_epoch())
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
  }();
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

  JsonValue event;
  event["id"] = new_uuid();
  event["timestamp"] = now_iso8601();
  event["type"] = "trace-create";
  event["body"] = std::move(body);
  return event;
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
  return ok || tracer_.config().best_effort;
}

std::string Trace::now_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
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
  // RFC 4122 v4-compatible UUID.
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(rng);
  uint64_t b = dist(rng);

  // Version 4
  a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
  // Variant 10xx
  b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

  char buf[37];
  std::snprintf(
      buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
      static_cast<unsigned>(a >> 32), static_cast<unsigned>((a >> 16) & 0xFFFF),
      static_cast<unsigned>(a & 0xFFFF), static_cast<unsigned>(b >> 48),
      static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
  return std::string(buf);
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
