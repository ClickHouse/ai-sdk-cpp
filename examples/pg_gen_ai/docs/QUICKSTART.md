# AI SQL Generator Worker - Quick Start Guide

## ✅ What You Have Now

You've successfully built a complete AI-powered SQL generation system with:

1. **C++ HTTP Worker** (`ai_worker_debug`) - Running on port 8088
2. **PostgreSQL Function** (`ai_gen_via_worker.sql`) - Ready to install
3. **Systemd Service** (`ai_worker.service`) - For production deployment
4. **Deployment Scripts** (`setup.sh`) - Automated installation

## 🚀 Quick Test (Local)

### 1. Start the Worker (Already Running!)

```bash
cd /Users/anupsharma/Documents/gen_postgres_updated/ai-sdk-cpp/build/pg_gen_ai
export OPENAI_API_KEY=your_key_here
./ai_worker_debug
```

### 2. Test the Health Endpoint

```bash
curl http://127.0.0.1:8088/health
```

**Response:**
```json
{
    "status": "healthy",
    "service": "ai_worker",
    "version": "1.0.0"
}
```

### 3. Generate SQL Queries

```bash
# Simple query
curl -X POST http://127.0.0.1:8088/generate \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Get all users","mode":"sql_only","schema_version":"v1"}'

# Complex query with aggregation
curl -X POST http://127.0.0.1:8088/generate \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Count orders by status in the last 7 days","mode":"sql_only","schema_version":"v1"}'
```

**Example Response:**
```json
{
    "sql": "SELECT * FROM users LIMIT 1000",
    "validated": true,
    "tokens_used": 123,
    "schema_version": "v1"
}
```

## 📊 Test Results

✅ **Health Check** - Working  
✅ **SQL Generation** - Working  
✅ **Validation** - SELECT only, LIMIT enforced  
✅ **Security** - Forbidden keywords blocked  

### Example Queries Generated:

1. **Simple SELECT:**
   ```sql
   SELECT * FROM users LIMIT 1000
   ```

2. **Ordered Query:**
   ```sql
   SELECT * FROM orders ORDER BY total_amount DESC LIMIT 10
   ```

3. **Aggregation with Date Filter:**
   ```sql
   SELECT registration_date, COUNT(*) AS user_count 
   FROM users 
   WHERE registration_date >= NOW() - INTERVAL '30 days' 
   GROUP BY registration_date 
   LIMIT 1000
   ```

## 🗄️ PostgreSQL Integration

### Install the plpython3u Function

```bash
# Connect to your database
psql -U postgres -d your_database

-- Enable plpython3u
CREATE EXTENSION IF NOT EXISTS plpython3u;

-- Install the cache schema
\i /path/to/ai-sdk-cpp/examples/pg_gen_ai/sql/cache_schema.sql

-- Install the function
\i /path/to/ai-sdk-cpp/examples/pg_gen_ai/sql/ai_gen_via_worker.sql

-- Grant permissions
GRANT EXECUTE ON FUNCTION public.ai_gen_via_worker(text, boolean, integer) TO your_user;
```

### Use from PostgreSQL

```sql
-- Optional: Set search_path to avoid 'public.' prefix
SET search_path TO public, "$user";

-- Generate SQL only (don't execute)
SELECT ai_gen_via_worker('Get all active users');

-- Generate and execute
SELECT ai_gen_via_worker('Show me the latest 10 orders', true);

-- With custom timeout
SELECT ai_gen_via_worker('Complex query', true, 30);
```

**Tip:** To permanently avoid the `public.` prefix, run:
```sql
ALTER DATABASE your_database SET search_path TO public, "$user";
```

## 🔧 Production Deployment (systemd)

### Manual Setup Steps

```bash
# 1. Create user
sudo useradd --system --no-create-home --shell /bin/false aiworker

# 2. Create directories
sudo mkdir -p /etc/ai_worker /var/lib/ai_worker /var/log/ai_worker

# 3. Copy API key
echo "your-openai-api-key" | sudo tee /etc/ai_worker/openai_key
sudo chmod 640 /etc/ai_worker/openai_key
sudo chown root:aiworker /etc/ai_worker/openai_key

# 4. Install binary
sudo cp build/pg_gen_ai/ai_worker_debug /usr/local/bin/ai_worker
sudo chmod 755 /usr/local/bin/ai_worker

# 5. Install service
sudo cp examples/pg_gen_ai/systemd/ai_worker.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable ai_worker
sudo systemctl start ai_worker

# 6. Check status
sudo systemctl status ai_worker
```

## 📝 Service Management

```bash
# Start service
sudo systemctl start ai_worker

# Stop service
sudo systemctl stop ai_worker

# Restart service
sudo systemctl restart ai_worker

# Check status
sudo systemctl status ai_worker

# View logs
sudo journalctl -u ai_worker -f

# View last 100 lines
sudo journalctl -u ai_worker -n 100
```

## 🔒 Security Features

### Multiple Layers of Protection

1. **Network Security**
   - Listens on `127.0.0.1` only (localhost)
   - Not accessible from external network
   - No authentication needed (localhost-only)

2. **SQL Injection Prevention**
   - Only SELECT statements allowed
   - Blocks DDL/DML: INSERT, UPDATE, DELETE, DROP, etc.
   - No semicolons (prevents multi-statement)
   - Enforces LIMIT clause (max 1000 rows)

3. **System Security (systemd)**
   - Runs as unprivileged `aiworker` user
   - `ProtectSystem=full` - Read-only system directories
   - `ProtectHome=true` - No access to home directories
   - `PrivateTmp=true` - Private temp directory
   - `NoNewPrivileges=true` - Cannot escalate privileges

## 🧪 Testing the Complete Flow

### 1. Worker is Running (Local or systemd)

```bash
curl http://127.0.0.1:8088/health
```

### 2. PostgreSQL Function is Installed

```sql
-- Test in psql (set search_path first)
SET search_path TO public, "$user";
SELECT ai_gen_via_worker('Get all users');
```

### 3. End-to-End Test

```sql
-- In PostgreSQL
SET search_path TO public, "$user";

SELECT ai_gen_via_worker(
    'Show me users who registered in the last 7 days, ordered by registration date',
    true  -- Execute the query
);
```

**Expected Result:**
```json
{
  "sql": "SELECT * FROM users WHERE registration_date >= NOW() - INTERVAL '7 days' ORDER BY registration_date DESC LIMIT 1000",
  "executed": true,
  "row_count": 42,
  "rows": [...],
  "tokens_used": 145
}
```

## 📊 Monitoring

### Check Worker Health

```bash
# Health endpoint
curl http://127.0.0.1:8088/health

# Service status
sudo systemctl status ai_worker

# Resource usage
sudo systemctl status ai_worker | grep -E "(Memory|CPU)"
```

### View Logs

```bash
# Real-time logs
sudo journalctl -u ai_worker -f

# Errors only
sudo journalctl -u ai_worker -p err

# Today's logs
sudo journalctl -u ai_worker --since today

# Count requests
sudo journalctl -u ai_worker | grep "Received request" | wc -l
```

## 🐛 Troubleshooting

### Worker Won't Start

```bash
# Check if port is in use
sudo lsof -i :8088

# Check API key
cat /etc/ai_worker/openai_key  # or ./openai_key for local

# Check logs
sudo journalctl -u ai_worker -n 50
```

### PostgreSQL Function Errors

```sql
-- Enable verbose logging
SET client_min_messages TO NOTICE;

-- Set search_path
SET search_path TO public, "$user";

-- Test the function
SELECT ai_gen_via_worker('test query');

-- Check if worker is accessible
SELECT ai_gen_via_worker('simple test');
```

### Common Issues

1. **"Failed to read OpenAI API key"**
   - Set `OPENAI_API_KEY` environment variable, or
   - Create `./openai_key` file (local), or
   - Create `/etc/ai_worker/openai_key` (systemd)

2. **"Connection refused"**
   - Worker not running: `sudo systemctl start ai_worker`
   - Wrong port: Check it's running on 8088

3. **"Schema version mismatch"**
   - Update `SCHEMA_VERSION` in SQL function to match worker
   - Default is "v1"

## 📚 Next Steps

1. **Customize System Prompt** - Edit `ai_worker.cpp` line 90-99
2. **Adjust Validation Rules** - Modify `FORBIDDEN_KEYWORDS` in `ai_worker.cpp`
3. **Add More Endpoints** - Extend the HTTP server
4. **Monitor Token Usage** - Track OpenAI API costs
5. **Set Up Alerts** - Monitor service health

## 📖 Documentation

- **Full Deployment Guide**: `deployment/README.md`
- **PostgreSQL Function**: `sql/ai_gen_via_worker.sql`
- **Worker Source**: `ai_worker.cpp`
- **Service File**: `systemd/ai_worker.service`

## 🎉 Success!

Your AI SQL Generator Worker is now:
- ✅ Built and compiled
- ✅ Tested and working
- ✅ Ready for PostgreSQL integration
- ✅ Ready for production deployment

Enjoy generating SQL with AI! 🚀
