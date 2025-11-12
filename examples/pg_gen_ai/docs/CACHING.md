# AI Worker Caching System

## Overview

The AI Worker implements a PostgreSQL-backed caching system to reduce repetitive OpenAI API calls and improve response times.

### Key Features

- **Automatic Caching**: All validated SQL queries are automatically cached
- **TTL-based Expiry**: Configurable time-to-live (default: 1 hour)
- **Hit Count Tracking**: Monitor cache effectiveness
- **Schema Version Support**: Automatic cache invalidation on schema changes
- **Defensive Schema Creation**: Worker creates cache table on startup if needed
- **Zero Downtime**: Worker continues without cache if PostgreSQL is unavailable

## Architecture

```
┌─────────────┐
│   Request   │
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│  Check Cache    │◄──────┐
│  (SHA256 hash)  │       │
└────────┬────────┘       │
         │                │
    ┌────┴────┐           │
    │  Hit?   │           │
    └─┬────┬──┘           │
      │Yes │No            │
      │    │              │
      │    ▼              │
      │  ┌────────────┐   │
      │  │  OpenAI    │   │
      │  │  API Call  │   │
      │  └──────┬─────┘   │
      │         │         │
      │         ▼         │
      │   ┌──────────┐    │
      │   │ Validate │    │
      │   └────┬─────┘    │
      │        │          │
      │        ▼          │
      │   ┌──────────┐    │
      │   │  Store   │────┘
      │   │  Cache   │
      │   └──────────┘
      │
      ▼
┌─────────────┐
│   Response  │
└─────────────┘
```

## Configuration

### Environment Variables

```bash
# PostgreSQL connection (optional - defaults shown)
export POSTGRES_HOST=localhost
export POSTGRES_PORT=5432
export POSTGRES_DB=postgres
export POSTGRES_USER=postgres
export POSTGRES_PASSWORD=your_password

# Cache TTL is configured in code (default: 3600 seconds / 1 hour)
```

### Cache Configuration in Code

Edit `ai_worker.cpp` to customize cache behavior:

```cpp
cache_config.default_ttl = std::chrono::seconds(3600);  // 1 hour
cache_config.auto_create_schema = true;  // Create table on startup
```

## Database Schema

The cache uses a single table with automatic schema creation:

```sql
CREATE TABLE ai_worker_cache (
    cache_key VARCHAR(64) PRIMARY KEY,  -- SHA256 hash
    prompt_text TEXT NOT NULL,
    model VARCHAR(100) NOT NULL,
    schema_version VARCHAR(20) NOT NULL,
    generated_sql TEXT NOT NULL,
    validated BOOLEAN NOT NULL DEFAULT true,
    tokens_used INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP NOT NULL,
    last_accessed_at TIMESTAMP NOT NULL DEFAULT NOW(),
    hit_count INTEGER NOT NULL DEFAULT 0
);
```

### Indexes

- `idx_ai_cache_expires` - For efficient expiry cleanup
- `idx_ai_cache_hits` - For analytics queries
- `idx_ai_cache_schema` - For schema-based invalidation

## Cache Key Generation

Cache keys are generated using SHA256 hash of:
```
SHA256(prompt + "|" + model + "|" + schema_version)
```

This ensures:
- Deterministic lookups
- Same prompt = same cache key
- Different models = different cache entries
- Schema version changes invalidate cache

## API Endpoints

### 1. Generate SQL (with caching)

```bash
curl -X POST http://127.0.0.1:8088/generate \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Get all users","mode":"sql_only","schema_version":"v1"}'
```

**Response (Cache Miss)**:
```json
{
  "sql": "SELECT * FROM users LIMIT 1000",
  "validated": true,
  "tokens_used": 123,
  "schema_version": "v1",
  "cache_hit": false
}
```

**Response (Cache Hit)**:
```json
{
  "sql": "SELECT * FROM users LIMIT 1000",
  "validated": true,
  "tokens_used": 123,
  "schema_version": "v1",
  "cache_hit": true,
  "hit_count": 5
}
```

### 2. Cache Statistics

```bash
curl http://127.0.0.1:8088/cache/stats
```

**Response**:
```json
{
  "total_entries": 150,
  "valid_entries": 120,
  "expired_entries": 30,
  "total_hits": 450,
  "avg_hits_per_entry": 3.75,
  "total_tokens_saved": 54000,
  "max_hits": 25,
  "oldest_entry": "2025-11-12 10:00:00",
  "newest_entry": "2025-11-12 14:30:00"
}
```

### 3. Manual Cache Cleanup

```bash
curl -X POST http://127.0.0.1:8088/cache/cleanup
```

**Response**:
```json
{
  "deleted_entries": 30,
  "message": "Cleanup completed"
}
```

## Cache Management

### View Cache Contents

```sql
-- All cached queries
SELECT prompt_text, generated_sql, hit_count, expires_at
FROM ai_worker_cache
WHERE expires_at > NOW()
ORDER BY hit_count DESC;

-- Most popular queries
SELECT prompt_text, hit_count, tokens_used
FROM ai_worker_cache
WHERE expires_at > NOW()
ORDER BY hit_count DESC
LIMIT 10;

-- Cache statistics
SELECT * FROM ai_worker_cache_stats;
```

### Manual Cleanup

```sql
-- Clean up expired entries
SELECT ai_worker_cleanup_cache();

-- Invalidate specific schema version
DELETE FROM ai_worker_cache WHERE schema_version = 'v1';

-- Clear all cache
TRUNCATE TABLE ai_worker_cache;
```

### Automated Cleanup (Optional)

Using `pg_cron` extension:

```sql
-- Install pg_cron
CREATE EXTENSION IF NOT EXISTS pg_cron;

-- Schedule hourly cleanup
SELECT cron.schedule(
    'cleanup-ai-cache',
    '0 * * * *',  -- Every hour
    'SELECT ai_worker_cleanup_cache()'
);

-- View scheduled jobs
SELECT * FROM cron.job;
```

## Performance Benefits

### Token Savings Example

```
Scenario: 100 requests, 50% cache hit rate
- Cache misses: 50 requests × 150 tokens = 7,500 tokens
- Cache hits: 50 requests × 0 tokens = 0 tokens
- Total: 7,500 tokens (50% savings)
```

### Response Time Improvement

```
- OpenAI API call: ~1-3 seconds
- Cache hit: ~10-50 milliseconds
- Improvement: 20-300x faster
```

## Monitoring

### Key Metrics to Track

1. **Cache Hit Rate**: `(total_hits / total_requests) × 100%`
2. **Token Savings**: `total_tokens_saved`
3. **Average Hits per Entry**: Indicates query reuse
4. **Expired Entries**: Should be cleaned regularly

### Example Monitoring Query

```sql
WITH stats AS (
    SELECT 
        COUNT(*) as total_entries,
        SUM(hit_count) as total_hits,
        SUM(tokens_used * hit_count) as tokens_saved
    FROM ai_worker_cache
    WHERE expires_at > NOW()
)
SELECT 
    total_entries,
    total_hits,
    tokens_saved,
    ROUND(tokens_saved * 0.0001, 2) as estimated_savings_usd
FROM stats;
```

## Troubleshooting

### Cache Not Working

**Check 1: PostgreSQL Connection**
```bash
# Test connection
psql -h localhost -U postgres -d postgres -c "SELECT 1"
```

**Check 2: Worker Logs**
```bash
# Look for cache initialization messages
sudo journalctl -u ai_worker | grep -i cache
```

**Check 3: Table Exists**
```sql
SELECT EXISTS (
    SELECT FROM information_schema.tables 
    WHERE table_name = 'ai_worker_cache'
);
```

### Cache Not Hitting

**Verify Cache Key**:
- Same prompt text (including whitespace)
- Same model name
- Same schema version
- Entry not expired

**Debug Query**:
```sql
SELECT 
    cache_key,
    prompt_text,
    model,
    schema_version,
    expires_at,
    hit_count
FROM ai_worker_cache
WHERE prompt_text LIKE '%your search%'
ORDER BY created_at DESC;
```

### High Memory Usage

**Solution**: Reduce TTL or implement size limits

```sql
-- Delete entries with low hit counts
DELETE FROM ai_worker_cache 
WHERE hit_count < 2 AND created_at < NOW() - INTERVAL '1 day';

-- Keep only top N entries
DELETE FROM ai_worker_cache
WHERE cache_key NOT IN (
    SELECT cache_key FROM ai_worker_cache
    ORDER BY hit_count DESC
    LIMIT 1000
);
```

## Best Practices

### 1. Set Appropriate TTL

```cpp
// Short TTL for frequently changing data
cache_config.default_ttl = std::chrono::seconds(300);  // 5 minutes

// Long TTL for stable queries
cache_config.default_ttl = std::chrono::seconds(86400);  // 24 hours
```

### 2. Monitor Cache Effectiveness

```sql
-- Daily cache report
SELECT 
    DATE(created_at) as date,
    COUNT(*) as new_entries,
    SUM(hit_count) as total_hits,
    AVG(hit_count) as avg_hits
FROM ai_worker_cache
GROUP BY DATE(created_at)
ORDER BY date DESC
LIMIT 7;
```

### 3. Regular Cleanup

Set up automated cleanup or run manually:

```bash
# Via HTTP API
curl -X POST http://127.0.0.1:8088/cache/cleanup

# Via SQL
psql -c "SELECT ai_worker_cleanup_cache()"
```

### 4. Schema Version Management

When updating your database schema:

```sql
-- Invalidate old cache
DELETE FROM ai_worker_cache WHERE schema_version = 'v1';

-- Update worker to use new version
-- Edit ai_worker.cpp: CURRENT_SCHEMA_VERSION = "v2"
```

## Security Considerations

### Access Control

```sql
-- Grant minimal permissions
GRANT SELECT, INSERT, UPDATE, DELETE ON ai_worker_cache TO aiworker;
GRANT EXECUTE ON FUNCTION ai_worker_cleanup_cache() TO aiworker;

-- Revoke from others
REVOKE ALL ON ai_worker_cache FROM PUBLIC;
```

### Data Retention

```sql
-- Limit data retention
DELETE FROM ai_worker_cache 
WHERE created_at < NOW() - INTERVAL '30 days';
```

### Sensitive Data

If caching sensitive prompts, consider:
1. Encrypting `prompt_text` column
2. Shorter TTL for sensitive queries
3. Regular cache purging
4. Access logging

## Future Enhancements

Potential improvements:

1. **LRU Eviction**: Implement least-recently-used eviction
2. **Size Limits**: Max cache size in MB
3. **Warm-up**: Pre-populate cache with common queries
4. **Distributed Cache**: Redis/Memcached for multi-worker setups
5. **Cache Warming**: Background job to refresh popular queries

## Summary

The caching system provides:
- ✅ Automatic caching of all validated queries
- ✅ Significant cost savings (50%+ token reduction typical)
- ✅ Faster response times (20-300x improvement)
- ✅ Zero configuration required (works out of the box)
- ✅ Graceful degradation (continues without cache if PostgreSQL unavailable)
- ✅ Full observability (statistics and monitoring)

The cache is managed entirely by the worker - the plpython function remains simple and just calls the worker.
