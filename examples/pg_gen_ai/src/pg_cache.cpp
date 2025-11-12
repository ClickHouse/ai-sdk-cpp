/**
 * PostgreSQL Cache Implementation for AI Worker
 */

#include "pg_cache.h"
#include <libpq-fe.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>

namespace ai_worker {

// Helper to convert bytes to hex string
static std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

PgCache::PgCache(const Config& config)
    : config_(config), conn_(nullptr) {}

PgCache::~PgCache() {
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

PgCache::PgCache(PgCache&& other) noexcept
    : config_(std::move(other.config_)),
      conn_(other.conn_),
      last_error_(std::move(other.last_error_)) {
    other.conn_ = nullptr;
}

PgCache& PgCache::operator=(PgCache&& other) noexcept {
    if (this != &other) {
        if (conn_) {
            PQfinish(conn_);
        }
        config_ = std::move(other.config_);
        conn_ = other.conn_;
        last_error_ = std::move(other.last_error_);
        other.conn_ = nullptr;
    }
    return *this;
}

bool PgCache::initialize() {
    // Build connection string
    std::ostringstream conn_str;
    conn_str << "host=" << config_.host
             << " port=" << config_.port
             << " dbname=" << config_.dbname
             << " user=" << config_.user;
    
    if (!config_.password.empty()) {
        conn_str << " password=" << config_.password;
    }
    
    conn_str << " connect_timeout=10";

    // Connect to database
    conn_ = PQconnectdb(conn_str.str().c_str());
    
    if (PQstatus(conn_) != CONNECTION_OK) {
        last_error_ = std::string("Connection failed: ") + PQerrorMessage(conn_);
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }

    std::cout << "Cache: Connected to PostgreSQL at " 
              << config_.host << ":" << config_.port << std::endl;

    // Create schema if needed
    if (config_.auto_create_schema) {
        if (!create_schema()) {
            return false;
        }
    }

    return true;
}

bool PgCache::is_connected() const {
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

std::string PgCache::generate_cache_key(
    const std::string& prompt,
    const std::string& model,
    const std::string& schema_version
) const {
    // Concatenate inputs
    std::string input = prompt + "|" + model + "|" + schema_version;
    
    // Compute SHA256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), 
           input.length(), hash);
    
    return bytes_to_hex(hash, SHA256_DIGEST_LENGTH);
}

std::optional<CachedResult> PgCache::get(
    const std::string& prompt,
    const std::string& model,
    const std::string& schema_version
) {
    if (!is_connected()) {
        last_error_ = "Not connected to database";
        return std::nullopt;
    }

    std::string cache_key = generate_cache_key(prompt, model, schema_version);

    // Query with expiry check and hit count update
    const char* query = 
        "UPDATE ai_worker_cache "
        "SET hit_count = hit_count + 1, last_accessed_at = NOW() "
        "WHERE cache_key = $1 AND expires_at > NOW() "
        "RETURNING generated_sql, validated, tokens_used, hit_count, "
        "         created_at, expires_at";

    const char* params[1] = { cache_key.c_str() };
    
    PGresult* res = PQexecParams(
        conn_,
        query,
        1,           // number of parameters
        nullptr,     // parameter types (NULL = infer)
        params,      // parameter values
        nullptr,     // parameter lengths (NULL = text)
        nullptr,     // parameter formats (NULL = text)
        0            // result format (0 = text)
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        last_error_ = std::string("Cache get failed: ") + PQerrorMessage(conn_);
        PQclear(res);
        return std::nullopt;
    }

    if (PQntuples(res) == 0) {
        // Cache miss
        PQclear(res);
        return std::nullopt;
    }

    // Cache hit - parse result
    CachedResult result;
    result.sql = PQgetvalue(res, 0, 0);
    result.validated = std::string(PQgetvalue(res, 0, 1)) == "t";
    result.tokens_used = std::stoi(PQgetvalue(res, 0, 2));
    result.hit_count = std::stoi(PQgetvalue(res, 0, 3));
    
    // Note: Parsing timestamps would require more complex logic
    // For now, we'll just mark them as current time (not critical for cache hit)
    result.created_at = std::chrono::system_clock::now();
    result.expires_at = std::chrono::system_clock::now() + config_.default_ttl;

    PQclear(res);
    
    std::cout << "Cache: HIT for prompt '" << prompt.substr(0, 50) << "...' "
              << "(hits: " << result.hit_count << ")" << std::endl;
    
    return result;
}

bool PgCache::put(
    const std::string& prompt,
    const std::string& model,
    const std::string& schema_version,
    const std::string& sql,
    bool validated,
    int tokens_used,
    std::optional<std::chrono::seconds> ttl
) {
    if (!is_connected()) {
        last_error_ = "Not connected to database";
        return false;
    }

    std::string cache_key = generate_cache_key(prompt, model, schema_version);
    auto ttl_seconds = ttl.value_or(config_.default_ttl).count();

    // Upsert query (INSERT ... ON CONFLICT UPDATE)
    const char* query =
        "INSERT INTO ai_worker_cache "
        "(cache_key, prompt_text, model, schema_version, generated_sql, "
        " validated, tokens_used, expires_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, NOW() + ($8 || ' seconds')::INTERVAL) "
        "ON CONFLICT (cache_key) DO UPDATE SET "
        "  generated_sql = EXCLUDED.generated_sql, "
        "  validated = EXCLUDED.validated, "
        "  tokens_used = EXCLUDED.tokens_used, "
        "  expires_at = EXCLUDED.expires_at, "
        "  last_accessed_at = NOW()";

    std::string validated_str = validated ? "true" : "false";
    std::string tokens_str = std::to_string(tokens_used);
    std::string ttl_str = std::to_string(ttl_seconds);

    const char* params[8] = {
        cache_key.c_str(),
        prompt.c_str(),
        model.c_str(),
        schema_version.c_str(),
        sql.c_str(),
        validated_str.c_str(),
        tokens_str.c_str(),
        ttl_str.c_str()
    };

    PGresult* res = PQexecParams(
        conn_,
        query,
        8,
        nullptr,
        params,
        nullptr,
        nullptr,
        0
    );

    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    
    if (!success) {
        last_error_ = std::string("Cache put failed: ") + PQerrorMessage(conn_);
    } else {
        std::cout << "Cache: STORED prompt '" << prompt.substr(0, 50) << "...' "
                  << "(TTL: " << ttl_seconds << "s)" << std::endl;
    }

    PQclear(res);
    return success;
}

int PgCache::cleanup_expired() {
    if (!is_connected()) {
        return 0;
    }

    const char* query = "SELECT ai_worker_cleanup_cache()";
    PGresult* res = PQexec(conn_, query);

    int deleted = 0;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        deleted = std::stoi(PQgetvalue(res, 0, 0));
        if (deleted > 0) {
            std::cout << "Cache: Cleaned up " << deleted << " expired entries" << std::endl;
        }
    }

    PQclear(res);
    return deleted;
}

int PgCache::invalidate_schema(const std::string& schema_version) {
    if (!is_connected()) {
        return 0;
    }

    const char* query = "DELETE FROM ai_worker_cache WHERE schema_version = $1";
    const char* params[1] = { schema_version.c_str() };

    PGresult* res = PQexecParams(conn_, query, 1, nullptr, params, 
                                 nullptr, nullptr, 0);

    int deleted = 0;
    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        const char* rows = PQcmdTuples(res);
        if (rows && *rows) {
            deleted = std::stoi(rows);
        }
        std::cout << "Cache: Invalidated " << deleted 
                  << " entries for schema " << schema_version << std::endl;
    }

    PQclear(res);
    return deleted;
}

int PgCache::clear_all() {
    if (!is_connected()) {
        return 0;
    }

    const char* query = "TRUNCATE TABLE ai_worker_cache";
    PGresult* res = PQexec(conn_, query);

    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);

    if (success) {
        std::cout << "Cache: Cleared all entries" << std::endl;
    }

    return success ? 1 : 0;
}

json PgCache::get_stats() {
    json stats;

    if (!is_connected()) {
        stats["error"] = "Not connected";
        return stats;
    }

    const char* query = "SELECT * FROM ai_worker_cache_stats";
    PGresult* res = PQexec(conn_, query);

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        stats["total_entries"] = std::stoi(PQgetvalue(res, 0, 0));
        stats["valid_entries"] = std::stoi(PQgetvalue(res, 0, 1));
        stats["expired_entries"] = std::stoi(PQgetvalue(res, 0, 2));
        stats["total_hits"] = PQgetvalue(res, 0, 3);
        stats["avg_hits_per_entry"] = PQgetvalue(res, 0, 4);
        stats["total_tokens_saved"] = PQgetvalue(res, 0, 5);
        stats["max_hits"] = PQgetvalue(res, 0, 6);
        stats["oldest_entry"] = PQgetvalue(res, 0, 7);
        stats["newest_entry"] = PQgetvalue(res, 0, 8);
    }

    PQclear(res);
    return stats;
}

bool PgCache::create_schema() {
    // Read and execute cache_schema.sql
    // For simplicity, we'll inline the essential CREATE TABLE statement
    const char* schema_sql = R"(
        CREATE TABLE IF NOT EXISTS ai_worker_cache (
            cache_key VARCHAR(64) PRIMARY KEY,
            prompt_text TEXT NOT NULL,
            model VARCHAR(100) NOT NULL,
            schema_version VARCHAR(20) NOT NULL,
            generated_sql TEXT NOT NULL,
            validated BOOLEAN NOT NULL DEFAULT true,
            tokens_used INTEGER NOT NULL DEFAULT 0,
            created_at TIMESTAMP NOT NULL DEFAULT NOW(),
            expires_at TIMESTAMP NOT NULL,
            last_accessed_at TIMESTAMP NOT NULL DEFAULT NOW(),
            hit_count INTEGER NOT NULL DEFAULT 0,
            CONSTRAINT valid_expiry CHECK (expires_at > created_at)
        );

        CREATE INDEX IF NOT EXISTS idx_ai_cache_expires 
            ON ai_worker_cache(expires_at);

        CREATE INDEX IF NOT EXISTS idx_ai_cache_hits 
            ON ai_worker_cache(hit_count DESC);

        CREATE INDEX IF NOT EXISTS idx_ai_cache_schema 
            ON ai_worker_cache(schema_version);

        CREATE OR REPLACE FUNCTION ai_worker_cleanup_cache()
        RETURNS INTEGER
        LANGUAGE plpgsql
        AS $$
        DECLARE
            deleted_count INTEGER;
        BEGIN
            DELETE FROM ai_worker_cache WHERE expires_at < NOW();
            GET DIAGNOSTICS deleted_count = ROW_COUNT;
            RETURN deleted_count;
        END;
        $$;

        CREATE OR REPLACE VIEW ai_worker_cache_stats AS
        SELECT 
            COUNT(*) as total_entries,
            COUNT(*) FILTER (WHERE expires_at > NOW()) as valid_entries,
            COUNT(*) FILTER (WHERE expires_at <= NOW()) as expired_entries,
            SUM(hit_count) as total_hits,
            AVG(hit_count) as avg_hits_per_entry,
            SUM(tokens_used) as total_tokens_saved,
            MAX(hit_count) as max_hits,
            MIN(created_at) as oldest_entry,
            MAX(created_at) as newest_entry
        FROM ai_worker_cache;
    )";

    PGresult* res = PQexec(conn_, schema_sql);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;

    if (!success) {
        last_error_ = std::string("Schema creation failed: ") + PQerrorMessage(conn_);
        std::cerr << "Cache: " << last_error_ << std::endl;
    } else {
        std::cout << "Cache: Schema initialized successfully" << std::endl;
    }

    PQclear(res);
    return success;
}

}  // namespace ai_worker
