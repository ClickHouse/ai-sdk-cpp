#include "ai/anthropic.h"
#include "ai/openai.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/tool.h"

#include <memory>
#include <random>
#include <string>

#include <clickhouse/client.h>
#include <gtest/gtest.h>

namespace ai {
namespace test {

// Utility function to generate random suffix for table names
std::string generateRandomSuffix(size_t length = 8) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, sizeof(alphabet) - 2);

  std::string suffix;
  suffix.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    suffix += alphabet[dis(gen)];
  }
  return suffix;
}

// ClickHouse connection parameters
const std::string kClickhouseHost = "localhost";
const int kClickhousePort = 19000;  // Native protocol port from docker-compose
const std::string kClickhouseUser = "default";
const std::string kClickhousePassword = "changeme";

// System prompt for SQL generation
const std::string kSystemPrompt = R"(You are a ClickHouse SQL code generator.

CRITICAL INSTRUCTION: When you have gathered the information you need from tools, your final message must contain ONLY the SQL statement. No explanations, no markdown, just the raw SQL.

TOOLS:
- list_databases(): Lists databases
- list_tables_in_database(database): Lists tables in a database
- get_schema_for_table(database, table): Gets table schema

ABSOLUTE RULES:
1. Use tools first if you need schema information
2. After using tools, output ONLY the SQL query
3. NEVER include explanations, markdown blocks, or commentary in your final output
4. Your final message = SQL statement only

WORKFLOW EXAMPLE:
User: "insert 3 rows into users table in test_db"
Step 1: Call get_schema_for_table("test_db", "users") 
Step 2: Output: INSERT INTO test_db.users VALUES (1, 'Alice', 25), (2, 'Bob', 30), (3, 'Charlie', 35);

CORRECT FINAL OUTPUTS:
CREATE TABLE github_events (id UInt64, type String, actor_id UInt64, actor_login String, repo_id UInt64, repo_name String, created_at DateTime, payload String) ENGINE = MergeTree() ORDER BY (created_at, repo_id);
INSERT INTO users VALUES (1, 'Alice', 25), (2, 'Bob', 30), (3, 'Charlie', 35);
SELECT * FROM users;

IMPORTANT: For CREATE TABLE statements, use String type for complex data instead of JSON type.

INCORRECT OUTPUTS (NEVER DO THIS):
"Here is the SQL: INSERT INTO..."
"```sql\nSELECT * FROM users;\n```"
"The query would be: SELECT..."
Any text before or after the SQL
```sql CREATE TABLE ... ```  (NO CODE BLOCKS!)

CRITICAL: Do NOT wrap SQL in markdown code blocks with ```sql or ```. Just output the raw SQL.

Your final output must be executable SQL only.)";

// Tool implementations
class ClickHouseTools {
 public:
  explicit ClickHouseTools(std::shared_ptr<clickhouse::Client> client)
      : client_(std::move(client)) {}

  Tool createListDatabasesTool() {
    return create_simple_tool(
        "list_databases",
        "List all available databases in the ClickHouse instance", {},
        [this](const JsonValue& args, const ToolExecutionContext& context) {
          try {
            std::vector<std::string> databases;
            client_->Select(
                "SELECT name FROM system.databases ORDER BY name",
                [&databases](const clickhouse::Block& block) {
                  for (size_t i = 0; i < block.GetRowCount(); ++i) {
                    databases.push_back(std::string(
                        block[0]->As<clickhouse::ColumnString>()->At(i)));
                  }
                });

            std::stringstream result;
            result << "Found " << databases.size() << " databases:\n";
            for (const auto& db : databases) {
              result << "- " << db << "\n";
            }

            return JsonValue{{"result", result.str()},
                             {"databases", databases}};
          } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error: ") + e.what());
          }
        });
  }

  Tool createListTablesInDatabaseTool() {
    return create_simple_tool(
        "list_tables_in_database", "List all tables in a specific database",
        {{"database", "string"}},
        [this](const JsonValue& args, const ToolExecutionContext& context) {
          try {
            std::string database = args["database"].get<std::string>();
            std::vector<std::string> tables;

            std::string query =
                "SELECT name FROM system.tables WHERE database = '" + database +
                "' ORDER BY name";
            client_->Select(query, [&tables](const clickhouse::Block& block) {
              for (size_t i = 0; i < block.GetRowCount(); ++i) {
                tables.push_back(std::string(
                    block[0]->As<clickhouse::ColumnString>()->At(i)));
              }
            });

            std::stringstream result;
            result << "Found " << tables.size() << " tables in database '"
                   << database << "':\n";
            for (const auto& table : tables) {
              result << "- " << table << "\n";
            }
            if (tables.empty()) {
              result << "(No tables found in this database)\n";
            }

            return JsonValue{{"result", result.str()},
                             {"database", database},
                             {"tables", tables}};
          } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error: ") + e.what());
          }
        });
  }

  Tool createGetSchemaForTableTool() {
    return create_simple_tool(
        "get_schema_for_table",
        "Get the CREATE TABLE statement (schema) for a specific table",
        {{"database", "string"}, {"table", "string"}},
        [this](const JsonValue& args, const ToolExecutionContext& context) {
          try {
            std::string database = args["database"].get<std::string>();
            std::string table = args["table"].get<std::string>();

            std::string query =
                "SHOW CREATE TABLE `" + database + "`.`" + table + "`";
            std::string schema;

            client_->Select(query, [&schema](const clickhouse::Block& block) {
              if (block.GetRowCount() > 0) {
                schema = block[0]->As<clickhouse::ColumnString>()->At(0);
              }
            });

            if (schema.empty()) {
              throw std::runtime_error("Could not retrieve schema for " +
                                       database + "." + table);
            }

            return JsonValue{{"result", "Schema for " + database + "." + table +
                                            ":\n" + schema},
                             {"database", database},
                             {"table", table},
                             {"schema", schema}};
          } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error: ") + e.what());
          }
        });
  }

 private:
  std::shared_ptr<clickhouse::Client> client_;
};

// Base test fixture
class ClickHouseIntegrationTest : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() override {
    // Generate random suffix for table names to allow parallel test execution
    table_suffix_ = generateRandomSuffix();

    // Initialize ClickHouse client
    clickhouse::ClientOptions options;
    options.SetHost(kClickhouseHost);
    options.SetPort(kClickhousePort);
    options.SetUser(kClickhouseUser);
    options.SetPassword(kClickhousePassword);

    clickhouse_client_ = std::make_shared<clickhouse::Client>(options);

    // Create test database
    clickhouse_client_->Execute("CREATE DATABASE IF NOT EXISTS test_db");

    // Initialize tools
    tools_helper_ = std::make_unique<ClickHouseTools>(clickhouse_client_);
    tools_ = {
        {"list_databases", tools_helper_->createListDatabasesTool()},
        {"list_tables_in_database",
         tools_helper_->createListTablesInDatabaseTool()},
        {"get_schema_for_table", tools_helper_->createGetSchemaForTableTool()}};

    // Initialize AI client based on provider
    provider_type_ = GetParam();
    if (provider_type_ == "openai") {
      const char* api_key = std::getenv("OPENAI_API_KEY");
      if (!api_key) {
        use_real_api_ = false;
        return;
      }
      client_ = std::make_shared<Client>(openai::create_client());
      model_ = openai::models::kGpt4o;
    } else if (provider_type_ == "anthropic") {
      const char* api_key = std::getenv("ANTHROPIC_API_KEY");
      if (!api_key) {
        use_real_api_ = false;
        return;
      }
      client_ = std::make_shared<Client>(anthropic::create_client());
      model_ = anthropic::models::kClaudeSonnet4;
    }
    use_real_api_ = true;
  }

  void TearDown() override {
    // Clean up test database
    if (clickhouse_client_) {
      clickhouse_client_->Execute("DROP DATABASE IF EXISTS test_db");
    }
  }

  std::string executeSQLGeneration(const std::string& prompt) {
    GenerateOptions options(model_, prompt);
    options.system = kSystemPrompt;
    options.tools = tools_;
    options.max_tokens = 1000;
    options.max_steps = 5;  // Allow multiple rounds of tool calls

    // Log the prompt being sent
    std::cout << "\n=== SQL Generation Request ===" << std::endl;
    std::cout << "Prompt: " << prompt << std::endl;
    std::cout << "Model: " << model_ << std::endl;
    std::cout << "Provider: " << provider_type_ << std::endl;

    auto result = client_->generate_text(options);

    if (!result.is_success()) {
      throw std::runtime_error("Failed to generate SQL: " +
                               result.error_message());
    }

    std::cout << "\n=== SQL Generation Response ===" << std::endl;
    std::cout << "Raw AI response: '" << result.text << "'" << std::endl;
    std::cout << "Tool calls made: " << result.tool_calls.size() << std::endl;
    std::cout << "Tool results: " << result.tool_results.size() << std::endl;
    std::cout << "Steps taken: " << result.steps.size() << std::endl;

    // For multi-step results, we want only the final step's text
    if (!result.steps.empty()) {
      // Debug all steps
      for (size_t i = 0; i < result.steps.size(); ++i) {
        const auto& step = result.steps[i];
        std::cout << "\n--- Step " << i + 1 << " ---" << std::endl;
        std::cout << "Text: '" << step.text << "'" << std::endl;
        std::cout << "Tool calls: " << step.tool_calls.size() << std::endl;
        for (const auto& tool_call : step.tool_calls) {
          std::cout << "  - Tool: " << tool_call.tool_name
                    << " (id: " << tool_call.id << ")" << std::endl;
          std::cout << "    Args: " << tool_call.arguments.dump() << std::endl;
        }
        std::cout << "Tool results: " << step.tool_results.size() << std::endl;
        for (const auto& tool_result : step.tool_results) {
          std::cout << "  - Result for " << tool_result.tool_name
                    << " (id: " << tool_result.tool_call_id << ")" << std::endl;
          if (tool_result.is_success()) {
            std::cout << "    Success: " << tool_result.result.dump()
                      << std::endl;
          } else {
            std::cout << "    Error: " << tool_result.error_message()
                      << std::endl;
          }
        }
        std::cout << "Finish reason: " << step.finish_reason << std::endl;
      }

      // Find the last step with non-empty text
      for (auto it = result.steps.rbegin(); it != result.steps.rend(); ++it) {
        if (!it->text.empty()) {
          std::cout << "\nReturning text from final step: '" << it->text << "'"
                    << std::endl;
          return it->text;
        }
      }
    }

    return result.text;
  }

  // Utility to clean SQL from markdown blocks if present
  std::string cleanSQL(const std::string& sql) {
    std::string cleaned = sql;

    // Remove leading/trailing whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
      cleaned = cleaned.substr(start, end - start + 1);
    }

    // Check if wrapped in markdown code blocks
    if (cleaned.substr(0, 6) == "```sql" || cleaned.substr(0, 3) == "```") {
      size_t code_start = cleaned.find('\n');
      size_t code_end = cleaned.rfind("```");
      if (code_start != std::string::npos && code_end != std::string::npos &&
          code_end > code_start) {
        cleaned = cleaned.substr(code_start + 1, code_end - code_start - 1);
        // Trim again
        start = cleaned.find_first_not_of(" \t\n\r");
        end = cleaned.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
          cleaned = cleaned.substr(start, end - start + 1);
        }
      }
    }

    return cleaned;
  }

  std::shared_ptr<clickhouse::Client> clickhouse_client_;
  std::unique_ptr<ClickHouseTools> tools_helper_;
  ToolSet tools_;
  std::shared_ptr<Client> client_;
  std::string model_;
  std::string provider_type_;
  std::string table_suffix_;
  bool use_real_api_ = false;
};

// Tests
TEST_P(ClickHouseIntegrationTest, CreateTableForGithubEvents) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  std::string table_name = "github_events_" + table_suffix_;

  // Clean up any existing table
  clickhouse_client_->Execute("DROP TABLE IF EXISTS test_db." + table_name);
  clickhouse_client_->Execute("DROP TABLE IF EXISTS default." + table_name);

  std::string sql =
      executeSQLGeneration("create a table named " + table_name +
                           " for github events in test_db database");

  // Clean SQL in case it's wrapped in markdown
  sql = cleanSQL(sql);

  // Execute the generated SQL
  ASSERT_NO_THROW(clickhouse_client_->Execute("USE test_db"));
  ASSERT_NO_THROW(clickhouse_client_->Execute(sql));

  // Verify table was created
  bool table_exists = false;
  clickhouse_client_->Select(
      "EXISTS TABLE test_db." + table_name,
      [&table_exists](const clickhouse::Block& block) {
        if (block.GetRowCount() > 0) {
          table_exists = block[0]->As<clickhouse::ColumnUInt8>()->At(0);
        }
      });
  EXPECT_TRUE(table_exists);
}

TEST_P(ClickHouseIntegrationTest, InsertAndQueryData) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  std::string table_name = "users_" + table_suffix_;

  // First create a users table
  clickhouse_client_->Execute("USE test_db");
  clickhouse_client_->Execute("DROP TABLE IF EXISTS " + table_name);
  clickhouse_client_->Execute(
      "CREATE TABLE " + table_name +
      " (id UInt64, name String, age UInt8) ENGINE = MergeTree() ORDER BY id");

  // Generate INSERT SQL
  std::string insert_sql = executeSQLGeneration(
      "insert 3 rows into " + table_name + " table in test_db");
  std::cout << "Generated INSERT SQL: " << insert_sql << std::endl;
  ASSERT_NO_THROW(clickhouse_client_->Execute(insert_sql));

  // Generate SELECT SQL
  std::string select_sql =
      executeSQLGeneration("show all users from " + table_name + " in test_db");

  // Verify data was inserted
  size_t row_count = 0;
  clickhouse_client_->Select(select_sql,
                             [&row_count](const clickhouse::Block& block) {
                               row_count += block.GetRowCount();
                             });
  EXPECT_EQ(row_count, 3);
}

TEST_P(ClickHouseIntegrationTest, ExploreExistingSchema) {
  if (!use_real_api_) {
    GTEST_SKIP() << "No API key set for " << GetParam();
  }

  std::string orders_table = "orders_" + table_suffix_;
  std::string products_table = "products_" + table_suffix_;

  // Create some test tables
  clickhouse_client_->Execute("USE test_db");
  clickhouse_client_->Execute("DROP TABLE IF EXISTS " + orders_table);
  clickhouse_client_->Execute("DROP TABLE IF EXISTS " + products_table);
  clickhouse_client_->Execute(
      "CREATE TABLE " + orders_table +
      " (id UInt64, user_id UInt64, amount Decimal(10,2), status String) "
      "ENGINE = MergeTree() ORDER BY id");
  clickhouse_client_->Execute("CREATE TABLE " + products_table +
                              " (id UInt64, name String, price Decimal(10,2), "
                              "stock UInt32) ENGINE = MergeTree() ORDER BY id");

  // Add some data to orders table
  clickhouse_client_->Execute(
      "INSERT INTO " + orders_table +
      " VALUES (1, 100, 50.25, 'completed'), (2, 101, 150.00, 'pending'), (3, "
      "100, 200.50, 'completed')");

  // Test schema exploration
  std::string sql = executeSQLGeneration(
      "find all orders with amount greater than 100 from " + orders_table +
      " in test_db");
  std::cout << "Generated SELECT SQL: " << sql << std::endl;

  // The generated SQL should reference the correct table and columns
  EXPECT_TRUE(sql.find(orders_table) != std::string::npos);
  EXPECT_TRUE(sql.find("amount") != std::string::npos);
  EXPECT_TRUE(sql.find("100") != std::string::npos);
}

// Instantiate tests for both providers
INSTANTIATE_TEST_SUITE_P(Providers,
                         ClickHouseIntegrationTest,
                         ::testing::Values("openai", "anthropic"));

}  // namespace test
}  // namespace ai