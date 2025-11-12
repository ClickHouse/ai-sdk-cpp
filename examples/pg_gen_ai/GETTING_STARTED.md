# Getting Started with PostgreSQL AI SQL Generator

> **5-minute setup guide** — Get AI-powered SQL generation running in your PostgreSQL database.

---

## Prerequisites

- PostgreSQL 12+ with `plpython3u` extension
- OpenAI API key
- C++20 compiler (for building)

---

## Step 1: Build the Worker

```bash
cd /path/to/ai-sdk-cpp
uv run scripts/build.py
```

✅ Binary will be at: `build/pg_gen_ai/ai_worker_debug`

---

## Step 2: Set Up PostgreSQL

```bash
# Connect to your database
psql postgres

# Enable plpython3u extension
CREATE EXTENSION IF NOT EXISTS plpython3u;

# Install cache schema
\i examples/pg_gen_ai/sql/cache_schema.sql

# Install the AI function
\i examples/pg_gen_ai/sql/ai_gen_via_worker.sql

# Exit psql
\q
```

---

## Step 3: Configure Environment

```bash
# Required: OpenAI API key
export OPENAI_API_KEY="sk-proj-..."

# Optional: PostgreSQL cache configuration
# (Only needed if your PostgreSQL setup differs from defaults)
export PG_CACHE_HOST=""              # Empty for Unix socket
export PG_CACHE_PORT="5432"          # PostgreSQL port
export PG_CACHE_USER="your_user"     # Your PostgreSQL username
export PG_CACHE_DBNAME="postgres"    # Database name
export PG_CACHE_PASSWORD=""          # Password if needed
```

**Tip:** If you can run `psql postgres` without extra parameters, the defaults will work!

---

## Step 4: Start the Worker

```bash
cd build/pg_gen_ai
./ai_worker_debug
```

You should see:
```
🚀 AI Worker starting...
✓ Cache initialized successfully
✓ Server listening on http://127.0.0.1:8088
```

---

## Step 5: Test It!

Open a new terminal and connect to PostgreSQL:

```sql
-- Generate SQL (no execution)
SELECT ai_gen_via_worker('Show me all users');

-- Generate and execute
SELECT ai_gen_via_worker('Count users by country', true);

-- Complex query
SELECT ai_gen_via_worker(
    'Find top 10 products by revenue in the last 30 days',
    true
);
```

---

## 🎉 Success!

You now have AI-powered SQL generation running in PostgreSQL!

### Next Steps

- **[README.md](README.md)** — Learn about architecture and features
- **[docs/QUICKSTART.md](docs/QUICKSTART.md)** — Production deployment guide
- **[docs/CACHING.md](docs/CACHING.md)** — Optimize with caching

---

## Common Issues

### "Connection refused" error
- Make sure the worker is running: `./ai_worker_debug`
- Check it's listening on port 8088: `curl http://127.0.0.1:8088/health`

### "role does not exist" error
- Set `PG_CACHE_USER` to your PostgreSQL username:
  ```bash
  export PG_CACHE_USER="your_username"
  ```
- Or update `src/pg_cache.h` with your defaults

### Cache connection failed
If you see "Cache initialization failed", the worker will continue without caching. To fix:
```bash
# Check your PostgreSQL connection
psql postgres

# Set the correct environment variables
export PG_CACHE_HOST=""              # Empty for Unix socket
export PG_CACHE_USER="your_username"
export PG_CACHE_DBNAME="postgres"
```

### "plpython3u does not exist"
```sql
CREATE EXTENSION plpython3u;
```

### "OPENAI_API_KEY not set"
```bash
export OPENAI_API_KEY="sk-..."
```

---

## Quick Reference

| Command | Purpose |
|---------|---------|
| `./ai_worker_debug` | Start the worker |
| `curl http://127.0.0.1:8088/health` | Check worker status |
| `curl http://127.0.0.1:8088/cache/stats` | View cache statistics |
| `SELECT ai_gen_via_worker('prompt')` | Generate SQL |
| `SELECT ai_gen_via_worker('prompt', true)` | Generate and execute |

---

**Need help?** See the full documentation in [README.md](README.md)

