-- AI SQL Generator via Worker - plpython3u Function
-- 
-- This function sends prompts to the local AI worker service
-- and optionally executes the generated SQL.
--
-- Prerequisites:
-- 1. PostgreSQL with plpython3u extension
-- 2. ai_worker service running on localhost:8088
--
-- Installation:
--   CREATE EXTENSION IF NOT EXISTS plpython3u;
--   \i ai_gen_via_worker.sql
--
--   -- Optional: Set search_path to avoid using 'public.' prefix
--   ALTER DATABASE your_database SET search_path TO public, "$user";
--   -- Or for current session only:
--   SET search_path TO public, "$user";
--
-- Usage:
--   -- Generate SQL only (don't execute)
--   SELECT ai_gen_via_worker('Get all users from the users table');
--
--   -- Generate and execute SQL
--   SELECT ai_gen_via_worker('Get all users', true);
--
--   -- With custom timeout
--   SELECT ai_gen_via_worker('Complex query', true, 30);

CREATE OR REPLACE FUNCTION public.ai_gen_via_worker(
  prompt_text text,
  execute_query boolean DEFAULT false,   -- if true, execute the generated SQL and return rows as JSON
  timeout_seconds integer DEFAULT 10     -- HTTP request timeout to local worker
) RETURNS text
LANGUAGE plpython3u
AS $$
import json
import re
import hashlib
import urllib.request
import urllib.error
import urllib.parse

# Configuration (keep in sync with your worker)
WORKER_URL = "http://127.0.0.1:8088/generate"
SCHEMA_VERSION = "v1"   # bump when DB schema changes; must match worker
MAX_EXECUTE_ROWS = 1000  # safety guard

def _sha256_hex(s):
    """Generate SHA256 hash of a string"""
    return hashlib.sha256(s.encode('utf-8')).hexdigest()

def _http_post_json(url, payload, timeout):
    """Send HTTP POST request with JSON payload"""
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'})
    
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            resp_body = resp.read().decode('utf-8')
            return resp_body
    except urllib.error.HTTPError as e:
        error_body = e.read().decode('utf-8') if e.fp else str(e)
        raise Exception("HTTPError: %s - %s" % (e.code, error_body))
    except urllib.error.URLError as e:
        raise Exception("URLError: %s" % (e.reason))
    except Exception as e:
        raise Exception("Request failed: %s" % str(e))

# SQL Validators
_forbidden_keywords = [
    "INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER", "TRUNCATE",
    "GRANT", "REVOKE", "SET", "BEGIN", "COMMIT", "ROLLBACK", "COPY", 
    "EXECUTE", "DECLARE", "PREPARE"
]
_forbidden_regex = re.compile(r"\b(" + "|".join(_forbidden_keywords) + r")\b", re.IGNORECASE)

def _starts_with_select(sql):
    """Check if SQL starts with SELECT"""
    return re.match(r"^\s*SELECT\b", sql, re.IGNORECASE) is not None

def _has_semicolon(sql):
    """Check for semicolons (prevents multi-statement injection)"""
    return ";" in sql

def _contains_forbidden(sql):
    """Check for forbidden keywords"""
    return _forbidden_regex.search(sql) is not None

def _append_limit_if_missing(sql, limit=1000):
    """Append LIMIT clause if missing"""
    if re.search(r"\bLIMIT\b", sql, re.IGNORECASE):
        return sql
    # Add LIMIT at the end
    return sql.strip() + " LIMIT %d" % limit

def _validate_sql(sql):
    """
    Validate generated SQL for safety
    Returns: (is_valid, error_message)
    """
    if not sql or not sql.strip():
        return (False, "Generated SQL is empty")
    
    if not _starts_with_select(sql):
        return (False, "SQL must start with SELECT")
    
    if _has_semicolon(sql):
        return (False, "Semicolons are not allowed (prevents injection)")
    
    if _contains_forbidden(sql):
        return (False, "SQL contains forbidden keywords")
    
    return (True, "Valid")

# Main logic
try:
    # Log the request
    plpy.notice("AI Worker Request - Prompt: %s" % prompt_text[:100])
    
    # Build request payload
    request_payload = {
        "prompt": prompt_text,
        "mode": "sql_only",
        "schema_version": SCHEMA_VERSION
    }
    
    # Send request to worker
    plpy.notice("Sending request to AI worker at %s" % WORKER_URL)
    response_body = _http_post_json(WORKER_URL, request_payload, timeout_seconds)
    
    # Parse response
    response = json.loads(response_body)
    
    # Check for errors from worker
    if "error" in response:
        error_msg = "AI Worker Error: %s" % response["error"]
        if "expected_version" in response:
            error_msg += " (Expected schema version: %s, got: %s)" % (
                response["expected_version"], 
                response.get("received_version", "unknown")
            )
        plpy.error(error_msg)
        return None
    
    # Extract generated SQL
    generated_sql = response.get("sql", "")
    validated = response.get("validated", False)
    
    if not validated or not generated_sql:
        plpy.error("Worker returned invalid response: no validated SQL")
        return None
    
    plpy.notice("Generated SQL: %s" % generated_sql)
    plpy.notice("Tokens used: %s" % response.get("tokens_used", "unknown"))
    
    # Additional validation in plpython (defense in depth)
    is_valid, validation_error = _validate_sql(generated_sql)
    if not is_valid:
        plpy.error("SQL validation failed: %s" % validation_error)
        return None
    
    # Ensure LIMIT clause exists
    generated_sql = _append_limit_if_missing(generated_sql, MAX_EXECUTE_ROWS)
    
    # If execute_query is False, just return the SQL
    if not execute_query:
        result = {
            "sql": generated_sql,
            "executed": False,
            "tokens_used": response.get("tokens_used", 0)
        }
        return json.dumps(result, indent=2)
    
    # Execute the query
    plpy.notice("Executing generated SQL...")
    try:
        # Execute with plpy (runs as the calling user's permissions)
        rows = plpy.execute(generated_sql, MAX_EXECUTE_ROWS)
        
        # Convert rows to list of dicts
        result_rows = []
        for row in rows:
            result_rows.append(dict(row))
        
        # Build response
        result = {
            "sql": generated_sql,
            "executed": True,
            "row_count": len(result_rows),
            "rows": result_rows,
            "tokens_used": response.get("tokens_used", 0)
        }
        
        plpy.notice("Execution successful - returned %d rows" % len(result_rows))
        return json.dumps(result, indent=2)
        
    except Exception as exec_error:
        plpy.error("SQL execution failed: %s" % str(exec_error))
        return None

except Exception as e:
    plpy.error("Function error: %s" % str(e))
    return None
$$;

-- Grant execute permission to appropriate roles
-- GRANT EXECUTE ON FUNCTION public.ai_gen_via_worker(text, boolean, integer) TO your_role;

-- Comments
COMMENT ON FUNCTION public.ai_gen_via_worker(text, boolean, integer) IS 
'Generate SQL using AI worker service. Optionally execute the generated query.
Parameters:
  - prompt_text: Natural language description of desired query
  - execute_query: If true, execute the generated SQL and return results
  - timeout_seconds: HTTP timeout for worker request (default 10)
Returns: JSON with generated SQL and optionally query results';

-- Example usage:
-- 
-- 1. Generate SQL only:
--    SELECT ai_gen_via_worker('Show me all users created in the last 7 days');
--
-- 2. Generate and execute:
--    SELECT ai_gen_via_worker('Get top 10 orders by amount', true);
--
-- 3. With custom timeout:
--    SELECT ai_gen_via_worker('Complex aggregation query', true, 30);
--
-- Note: If you get "function does not exist" error, either:
--   a) Use the full name: SELECT public.ai_gen_via_worker(...)
--   b) Set search_path: SET search_path TO public, "$user";

-- ============================================================================
-- OPTIONAL: Setup for easier usage (no 'public.' prefix needed)
-- ============================================================================

-- Option 1: Set search_path for current session
-- SET search_path TO public, "$user";

-- Option 2: Set search_path permanently for your database
-- ALTER DATABASE your_database_name SET search_path TO public, "$user";

-- Option 3: Set search_path for specific user
-- ALTER USER your_username SET search_path TO public, "$user";

-- After setting search_path, you can use:
-- SELECT ai_gen_via_worker('your prompt here');
-- Instead of:
-- SELECT public.ai_gen_via_worker('your prompt here');
