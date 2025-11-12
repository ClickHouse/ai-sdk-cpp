-- AI Worker Cache Schema
-- 
-- This table stores cached SQL generation results to reduce OpenAI API calls.
-- The worker manages cache policy (TTL, eviction, etc.)
--
-- Features:
-- - SHA256 hash-based lookup for fast retrieval
-- - Configurable TTL (time-to-live)
-- - Automatic expiry checking
-- - Hit count tracking for analytics
-- - Schema version for cache invalidation

CREATE TABLE IF NOT EXISTS ai_worker_cache (
    -- Primary key: hash of (prompt + model + schema_version)
    cache_key VARCHAR(64) PRIMARY KEY,
    
    -- Input parameters (for debugging/analytics)
    prompt_text TEXT NOT NULL,
    model VARCHAR(100) NOT NULL,
    schema_version VARCHAR(20) NOT NULL,
    
    -- Cached result
    generated_sql TEXT NOT NULL,
    validated BOOLEAN NOT NULL DEFAULT true,
    
    -- Metadata
    tokens_used INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP NOT NULL,
    last_accessed_at TIMESTAMP NOT NULL DEFAULT NOW(),
    hit_count INTEGER NOT NULL DEFAULT 0,
    
    -- Indexing for performance
    CONSTRAINT valid_expiry CHECK (expires_at > created_at)
);

-- Index for expiry-based cleanup
CREATE INDEX IF NOT EXISTS idx_ai_cache_expires 
    ON ai_worker_cache(expires_at);

-- Index for analytics (most popular queries)
CREATE INDEX IF NOT EXISTS idx_ai_cache_hits 
    ON ai_worker_cache(hit_count DESC);

-- Index for schema version (for bulk invalidation)
CREATE INDEX IF NOT EXISTS idx_ai_cache_schema 
    ON ai_worker_cache(schema_version);

-- Function to clean up expired cache entries
CREATE OR REPLACE FUNCTION ai_worker_cleanup_cache()
RETURNS INTEGER
LANGUAGE plpgsql
AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM ai_worker_cache
    WHERE expires_at < NOW();
    
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$;

COMMENT ON TABLE ai_worker_cache IS 
'Cache table for AI-generated SQL queries. Managed by ai_worker service.';

COMMENT ON COLUMN ai_worker_cache.cache_key IS 
'SHA256 hash of (prompt + model + schema_version) for deterministic lookup';

COMMENT ON COLUMN ai_worker_cache.expires_at IS 
'Expiration timestamp. Entries are invalid after this time.';

COMMENT ON COLUMN ai_worker_cache.hit_count IS 
'Number of times this cached entry has been used. Useful for analytics.';

COMMENT ON FUNCTION ai_worker_cleanup_cache() IS 
'Removes expired cache entries. Can be called manually or via cron job.';

-- Example: Set up automatic cleanup (optional, requires pg_cron extension)
-- SELECT cron.schedule('cleanup-ai-cache', '0 * * * *', 'SELECT ai_worker_cleanup_cache()');

-- Grant permissions (adjust as needed)
-- GRANT SELECT, INSERT, UPDATE, DELETE ON ai_worker_cache TO aiworker;
-- GRANT EXECUTE ON FUNCTION ai_worker_cleanup_cache() TO aiworker;

-- ============================================================================
-- Cache Statistics View (optional, for monitoring)
-- ============================================================================

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

COMMENT ON VIEW ai_worker_cache_stats IS 
'Statistics about cache usage and effectiveness';

-- Example queries:
--
-- 1. View cache statistics:
--    SELECT * FROM ai_worker_cache_stats;
--
2. Most popular cached queries:
   SELECT prompt_text, hit_count, tokens_used 
   FROM ai_worker_cache 
   WHERE expires_at > NOW()
   ORDER BY hit_count DESC 
   LIMIT 10;
--
-- 3. Clean up expired entries:
--    SELECT ai_worker_cleanup_cache();
--
-- 4. Invalidate cache for specific schema version:
--    DELETE FROM ai_worker_cache WHERE schema_version = 'v1';
--
-- 5. Clear all cache:
--    TRUNCATE TABLE ai_worker_cache;
