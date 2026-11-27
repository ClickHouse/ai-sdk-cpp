#include "ai/langfuse.h"

#include <httplib.h>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace ai {
namespace test {

namespace {

struct CapturedRequest {
  std::string path;
  httplib::Headers headers;
  std::string body;
};

// Local HTTP server standing in for Langfuse's OTLP endpoint.
class FakeLangfuseServer {
 public:
  FakeLangfuseServer() {
    server_.Post(".*",
                 [this](const httplib::Request& req, httplib::Response& res) {
                   std::lock_guard<std::mutex> lock(mu_);
                   requests_.push_back({req.path, req.headers, req.body});
                   res.status = status_;
                   res.set_content(response_body_, "application/json");
                 });
    port_ = server_.bind_to_any_port("127.0.0.1");
    thread_ = std::thread([this] { server_.listen_after_bind(); });
    server_.wait_until_ready();
  }

  ~FakeLangfuseServer() {
    server_.stop();
    thread_.join();
  }

  std::string url() const {
    return "http://127.0.0.1:" + std::to_string(port_);
  }

  void respond_with(int status, std::string body) {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = status;
    response_body_ = std::move(body);
  }

  std::vector<CapturedRequest> requests() {
    std::lock_guard<std::mutex> lock(mu_);
    return requests_;
  }

 private:
  httplib::Server server_;
  int port_ = 0;
  std::thread thread_;
  std::mutex mu_;
  int status_ = 200;
  std::string response_body_ = "{}";
  std::vector<CapturedRequest> requests_;
};

std::string header_value(const httplib::Headers& headers,
                         const std::string& name) {
  auto it = headers.find(name);  // httplib compares header names
                                 // case-insensitively
  return it == headers.end() ? std::string() : it->second;
}

// Flattens the OTLP attribute list of a span into key -> value object.
std::map<std::string, JsonValue> attributes_of(const JsonValue& span) {
  std::map<std::string, JsonValue> out;
  for (const auto& attr : span.at("attributes"))
    out[attr.at("key").get<std::string>()] = attr.at("value");
  return out;
}

std::string string_attribute(const JsonValue& span, const std::string& key) {
  auto attrs = attributes_of(span);
  auto it = attrs.find(key);
  if (it == attrs.end())
    return "<missing " + key + ">";
  return it->second.at("stringValue").get<std::string>();
}

bool is_lower_hex(const std::string& s, size_t length) {
  if (s.size() != length)
    return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return true;
}

const JsonValue& find_span(const JsonValue& spans, const std::string& name) {
  for (const auto& span : spans) {
    if (span.at("name") == name)
      return span;
  }
  throw std::runtime_error("span not found: " + name);
}

langfuse::Config config_for(const FakeLangfuseServer& server,
                            const std::string& sub_path = "") {
  langfuse::Config config;
  config.host = server.url() + sub_path;
  config.public_key = "pk-lf-test";
  config.secret_key = "sk-lf-test";
  config.release = "v1.2.3";
  config.environment = "test-env";
  return config;
}

}  // namespace

TEST(LangfuseTracerTest, ExportsOtlpRequestToSubPathHost) {
  FakeLangfuseServer server;
  langfuse::Tracer tracer(config_for(server, "/langfuse/"));

  auto trace = tracer.start_trace("request");
  ASSERT_TRUE(trace->end());

  auto requests = server.requests();
  ASSERT_EQ(requests.size(), 1u);
  const auto& req = requests[0];
  EXPECT_EQ(req.path, "/langfuse/api/public/otel/v1/traces");
  // base64("pk-lf-test:sk-lf-test")
  EXPECT_EQ(header_value(req.headers, "Authorization"),
            "Basic cGstbGYtdGVzdDpzay1sZi10ZXN0");
  EXPECT_EQ(header_value(req.headers, "x-langfuse-ingestion-version"), "4");
  EXPECT_EQ(header_value(req.headers, "Content-Type"), "application/json");

  auto body = JsonValue::parse(req.body);
  ASSERT_EQ(body.at("resourceSpans").size(), 1u);
  const auto& resource_spans = body["resourceSpans"][0];
  ASSERT_EQ(resource_spans.at("scopeSpans").size(), 1u);
  const auto& scope_spans = resource_spans["scopeSpans"][0];
  EXPECT_EQ(scope_spans.at("scope").at("name"), "ai-sdk-cpp");
  ASSERT_EQ(scope_spans.at("spans").size(), 1u);

  const auto& root = scope_spans["spans"][0];
  EXPECT_EQ(root.at("traceId"), trace->id());
  EXPECT_TRUE(is_lower_hex(trace->id(), 32));
  EXPECT_TRUE(is_lower_hex(root.at("spanId").get<std::string>(), 16));
  EXPECT_FALSE(root.contains("parentSpanId"));
  EXPECT_EQ(root.at("name"), "request");
  EXPECT_EQ(string_attribute(root, "langfuse.observation.type"), "span");
  EXPECT_EQ(string_attribute(root, "langfuse.trace.name"), "request");
  EXPECT_EQ(string_attribute(root, "langfuse.environment"), "test-env");
  EXPECT_EQ(string_attribute(root, "langfuse.release"), "v1.2.3");

  // Times are unix nanoseconds encoded as decimal strings.
  auto start = std::stoll(root.at("startTimeUnixNano").get<std::string>());
  auto end = std::stoll(root.at("endTimeUnixNano").get<std::string>());
  EXPECT_GT(start, 1'700'000'000'000'000'000LL);
  EXPECT_GE(end, start);
}

TEST(LangfuseTracerTest, ExportsTraceGenerationAndToolHierarchy) {
  FakeLangfuseServer server;
  langfuse::Tracer tracer(config_for(server));

  langfuse::TraceOptions trace_options;
  trace_options.user_id = "user-1";
  trace_options.session_id = "session-1";
  trace_options.metadata = JsonValue{{"tenant", "acme"}, {"n", 3}};
  trace_options.tags = {"a", "b"};
  auto trace = tracer.start_trace("sql-generation", trace_options);
  trace->set_input("show tables");

  GenerateOptions options;
  options.model = "test-model";
  options.system = "be brief";
  options.prompt = "show tables";
  options.temperature = 0.5;
  options.max_steps = 3;

  bool user_callback_called = false;
  options.on_tool_call_start = [&](const ToolCall&) {
    user_callback_called = true;
  };
  trace->instrument(options, "llm");

  (*options.on_tool_call_start)(
      ToolCall("call-1", "run_query", JsonValue{{"sql", "SHOW TABLES"}}));
  (*options.on_tool_call_finish)(ToolResult("call-1", "run_query",
                                            JsonValue{{"sql", "SHOW TABLES"}},
                                            JsonValue{{"rows", 2}}));
  (*options.on_tool_call_start)(
      ToolCall("call-2", "broken", JsonValue::object()));
  (*options.on_tool_call_finish)(ToolResult(
      "call-2", "broken", JsonValue::object(), std::string("tool failed")));
  EXPECT_TRUE(user_callback_called);

  GenerateResult result("t1, t2", kFinishReasonStop, Usage(10, 5));
  trace->finish_generation(result);
  trace->set_output("t1, t2");
  ASSERT_TRUE(trace->end());

  auto requests = server.requests();
  ASSERT_EQ(requests.size(), 1u);
  auto body = JsonValue::parse(requests[0].body);
  const auto& spans = body["resourceSpans"][0]["scopeSpans"][0]["spans"];
  ASSERT_EQ(spans.size(), 4u);

  const JsonValue root = find_span(spans, "sql-generation");
  const JsonValue generation = find_span(spans, "llm");
  const JsonValue tool = find_span(spans, "run_query");
  const JsonValue broken = find_span(spans, "broken");

  // Hierarchy: root span -> generation -> tool spans, all in one trace.
  for (const auto& span : spans) {
    EXPECT_EQ(span.at("traceId"), trace->id());
    EXPECT_TRUE(is_lower_hex(span.at("spanId").get<std::string>(), 16));
    EXPECT_TRUE(span.contains("endTimeUnixNano"));
  }
  EXPECT_FALSE(root.contains("parentSpanId"));
  EXPECT_EQ(generation.at("parentSpanId"), root.at("spanId"));
  EXPECT_EQ(tool.at("parentSpanId"), generation.at("spanId"));
  EXPECT_EQ(broken.at("parentSpanId"), generation.at("spanId"));

  // Trace-level attributes are propagated to every span.
  for (const auto& span : spans) {
    EXPECT_EQ(string_attribute(span, "langfuse.trace.name"), "sql-generation");
    EXPECT_EQ(string_attribute(span, "langfuse.user.id"), "user-1");
    EXPECT_EQ(string_attribute(span, "langfuse.session.id"), "session-1");
    EXPECT_EQ(
        JsonValue::parse(string_attribute(span, "langfuse.trace.metadata")),
        (JsonValue{{"tenant", "acme"}, {"n", 3}}));
    auto tags = attributes_of(span).at("langfuse.trace.tags");
    EXPECT_EQ(
        tags,
        (JsonValue{
            {"arrayValue",
             {{"values", {{{"stringValue", "a"}}, {{"stringValue", "b"}}}}}}}));
  }

  // Root span carries the trace input/output.
  EXPECT_EQ(string_attribute(root, "langfuse.observation.input"),
            "show tables");
  EXPECT_EQ(string_attribute(root, "langfuse.trace.input"), "show tables");
  EXPECT_EQ(string_attribute(root, "langfuse.observation.output"), "t1, t2");
  EXPECT_EQ(string_attribute(root, "langfuse.trace.output"), "t1, t2");

  // Generation.
  EXPECT_EQ(string_attribute(generation, "langfuse.observation.type"),
            "generation");
  EXPECT_EQ(string_attribute(generation, "langfuse.observation.model.name"),
            "test-model");
  EXPECT_EQ(string_attribute(generation, "gen_ai.request.model"), "test-model");
  EXPECT_EQ(JsonValue::parse(string_attribute(
                generation, "langfuse.observation.model.parameters")),
            (JsonValue{{"temperature", 0.5}, {"max_steps", 3}}));
  EXPECT_EQ(
      JsonValue::parse(
          string_attribute(generation, "langfuse.observation.input")),
      (JsonValue::array({{{"role", "system"}, {"content", "be brief"}},
                         {{"role", "user"}, {"content", "show tables"}}})));
  EXPECT_EQ(string_attribute(generation, "langfuse.observation.output"),
            "t1, t2");
  EXPECT_EQ(JsonValue::parse(string_attribute(
                generation, "langfuse.observation.usage_details")),
            (JsonValue{{"input", 10}, {"output", 5}, {"total", 15}}));
  auto gen_attrs = attributes_of(generation);
  EXPECT_EQ(gen_attrs.at("gen_ai.usage.input_tokens"),
            (JsonValue{{"intValue", "10"}}));
  EXPECT_EQ(gen_attrs.at("gen_ai.usage.output_tokens"),
            (JsonValue{{"intValue", "5"}}));
  EXPECT_EQ(JsonValue::parse(string_attribute(
                generation, "langfuse.observation.metadata"))["finish_reason"],
            "stop");
  EXPECT_FALSE(generation.contains("status"));

  // Tool spans.
  EXPECT_EQ(string_attribute(tool, "langfuse.observation.type"), "span");
  EXPECT_EQ(
      JsonValue::parse(string_attribute(tool, "langfuse.observation.input")),
      (JsonValue{{"sql", "SHOW TABLES"}}));
  EXPECT_EQ(
      JsonValue::parse(string_attribute(tool, "langfuse.observation.output")),
      (JsonValue{{"rows", 2}}));
  EXPECT_EQ(attributes_of(tool).count("langfuse.observation.level"), 0u);

  EXPECT_EQ(string_attribute(broken, "langfuse.observation.level"), "ERROR");
  EXPECT_EQ(string_attribute(broken, "langfuse.observation.status_message"),
            "tool failed");
  EXPECT_EQ(broken.at("status"),
            (JsonValue{{"code", 2}, {"message", "tool failed"}}));
}

TEST(LangfuseTracerTest, FailedGenerationIsMarkedAsError) {
  FakeLangfuseServer server;
  langfuse::Tracer tracer(config_for(server));

  auto trace = tracer.start_trace("failing");
  GenerateOptions options;
  options.model = "test-model";
  options.prompt = "hi";
  trace->instrument(options);
  trace->finish_generation(GenerateResult(std::string("rate limited")));
  ASSERT_TRUE(trace->end());

  auto body = JsonValue::parse(server.requests().at(0).body);
  const auto& spans = body["resourceSpans"][0]["scopeSpans"][0]["spans"];
  const JsonValue generation = find_span(spans, "generate_text");
  EXPECT_EQ(string_attribute(generation, "langfuse.observation.level"),
            "ERROR");
  EXPECT_EQ(string_attribute(generation, "langfuse.observation.status_message"),
            "rate limited");
  EXPECT_EQ(generation.at("status").at("code"), 2);
}

TEST(LangfuseTracerTest, UnfinishedSpansAreClosedAtEnd) {
  FakeLangfuseServer server;
  langfuse::Tracer tracer(config_for(server));

  auto trace = tracer.start_trace("unfinished");
  GenerateOptions options;
  options.model = "test-model";
  options.prompt = "hi";
  trace->instrument(options, "llm");
  (*options.on_tool_call_start)(ToolCall("call-1", "slow", JsonValue{}));
  ASSERT_TRUE(trace->end());

  auto body = JsonValue::parse(server.requests().at(0).body);
  const auto& spans = body["resourceSpans"][0]["scopeSpans"][0]["spans"];
  ASSERT_EQ(spans.size(), 3u);
  const JsonValue generation = find_span(spans, "llm");
  const JsonValue tool = find_span(spans, "slow");
  EXPECT_EQ(tool.at("parentSpanId"), generation.at("spanId"));
  EXPECT_TRUE(generation.contains("endTimeUnixNano"));
  EXPECT_TRUE(tool.contains("endTimeUnixNano"));
}

TEST(LangfuseTracerTest, EndIsIdempotent) {
  FakeLangfuseServer server;
  langfuse::Tracer tracer(config_for(server));

  auto trace = tracer.start_trace("once");
  ASSERT_TRUE(trace->end());
  ASSERT_TRUE(trace->end());
  EXPECT_EQ(server.requests().size(), 1u);
}

TEST(LangfuseTracerTest, ErrorPolicy) {
  FakeLangfuseServer server;
  server.respond_with(400, R"({"error":"bad request"})");

  langfuse::Tracer strict(config_for(server));
  EXPECT_FALSE(strict.start_trace("strict")->end());

  auto config = config_for(server);
  config.error_policy = langfuse::Config::ErrorPolicy::kBestEffort;
  langfuse::Tracer best_effort(config);
  EXPECT_TRUE(best_effort.start_trace("best-effort")->end());
}

TEST(LangfuseTracerTest, RejectedSpansAreAnError) {
  FakeLangfuseServer server;
  server.respond_with(
      200,
      R"({"partialSuccess":{"rejectedSpans":"1","errorMessage":"invalid"}})");

  langfuse::Tracer tracer(config_for(server));
  EXPECT_FALSE(tracer.start_trace("rejected")->end());
}

TEST(LangfuseTracerTest, InvalidConfigDoesNotSend) {
  langfuse::Config config;
  config.host = "http://127.0.0.1:1";
  langfuse::Tracer tracer(config);
  EXPECT_FALSE(tracer.is_valid());
  EXPECT_FALSE(tracer.start_trace("unconfigured")->end());
}

TEST(LangfuseTracerTest, GeneratesOpenTelemetryIds) {
  auto trace_id = langfuse::Trace::new_trace_id();
  auto span_id = langfuse::Trace::new_span_id();
  EXPECT_TRUE(is_lower_hex(trace_id, 32));
  EXPECT_TRUE(is_lower_hex(span_id, 16));
  EXPECT_NE(trace_id, langfuse::Trace::new_trace_id());
  EXPECT_NE(span_id, langfuse::Trace::new_span_id());
}

}  // namespace test
}  // namespace ai
