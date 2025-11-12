/**
 * PostgreSQL Cache for AI Worker
 * 
 * Provides caching functionality to reduce OpenAI API calls.
 * Uses PostgreSQL as the cache backend with automatic expiry.
 */

#pragma once

#include <string>
#include <optional>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

// Forward declare libpq types
struct pg_conn;
typedef struct pg_conn PGconn;
struct pg_result;
typedef struct pg_result PGresult;

namespace ai_worker {

using json = nlohmann::json;

/**
 * Cached SQL result
 */
struct CachedResult {
    std::string sql;
    bool validated;
    int tokens_used;
    int hit_count;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
};

/**
 * PostgreSQL-backed cache for AI-generated SQL
 * 
 * Features:
 * - Automatic expiry based on TTL
 * - Hit count tracking
 * - Schema version support
 * - Thread-safe operations
 */
class PgCache {
public:
    /**
     * Configuration for cache connection
     */
    struct Config {
        std::string host = "";
        int port = 5432;
        std::string dbname = "postgres";
        std::string user = "anupsharma";
        std::string password = "";
        std::chrono::seconds default_ttl{3600};  // 1 hour default
        bool auto_create_schema = true;
    };

    /**
     * Create cache instance with configuration
     */
    explicit PgCache(const Config& config);
    
    /**
     * Destructor - closes connection
     */
    ~PgCache();

    // Disable copy, allow move
    PgCache(const PgCache&) = delete;
    PgCache& operator=(const PgCache&) = delete;
    PgCache(PgCache&&) noexcept;
    PgCache& operator=(PgCache&&) noexcept;

    /**
     * Initialize connection and create schema if needed
     * @return true if successful
     */
    bool initialize();

    /**
     * Check if cache is connected and ready
     */
    bool is_connected() const;

    /**
     * Get cached result if available and not expired
     * @param prompt The user prompt
     * @param model The model name
     * @param schema_version The schema version
     * @return Cached result if found and valid, nullopt otherwise
     */
    std::optional<CachedResult> get(
        const std::string& prompt,
        const std::string& model,
        const std::string& schema_version
    );

    /**
     * Store result in cache
     * @param prompt The user prompt
     * @param model The model name
     * @param schema_version The schema version
     * @param sql The generated SQL
     * @param validated Whether the SQL passed validation
     * @param tokens_used Number of tokens used
     * @param ttl Time-to-live (optional, uses default if not specified)
     * @return true if successfully cached
     */
    bool put(
        const std::string& prompt,
        const std::string& model,
        const std::string& schema_version,
        const std::string& sql,
        bool validated,
        int tokens_used,
        std::optional<std::chrono::seconds> ttl = std::nullopt
    );

    /**
     * Clean up expired cache entries
     * @return Number of entries deleted
     */
    int cleanup_expired();

    /**
     * Invalidate all cache entries for a specific schema version
     * @param schema_version The schema version to invalidate
     * @return Number of entries deleted
     */
    int invalidate_schema(const std::string& schema_version);

    /**
     * Clear all cache entries
     * @return Number of entries deleted
     */
    int clear_all();

    /**
     * Get cache statistics
     * @return JSON object with cache stats
     */
    json get_stats();

    /**
     * Get last error message
     */
    std::string last_error() const { return last_error_; }

private:
    /**
     * Generate cache key from inputs
     */
    std::string generate_cache_key(
        const std::string& prompt,
        const std::string& model,
        const std::string& schema_version
    ) const;

    /**
     * Create cache schema if it doesn't exist
     */
    bool create_schema();

    /**
     * Execute SQL and return result
     */
    PGresult* execute_query(const std::string& query);

    /**
     * Execute parameterized query
     */
    PGresult* execute_params(
        const std::string& query,
        int n_params,
        const char* const* param_values
    );

    Config config_;
    PGconn* conn_;
    std::string last_error_;
};

}  // namespace ai_worker
