CREATE OR REPLACE FUNCTION pg_gen_query(text) RETURNS text
  AS '/full/path/pg_gen_query.so', 'pg_gen_query'
  LANGUAGE c STRICT;

CREATE OR REPLACE FUNCTION pg_ai_provider() RETURNS text
  AS '/full/path/pg_gen_query.so', 'pg_ai_provider'
  LANGUAGE c STRICT;
