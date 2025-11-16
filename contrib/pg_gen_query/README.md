Usage
-
- Set provider API key (GUC) in your psql session and call `pg_gen_query` with a natural language prompt.

```sql
SET pg_gen_query.gemini_api_key = 'YOUR_GEMINI_API_KEY';
SELECT pg_gen_query('number of events from Texas');
```

Features
-
- Schema-aware prompts: includes a concise list of non-system tables and columns in the prompt.
- Provider support: Gemini (primary) with optional fallbacks to OpenAI/Anthropic.
- Auto-execute read-only: executes returned SQL only when it begins with `SELECT`.
- Preview-only: returns generated SQL without executing when not a `SELECT`.

Build & Install
-
- From `contrib/pg_gen_query`:

```fish
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/ai-sdk-cpp/build/install
cmake --build . --config Release
```

- Copy the built library into your Postgres extension directory (example Homebrew path):

```fish
cp build/pg_gen_query.so /opt/homebrew/lib/postgresql@14/
```

- Then in `psql` create/load the extension and set GUCs as needed.

Examples (exact commands and results)

Run the setup and insert the sample rows (API key masked):

```fish
psql -d postgres -c "SET client_min_messages = 'log'; SET pg_gen_query.gemini_model = 'gemini-2.0-flash'; SET pg_gen_query.gemini_api_key = 'YOUR_GEMINI_API_KEY'; DROP TABLE IF EXISTS events; CREATE TABLE events(id serial primary key, name text, state text); TRUNCATE events; INSERT INTO events(name,state) VALUES('Concert','Texas'),('Meetup','Texas'),('Conference','California'),('Webinar','Texas'),('Hackathon','New York');"
```

Run each example query (these are the exact commands used during testing) and the expected outputs shown after each:

```sql
-- 1) Count Texas events
SELECT pg_gen_query('number of events from Texas');
-- Generated SQL (example):
-- SELECT COUNT(*) AS count FROM events WHERE state = 'Texas';
-- Result (example):
-- count
-- ------
-- 3

-- 2) List events in California
SELECT pg_gen_query('list events in California');
-- Generated SQL (example):
-- SELECT * FROM events WHERE state = 'California';
-- Result (example):
-- id | name       | state
-- ---+------------+-----------
-- 3  | Conference | California

-- 3) Names of events in New York
SELECT pg_gen_query('return the names of events in New York');
-- Generated SQL (example):
-- SELECT name FROM events WHERE state = 'New York';
-- Result (example):
-- name
-- -------
-- Hackathon
```



