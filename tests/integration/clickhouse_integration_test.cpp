#include <gtest/gtest.h>
#include "ai/openai.h"
#include "ai/anthropic.h"
#include "ai/tools.h"
#include "ai/types/generate_options.h"
#include "ai/types/tool.h"
#include <clickhouse/client.h>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <map>

namespace ai {
namespace test {

// ClickHouse connection parameters
const std::string kClickhouseHost = "localhost";
const int kClickhousePort = 19000;  // Native protocol port from docker-compose
const std::string kClickhouseUser = "default";
const std::string kClickhousePassword = "changeme";

// System prompt for SQL generation
const std::string kSystemPrompt = R"(You are a ClickHouse SQL generator. Convert natural language to ClickHouse SQL queries.

TOOLS AVAILABLE:
- list_databases(): See available databases
- list_tables_in_database(database): See tables in a database
- get_schema_for_table(database, table): Get table structure

IMPORTANT RULES:
1. When you have enough information to write the SQL, STOP using tools
2. After getting schema information, generate the SQL immediately
3. Your final response must be ONLY the SQL query - no explanations
4. Don't call the same tool twice with the same parameters
5. Skip system/information_schema databases unless requested
6. If you've already seen the tool result, don't call it again - use the information you have

WORKFLOW:
- For CREATE TABLE: You usually don't need tools, just generate the SQL
- For INSERT: If table name is mentioned, check its schema then generate SQL
- For SELECT/UPDATE/DELETE: Check if table exists and get schema if needed
- Once you know the schema, STOP exploring and return the SQL

EXAMPLES:
"create a table for github events"
→ CREATE TABLE github_events (id UInt64, type String, actor_id UInt64, actor_login String, repo_id UInt64, repo_name String, created_at DateTime, payload String) ENGINE = MergeTree() ORDER BY (created_at, repo_id);

"insert 3 rows into users table"  
→ INSERT INTO users VALUES (1, 'Alice', 25), (2, 'Bob', 30), (3, 'Charlie', 35);

"show all orders"
→ SELECT * FROM orders;

Remember: As soon as you have the information needed, provide ONLY the SQL query.)";

// Tool implementations
class ClickHouseTools {
public:
    ClickHouseTools(std::shared_ptr<clickhouse::Client> client) : client_(client) {}

    Tool createListDatabasesTool() {
        return create_simple_tool(
            "list_databases", 
            "List all available databases in the ClickHouse instance",
            {},
            [this](const JsonValue& args, const ToolExecutionContext& context) {
                try {
                    std::vector<std::string> databases;
                    client_->Select("SELECT name FROM system.databases ORDER BY name",
                        [&databases](const clickhouse::Block& block) {
                            for (size_t i = 0; i < block.GetRowCount(); ++i) {
                                databases.push_back(std::string(block[0]->As<clickhouse::ColumnString>()->At(i)));
                            }
                        }
                    );

                    std::stringstream result;
                    result << "Found " << databases.size() << " databases:\n";
                    for (const auto& db : databases) {
                        result << "- " << db << "\n";
                    }

                    return JsonValue{
                        {"result", result.str()},
                        {"databases", databases}
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Error: ") + e.what());
                }
            }
        );
    }

    Tool createListTablesInDatabaseTool() {
        return create_simple_tool(
            "list_tables_in_database",
            "List all tables in a specific database",
            {{"database", "string"}},
            [this](const JsonValue& args, const ToolExecutionContext& context) {
                try {
                    std::string database = args["database"].get<std::string>();
                    std::vector<std::string> tables;
                    
                    std::string query = "SELECT name FROM system.tables WHERE database = '" + database + "' ORDER BY name";
                    client_->Select(query,
                        [&tables](const clickhouse::Block& block) {
                            for (size_t i = 0; i < block.GetRowCount(); ++i) {
                                tables.push_back(std::string(block[0]->As<clickhouse::ColumnString>()->At(i)));
                            }
                        }
                    );

                    std::stringstream result;
                    result << "Found " << tables.size() << " tables in database '" << database << "':\n";
                    for (const auto& table : tables) {
                        result << "- " << table << "\n";
                    }
                    if (tables.empty()) {
                        result << "(No tables found in this database)\n";
                    }

                    return JsonValue{
                        {"result", result.str()},
                        {"database", database},
                        {"tables", tables}
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Error: ") + e.what());
                }
            }
        );
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
                    
                    std::string query = "SHOW CREATE TABLE `" + database + "`.`" + table + "`";
                    std::string schema;
                    
                    client_->Select(query,
                        [&schema](const clickhouse::Block& block) {
                            if (block.GetRowCount() > 0) {
                                schema = block[0]->As<clickhouse::ColumnString>()->At(0);
                            }
                        }
                    );

                    if (schema.empty()) {
                        throw std::runtime_error("Could not retrieve schema for " + database + "." + table);
                    }

                    return JsonValue{
                        {"result", "Schema for " + database + "." + table + ":\n" + schema},
                        {"database", database},
                        {"table", table},
                        {"schema", schema}
                    };
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Error: ") + e.what());
                }
            }
        );
    }

private:
    std::shared_ptr<clickhouse::Client> client_;
};

// Base test fixture
class ClickHouseIntegrationTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
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
            {"list_tables_in_database", tools_helper_->createListTablesInDatabaseTool()},
            {"get_schema_for_table", tools_helper_->createGetSchemaForTableTool()}
        };
        
        // Initialize AI client based on provider
        std::string provider = GetParam();
        if (provider == "openai") {
            const char* api_key = std::getenv("OPENAI_API_KEY");
            if (!api_key) {
                use_real_api_ = false;
                return;
            }
            client_ = std::make_shared<Client>(openai::create_client());
            model_ = openai::models::kGpt4o;
        } else if (provider == "anthropic") {
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
        options.max_tokens = 500;
        
        auto result = client_->generate_text(options);
        
        if (!result.is_success()) {
            throw std::runtime_error("Failed to generate SQL: " + result.error_message());
        }
        
        return result.text;
    }

    std::shared_ptr<clickhouse::Client> clickhouse_client_;
    std::unique_ptr<ClickHouseTools> tools_helper_;
    ToolSet tools_;
    std::shared_ptr<Client> client_;
    std::string model_;
    bool use_real_api_ = false;
};

// Tests
TEST_P(ClickHouseIntegrationTest, CreateTableForGithubEvents) {
    if (!use_real_api_) {
        GTEST_SKIP() << "No API key set for " << GetParam();
    }
    
    std::string sql = executeSQLGeneration("create a table for github events");
    
    // Execute the generated SQL
    ASSERT_NO_THROW(clickhouse_client_->Execute("USE test_db"));
    ASSERT_NO_THROW(clickhouse_client_->Execute(sql));
    
    // Verify table was created
    bool table_exists = false;
    clickhouse_client_->Select("EXISTS TABLE test_db.github_events",
        [&table_exists](const clickhouse::Block& block) {
            if (block.GetRowCount() > 0) {
                table_exists = block[0]->As<clickhouse::ColumnUInt8>()->At(0);
            }
        }
    );
    EXPECT_TRUE(table_exists);
}

TEST_P(ClickHouseIntegrationTest, InsertAndQueryData) {
    if (!use_real_api_) {
        GTEST_SKIP() << "No API key set for " << GetParam();
    }
    
    // First create a users table
    clickhouse_client_->Execute("USE test_db");
    clickhouse_client_->Execute("CREATE TABLE users (id UInt64, name String, age UInt8) ENGINE = MergeTree() ORDER BY id");
    
    // Generate INSERT SQL
    std::string insert_sql = executeSQLGeneration("insert 3 rows into users table in test_db");
    ASSERT_NO_THROW(clickhouse_client_->Execute(insert_sql));
    
    // Generate SELECT SQL
    std::string select_sql = executeSQLGeneration("show all users from test_db");
    
    // Verify data was inserted
    size_t row_count = 0;
    clickhouse_client_->Select(select_sql,
        [&row_count](const clickhouse::Block& block) {
            row_count += block.GetRowCount();
        }
    );
    EXPECT_EQ(row_count, 3);
}

TEST_P(ClickHouseIntegrationTest, ExploreExistingSchema) {
    if (!use_real_api_) {
        GTEST_SKIP() << "No API key set for " << GetParam();
    }
    
    // Create some test tables
    clickhouse_client_->Execute("USE test_db");
    clickhouse_client_->Execute("CREATE TABLE orders (id UInt64, user_id UInt64, amount Decimal(10,2), status String) ENGINE = MergeTree() ORDER BY id");
    clickhouse_client_->Execute("CREATE TABLE products (id UInt64, name String, price Decimal(10,2), stock UInt32) ENGINE = MergeTree() ORDER BY id");
    
    // Test schema exploration
    std::string sql = executeSQLGeneration("find all orders with amount greater than 100 in test_db");
    
    // The generated SQL should reference the correct table and columns
    EXPECT_TRUE(sql.find("orders") != std::string::npos);
    EXPECT_TRUE(sql.find("amount") != std::string::npos);
    EXPECT_TRUE(sql.find("100") != std::string::npos);
}

// Instantiate tests for both providers
INSTANTIATE_TEST_SUITE_P(
    Providers,
    ClickHouseIntegrationTest,
    ::testing::Values("openai", "anthropic")
);

} // namespace test
} // namespace ai