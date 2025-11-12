# PostgreSQL AI SQL Generator

> **AI-powered SQL generation for PostgreSQL** — Generate, validate, and execute SQL queries using natural language prompts with OpenAI integration.

Built with [ai-sdk-cpp](https://github.com/ClickHouse/ai-sdk-cpp) — A modern C++20 toolkit for AI-powered applications.

---

## 🏗️ Architecture

```
┌─────────────────┐
│   PostgreSQL    │
│                 │
│ ai_gen_via_     │
│   worker()      │◄────┐
└────────┬────────┘     │
         │              │
         │ HTTP         │ Results
         │              │
         ▼              │
┌─────────────────┐     │
│   AI Worker     │─────┘
│   (C++ Server)  │
│                 │
│ • Validation    │◄────┐
│ • Cache Layer   │     │
│ • OpenAI API    │     │ Cache
└─────────────────┘     │
         │              │
         │              ▼
         │      ┌──────────────┐
         │      │  PostgreSQL  │
         │      │    Cache     │
         │      │   Tables     │
         │      └──────────────┘
         │
         ▼
┌─────────────────┐
│   OpenAI API    │
│   (gpt-4o-mini) │
└─────────────────┘
```

### Components

1. **PostgreSQL Function** (`sql/ai_gen_via_worker.sql`)
   - `plpython3u` function that accepts natural language prompts
   - Validates and optionally executes generated SQL
   - Returns results as JSON

2. **AI Worker** (`src/ai_worker.cpp`)
   - C++ HTTP server (port 8088)
   - OpenAI integration via ai-sdk-cpp
   - SQL validation (SELECT-only, no DDL)
   - PostgreSQL-backed caching layer

3. **Cache Module** (`src/pg_cache.cpp`)
   - Reduces OpenAI API calls
   - SHA256-based cache keys
   - Automatic expiry (1-hour TTL)
   - Hit tracking and statistics

---

## 🚀 Quick Start

### 1. Build the Worker

```bash
cd /path/to/ai-sdk-cpp
uv run scripts/build.py
```

The binary will be at: `build/pg_gen_ai/ai_worker_debug`

### 2. Set Up Database Schema

```bash
psql postgres < examples/pg_gen_ai/sql/cache_schema.sql
psql postgres < examples/pg_gen_ai/sql/ai_gen_via_worker.sql
```

### 3. Configure Environment

```bash
# Required: OpenAI API key
export OPENAI_API_KEY="sk-..."

# Optional: PostgreSQL cache configuration
# If not set, defaults from src/pg_cache.h will be used
export PG_CACHE_HOST=""              # Empty for Unix socket (default: "")
export PG_CACHE_PORT="5432"          # PostgreSQL port (default: 5432)
export PG_CACHE_USER="your_user"     # Your PostgreSQL username (default: anupsharma)
export PG_CACHE_DBNAME="postgres"    # Database name (default: postgres)
export PG_CACHE_PASSWORD=""          # Password if needed (default: empty)
```

**Note:** The cache will work with default settings if you can connect to PostgreSQL using `psql postgres` without additional parameters. If the cache fails to connect, the worker will continue without caching (graceful degradation).

### 4. Run the Worker

```bash
./build/pg_gen_ai/ai_worker_debug
```

### 5. Generate SQL

```sql
-- Simple query generation
SELECT ai_gen_via_worker('Show me all users created in the last 7 days');

-- Generate and execute
SELECT ai_gen_via_worker('Count active users by country', true);
```

---

## 📊 Example Queries

### Generate SQL (no execution)

```sql
SELECT ai_gen_via_worker(
    'Find top 10 products by revenue in Q4 2024'
);
```

**Returns:**
```json
{
  "sql": "SELECT product_id, product_name, SUM(revenue) as total_revenue FROM sales WHERE sale_date >= '2024-10-01' AND sale_date < '2025-01-01' GROUP BY product_id, product_name ORDER BY total_revenue DESC LIMIT 10",
  "validated": true,
  "cached": false,
  "tokens": 145
}
```

### Generate and Execute

```sql
SELECT ai_gen_via_worker(
    'Show me users who signed up today',
    true  -- execute the query
);
```

**Returns:**
```json
{
  "sql": "SELECT id, email, created_at FROM users WHERE created_at::date = CURRENT_DATE LIMIT 1000",
  "validated": true,
  "cached": true,
  "tokens": 0,
  "rows": [
    {"id": 123, "email": "user@example.com", "created_at": "2025-11-12T10:30:00Z"}
  ]
}
```

---

## 📚 Documentation

- **[GETTING_STARTED.md](GETTING_STARTED.md)** — 5-minute setup guide (start here!)
- **[QUICKSTART.md](docs/QUICKSTART.md)** — Detailed setup and deployment guide
- **[CACHING.md](docs/CACHING.md)** — Cache architecture, configuration, and monitoring

---

## 🗂️ Project Structure

```
pg_gen_ai/
├── src/
│   ├── ai_worker.cpp      # HTTP server + OpenAI integration
│   ├── pg_cache.cpp       # PostgreSQL cache implementation
│   └── pg_cache.h         # Cache interface
├── sql/
│   ├── cache_schema.sql   # Cache tables DDL
│   └── ai_gen_via_worker.sql  # PostgreSQL function
├── systemd/
│   └── ai_worker.service  # Systemd service file
├── docs/
│   ├── QUICKSTART.md      # Setup guide
│   └── CACHING.md         # Cache documentation
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

---

## 🔒 Security Features

- **SQL Validation**: Only SELECT queries allowed, DDL blocked
- **Automatic LIMIT**: Adds `LIMIT 1000` if missing
- **No Multi-Statement**: Semicolons rejected
- **Read-Only Execution**: Runs with restricted permissions
- **Local-Only Server**: Binds to 127.0.0.1 (no external access)

---

## 🎯 Use Cases

- **Data Exploration**: "Show me sales trends for the last quarter"
- **Ad-hoc Reporting**: "Count users by signup month"
- **Schema Discovery**: "What tables contain customer data?"
- **Query Assistance**: "Find duplicate orders in the orders table"

---

## 📦 Dependencies

- **[ai-sdk-cpp](https://github.com/ClickHouse/ai-sdk-cpp)** — AI SDK for C++20 (Apache 2.0 License)
- **cpp-httplib** — HTTP server (included in ai-sdk-cpp)
- **libpq** — PostgreSQL C client
- **OpenSSL** — HTTPS support
- **nlohmann/json** — JSON parsing (included in ai-sdk-cpp)

---

## 📄 License

This project is built on top of [ai-sdk-cpp](https://github.com/ClickHouse/ai-sdk-cpp) by ClickHouse, licensed under the **Apache License 2.0**.

See the [ai-sdk-cpp LICENSE](https://github.com/ClickHouse/ai-sdk-cpp/blob/main/LICENSE) for details.

---

## 🤝 Contributing

This is a standalone example project within the ai-sdk-cpp repository. For issues or contributions:

1. **ai-sdk-cpp core**: [ClickHouse/ai-sdk-cpp](https://github.com/ClickHouse/ai-sdk-cpp)
2. **This project**: Submit issues/PRs to the main ai-sdk-cpp repo with `[pg_gen_ai]` prefix

---

## 🌟 Credits

- **[ClickHouse](https://github.com/ClickHouse)** — For the excellent ai-sdk-cpp library
- **OpenAI** — For the GPT models
- **PostgreSQL Community** — For the robust database system

---

**Ready to generate SQL with AI?** See [QUICKSTART.md](docs/QUICKSTART.md) to get started! 🚀
