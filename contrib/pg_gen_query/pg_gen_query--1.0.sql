CREATE FUNCTION pg_gen_query(text) RETURNS text
  AS '$libdir/pg_gen_query', 'pg_gen_query'
  LANGUAGE c STRICT;

CREATE FUNCTION pg_ai_provider() RETURNS text
  AS '$libdir/pg_gen_query', 'pg_ai_provider'
  LANGUAGE c STRICT;
