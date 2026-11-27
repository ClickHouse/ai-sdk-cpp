#include "ai/anthropic.h"
#include "ai/openai.h"

#include <httplib.h>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

namespace {

// Serves canned responses on a local port for the lifetime of the object.
class LocalServer {
 public:
  explicit LocalServer(
      const std::function<void(httplib::Server&)>& register_handlers) {
    register_handlers(server_);
    port_ = server_.bind_to_any_port("127.0.0.1");
    thread_ = std::thread([this] { server_.listen_after_bind(); });
    server_.wait_until_ready();
  }

  ~LocalServer() {
    server_.stop();
    thread_.join();
  }

  std::string url() const {
    return "http://127.0.0.1:" + std::to_string(port_);
  }

 private:
  httplib::Server server_;
  int port_ = 0;
  std::thread thread_;
};

}  // namespace

TEST(ListModelsTest, AnthropicFollowsPagination) {
  std::vector<std::string> seen_after_ids;
  std::string seen_api_key;
  std::string seen_version;
  LocalServer server([&](httplib::Server& s) {
    s.Get("/v1/models", [&](const httplib::Request& req,
                            httplib::Response& res) {
      seen_api_key = req.get_header_value("x-api-key");
      seen_version = req.get_header_value("anthropic-version");
      const auto after_id = req.get_param_value("after_id");
      seen_after_ids.push_back(after_id);
      if (after_id.empty()) {
        res.set_content(
            R"({"data":[{"id":"claude-opus-5-5"},{"id":"claude-sonnet-5-5"}],)"
            R"("has_more":true,"first_id":"claude-opus-5-5","last_id":"claude-sonnet-5-5"})",
            "application/json");
      } else {
        res.set_content(
            R"({"data":[{"id":"claude-haiku-5-5"}],"has_more":false,)"
            R"("first_id":"claude-haiku-5-5","last_id":"claude-haiku-5-5"})",
            "application/json");
      }
    });
  });

  auto client = anthropic::create_client("sk-ant-test", server.url());
  const auto result = client.list_models();

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_THAT(result.models,
              testing::ElementsAre("claude-opus-5-5", "claude-sonnet-5-5",
                                   "claude-haiku-5-5"));
  EXPECT_THAT(seen_after_ids, testing::ElementsAre("", "claude-sonnet-5-5"));
  EXPECT_EQ(seen_api_key, "sk-ant-test");
  EXPECT_FALSE(seen_version.empty());
}

TEST(ListModelsTest, FailsWhenPaginationDoesNotAdvance) {
  int requests = 0;
  LocalServer server([&](httplib::Server& s) {
    s.Get("/v1/models", [&](const httplib::Request&, httplib::Response& res) {
      // Ignores `after_id` and always reports more pages.
      ++requests;
      res.set_content(R"({"data":[{"id":"claude-opus-5-5"}],"has_more":true,)"
                      R"("last_id":"claude-opus-5-5"})",
                      "application/json");
    });
  });

  auto client = anthropic::create_client("sk-ant-test", server.url());
  const auto result = client.list_models();

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("repeated"));
  EXPECT_EQ(requests, 2);
}

TEST(ListModelsTest, OpenAIReturnsAllModelsAtOnce) {
  std::string seen_authorization;
  LocalServer server([&](httplib::Server& s) {
    s.Get("/v1/models", [&](const httplib::Request& req,
                            httplib::Response& res) {
      seen_authorization = req.get_header_value("Authorization");
      res.set_content(
          R"({"object":"list","data":[{"id":"gpt-6.2-sol","object":"model"},)"
          R"({"id":"gpt-6-luna","object":"model"}]})",
          "application/json");
    });
  });

  auto client = openai::create_client("sk-test", server.url());
  const auto result = client.list_models();

  ASSERT_TRUE(result.is_success()) << result.error_message();
  EXPECT_THAT(result.models, testing::ElementsAre("gpt-6.2-sol", "gpt-6-luna"));
  EXPECT_EQ(seen_authorization, "Bearer sk-test");
}

TEST(ListModelsTest, ReportsProviderErrors) {
  LocalServer server([](httplib::Server& s) {
    s.Get("/v1/models", [](const httplib::Request&, httplib::Response& res) {
      res.status = 401;
      res.set_content(
          R"({"error":{"message":"Incorrect API key provided","type":"invalid_request_error"}})",
          "application/json");
    });
  });

  auto client = openai::create_client("sk-wrong", server.url());
  const auto result = client.list_models();

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(),
              testing::HasSubstr("Incorrect API key provided"));
  EXPECT_TRUE(result.models.empty());
}

TEST(ListModelsTest, RejectsUnexpectedResponseShape) {
  LocalServer server([](httplib::Server& s) {
    s.Get("/v1/models", [](const httplib::Request&, httplib::Response& res) {
      res.set_content(R"({"models":[]})", "application/json");
    });
  });

  auto client = openai::create_client("sk-test", server.url());
  const auto result = client.list_models();

  EXPECT_FALSE(result.is_success());
  EXPECT_THAT(result.error_message(), testing::HasSubstr("data"));
}

TEST(ListModelsTest, AnyNonEmptyModelNameIsSupported) {
  auto openai_client = openai::create_client("sk-test");
  auto anthropic_client = anthropic::create_client("sk-ant-test");

  // Model names are forwarded as is, so new and gateway-specific names work
  // without an SDK update.
  for (const auto* model :
       {"gpt-7", "claude-opus-9-9", "anthropic/claude-sonnet-5-5"}) {
    EXPECT_TRUE(openai_client.supports_model(model)) << model;
    EXPECT_TRUE(anthropic_client.supports_model(model)) << model;
  }
  EXPECT_FALSE(openai_client.supports_model(""));
  EXPECT_FALSE(anthropic_client.supports_model(""));
}

}  // namespace test
}  // namespace ai
