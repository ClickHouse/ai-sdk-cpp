/**
 * AI SQL Generator Worker - AI SDK C++
 *
 * HTTP server that generates SQL queries using OpenAI.
 * Designed to run as a systemd service on localhost only.
 *
 * Features:
 * - HTTP server on port 8088 (localhost only)
 * - Reads OpenAI API key from /etc/ai_worker/openai_key
 * - Endpoint: POST /generate
 * - Request: { "prompt": "...", "mode": "sql_only", "schema_version": "v1" }
 * - Response: { "sql": "SELECT ...", "validated": true }
 * - Basic SQL validation (SELECT only, no DDL)
 * - Schema version checking
 *
 * Usage:
 *   sudo systemctl start ai_worker
 *   curl -X POST http://127.0.0.1:8088/generate \
 *     -H "Content-Type: application/json" \
 *     -d '{"prompt":"Get all users","mode":"sql_only","schema_version":"v1"}'
 */

#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <ai/openai.h>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include "pg_cache.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Configuration
constexpr const char* OPENAI_KEY_PATH = "/etc/ai_worker/openai_key";
constexpr const char* OPENAI_KEY_PATH_DEV = "./openai_key";  // fallback for development
constexpr const char* LISTEN_HOST = "127.0.0.1";
constexpr int LISTEN_PORT = 8088;
constexpr const char* CURRENT_SCHEMA_VERSION = "v1";

// SQL validation patterns
const std::vector<std::string> FORBIDDEN_KEYWORDS = {
    "INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER", 
    "TRUNCATE", "GRANT", "REVOKE", "SET", "BEGIN", "COMMIT", 
    "ROLLBACK", "COPY", "EXECUTE", "DECLARE", "PREPARE"
};

class AIWorker {
public:
    AIWorker() : initialized_(false), cache_(nullptr) {}

    bool initialize() {
        // Try to read OpenAI API key
        std::string api_key = read_api_key();
        if (api_key.empty()) {
            std::cerr << "Failed to read OpenAI API key from " 
                      << OPENAI_KEY_PATH << " or " << OPENAI_KEY_PATH_DEV << std::endl;
            return false;
        }

        // Create OpenAI client
        try {
            client_ = ai::openai::create_client(api_key);
            initialized_ = true;
            std::cout << "AI Worker initialized successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize OpenAI client: " << e.what() << std::endl;
            return false;
        }

        // Initialize cache (optional - worker continues without cache if it fails)
        try {
            ai_worker::PgCache::Config cache_config;
            // Use defaults from Config struct (which you've customized)
            // Only override if environment variables are explicitly set
            if (const char* host = std::getenv("POSTGRES_HOST")) {
                cache_config.host = host;
            }
            if (const char* port = std::getenv("POSTGRES_PORT")) {
                cache_config.port = std::stoi(port);
            }
            if (const char* dbname = std::getenv("POSTGRES_DB")) {
                cache_config.dbname = dbname;
            }
            if (const char* user = std::getenv("POSTGRES_USER")) {
                cache_config.user = user;
            }
            if (const char* password = std::getenv("POSTGRES_PASSWORD")) {
                cache_config.password = password;
            }
            cache_config.default_ttl = std::chrono::seconds(3600);  // 1 hour
            cache_config.auto_create_schema = true;

            cache_ = std::make_unique<ai_worker::PgCache>(cache_config);
            if (cache_->initialize()) {
                std::cout << "Cache initialized successfully" << std::endl;
            } else {
                std::cerr << "Cache initialization failed: " << cache_->last_error() << std::endl;
                std::cerr << "Continuing without cache..." << std::endl;
                cache_.reset();
            }
        } catch (const std::exception& e) {
            std::cerr << "Cache initialization error: " << e.what() << std::endl;
            std::cerr << "Continuing without cache..." << std::endl;
            cache_.reset();
        }

        return true;
    }

    json generate_sql(const std::string& prompt, const std::string& mode, 
                      const std::string& schema_version) {
        json response;
        const std::string model = "gpt-4o-mini";

        // Validate schema version
        if (schema_version != CURRENT_SCHEMA_VERSION) {
            response["error"] = "Schema version mismatch";
            response["expected_version"] = CURRENT_SCHEMA_VERSION;
            response["received_version"] = schema_version;
            return response;
        }

        // Try cache first
        if (cache_ && cache_->is_connected()) {
            auto cached = cache_->get(prompt, model, schema_version);
            if (cached) {
                response["sql"] = cached->sql;
                response["validated"] = cached->validated;
                response["tokens_used"] = cached->tokens_used;
                response["schema_version"] = CURRENT_SCHEMA_VERSION;
                response["cache_hit"] = true;
                response["hit_count"] = cached->hit_count;
                return response;
            }
        }

        // Cache miss - generate SQL using OpenAI
        std::string system_prompt = 
            "You are a PostgreSQL SQL expert. Generate ONLY valid PostgreSQL SELECT queries. "
            "Rules:\n"
            "1. Return ONLY the SQL query, no explanations or markdown\n"
            "2. Use only SELECT statements\n"
            "3. Always include a LIMIT clause (max 1000)\n"
            "4. Use proper PostgreSQL syntax\n"
            "5. Do not use semicolons\n"
            "6. No DDL, DML (except SELECT), or administrative commands\n"
            "7. Use table and column names that are common in typical databases";

        ai::GenerateOptions options;
        options.model = model;
        options.system = system_prompt;
        options.prompt = prompt;
        options.temperature = 0.0;  // Deterministic output
        options.max_tokens = 500;

        auto result = client_.generate_text(options);

        if (!result) {
            response["error"] = result.error_message();
            response["validated"] = false;
            response["cache_hit"] = false;
            return response;
        }

        std::string generated_sql = result.text;
        int tokens_used = result.usage.total_tokens;
        
        // Clean up the SQL (remove markdown, extra whitespace, etc.)
        generated_sql = clean_sql(generated_sql);

        // Validate the generated SQL
        auto validation_result = validate_sql(generated_sql);
        
        if (!validation_result.first) {
            response["error"] = validation_result.second;
            response["validated"] = false;
            response["raw_sql"] = generated_sql;
            return response;
        }

        // Store in cache
        if (cache_ && cache_->is_connected()) {
            cache_->put(prompt, model, schema_version, generated_sql, true, tokens_used);
        }

        // Return validated SQL
        response["sql"] = generated_sql;
        response["validated"] = true;
        response["tokens_used"] = tokens_used;
        response["schema_version"] = CURRENT_SCHEMA_VERSION;
        response["cache_hit"] = false;

        return response;
    }

    json get_cache_stats() {
        if (cache_ && cache_->is_connected()) {
            return cache_->get_stats();
        }
        json stats;
        stats["error"] = "Cache not available";
        return stats;
    }

    int cleanup_cache() {
        if (cache_ && cache_->is_connected()) {
            return cache_->cleanup_expired();
        }
        return 0;
    }

private:
    std::string read_api_key() {
        // Try production path first
        std::ifstream file(OPENAI_KEY_PATH);
        if (!file.is_open()) {
            // Try development path
            file.open(OPENAI_KEY_PATH_DEV);
            if (!file.is_open()) {
                // Try environment variable as last resort
                const char* env_key = std::getenv("OPENAI_API_KEY");
                if (env_key) {
                    return std::string(env_key);
                }
                return "";
            }
        }

        std::string key;
        std::getline(file, key);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\n\r"));
        key.erase(key.find_last_not_of(" \t\n\r") + 1);
        
        return key;
    }

    std::string clean_sql(const std::string& sql) {
        std::string cleaned = sql;

        // Remove markdown code blocks
        // Note: Using multiline mode since dotall doesn't exist in std::regex
        std::regex markdown_regex(R"(```(?:sql)?[\s\S]*?(.*?)[\s\S]*?```)", 
                                 std::regex::icase);
        std::smatch match;
        if (std::regex_search(cleaned, match, markdown_regex)) {
            cleaned = match[1].str();
        }

        // Remove leading/trailing whitespace
        cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
        cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

        // Remove trailing semicolon if present
        if (!cleaned.empty() && cleaned.back() == ';') {
            cleaned.pop_back();
        }

        // Normalize whitespace
        cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");

        return cleaned;
    }

    std::pair<bool, std::string> validate_sql(const std::string& sql) {
        if (sql.empty()) {
            return {false, "Generated SQL is empty"};
        }

        // Check if it starts with SELECT
        std::string sql_upper = sql;
        std::transform(sql_upper.begin(), sql_upper.end(), sql_upper.begin(), ::toupper);
        
        size_t first_non_space = sql_upper.find_first_not_of(" \t\n\r");
        if (first_non_space == std::string::npos || 
            sql_upper.substr(first_non_space, 6) != "SELECT") {
            return {false, "SQL must start with SELECT"};
        }

        // Check for semicolons (prevents multi-statement injection)
        if (sql.find(';') != std::string::npos) {
            return {false, "Semicolons are not allowed"};
        }

        // Check for forbidden keywords
        for (const auto& keyword : FORBIDDEN_KEYWORDS) {
            std::regex keyword_regex("\\b" + keyword + "\\b", std::regex::icase);
            if (std::regex_search(sql, keyword_regex)) {
                return {false, "Forbidden keyword detected: " + keyword};
            }
        }

        // Check for LIMIT clause
        std::regex limit_regex("\\bLIMIT\\s+\\d+", std::regex::icase);
        if (!std::regex_search(sql, limit_regex)) {
            return {false, "SQL must include a LIMIT clause"};
        }

        return {true, "Valid"};
    }

    ai::Client client_;
    bool initialized_;
    std::unique_ptr<ai_worker::PgCache> cache_;
};

int main() {
    std::cout << "AI SQL Generator Worker Starting..." << std::endl;
    std::cout << "====================================" << std::endl;

    // Initialize AI Worker
    AIWorker worker;
    if (!worker.initialize()) {
        std::cerr << "Failed to initialize AI Worker" << std::endl;
        return 1;
    }

    // Create HTTP server
    httplib::Server server;

    // Health check endpoint
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        json response;
        response["status"] = "healthy";
        response["service"] = "ai_worker";
        response["version"] = "1.0.0";
        res.set_content(response.dump(), "application/json");
    });

    // Cache statistics endpoint
    server.Get("/cache/stats", [&worker](const httplib::Request&, httplib::Response& res) {
        json response = worker.get_cache_stats();
        res.set_content(response.dump(), "application/json");
    });

    // Cache cleanup endpoint
    server.Post("/cache/cleanup", [&worker](const httplib::Request&, httplib::Response& res) {
        int deleted = worker.cleanup_cache();
        json response;
        response["deleted_entries"] = deleted;
        response["message"] = "Cleanup completed";
        res.set_content(response.dump(), "application/json");
    });

    // Main generation endpoint
    server.Post("/generate", [&worker](const httplib::Request& req, httplib::Response& res) {
        json response;

        try {
            // Parse request
            json request_body = json::parse(req.body);

            // Validate required fields
            if (!request_body.contains("prompt")) {
                response["error"] = "Missing required field: prompt";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }

            std::string prompt = request_body["prompt"];
            std::string mode = request_body.value("mode", "sql_only");
            std::string schema_version = request_body.value("schema_version", "v1");

            // Log request
            std::cout << "Received request - Prompt: " << prompt.substr(0, 50) 
                      << (prompt.length() > 50 ? "..." : "") << std::endl;

            // Generate SQL
            response = worker.generate_sql(prompt, mode, schema_version);

            // Set appropriate status code
            if (response.contains("error")) {
                res.status = response.contains("expected_version") ? 409 : 500;
            } else {
                res.status = 200;
            }

            res.set_content(response.dump(), "application/json");

        } catch (const json::parse_error& e) {
            response["error"] = "Invalid JSON in request body";
            response["details"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            response["error"] = "Internal server error";
            response["details"] = e.what();
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Start server
    std::cout << "Starting HTTP server on " << LISTEN_HOST << ":" << LISTEN_PORT << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET  /health        - Health check" << std::endl;
    std::cout << "  POST /generate      - Generate SQL from prompt" << std::endl;
    std::cout << "  GET  /cache/stats   - Cache statistics" << std::endl;
    std::cout << "  POST /cache/cleanup - Clean up expired cache" << std::endl;
    std::cout << std::endl;

    if (!server.listen(LISTEN_HOST, LISTEN_PORT)) {
        std::cerr << "Failed to start server on " << LISTEN_HOST << ":" << LISTEN_PORT << std::endl;
        return 1;
    }

    return 0;
}
