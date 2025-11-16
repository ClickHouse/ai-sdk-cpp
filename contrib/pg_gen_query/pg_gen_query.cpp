#ifdef __cplusplus
extern "C" {
#endif
#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <executor/spi.h>
#include <access/htup_details.h>
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
#ifdef __cplusplus
}
#endif

#include <ai/ai.h>

// C++ headers must be included outside extern "C"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

/* GUCs to allow configuring API keys from Postgres instead of process env */
static char *pg_gen_query_gemini_api_key = NULL;
static char *pg_gen_query_openai_api_key = NULL;
static char *pg_gen_query_anthropic_api_key = NULL;
static char *pg_gen_query_gemini_model = NULL;

extern "C" void _PG_init(void);

extern "C" PGDLLEXPORT Datum pg_gen_query(PG_FUNCTION_ARGS);
extern "C" PGDLLEXPORT Datum pg_ai_provider(PG_FUNCTION_ARGS);

extern "C" {
PG_FUNCTION_INFO_V1(pg_gen_query);
PG_FUNCTION_INFO_V1(pg_ai_provider);
}

Datum pg_gen_query(PG_FUNCTION_ARGS) {
  text* prompt_text = PG_GETARG_TEXT_PP(0);
  char* prompt_cstr = text_to_cstring(prompt_text);

  // Gather DB schema information (tables and columns) via SPI.
  auto get_schema_description = [&]() -> std::string {
    std::ostringstream out;
    if (SPI_connect() != SPI_OK_CONNECT) return "";
    const char* q = "SELECT table_schema, table_name, column_name, data_type FROM information_schema.columns WHERE table_schema NOT IN ('pg_catalog','information_schema') ORDER BY table_schema, table_name, ordinal_position";
    int rc = SPI_execute(q, true, 0);
    if (rc == SPI_OK_SELECT && SPI_processed > 0) {
      TupleDesc tup = SPI_tuptable->tupdesc;
      SPITupleTable *tuptable = SPI_tuptable;
      std::string last_table;
      for (uint64_t i = 0; i < (uint64_t)SPI_processed; ++i) {
        HeapTuple ht = tuptable->vals[i];
        char* schema = SPI_getvalue(ht, tup, 1);
        char* tbl = SPI_getvalue(ht, tup, 2);
        char* col = SPI_getvalue(ht, tup, 3);
        char* dt = SPI_getvalue(ht, tup, 4);
        if (!schema || !tbl || !col || !dt) continue;
        std::string table_full = std::string(schema) + "." + std::string(tbl);
        if (table_full != last_table) {
          out << table_full << "\n";
          last_table = table_full;
        }
        out << "  - " << col << " : " << dt << "\n";
      }
    }
    SPI_finish();
    return out.str();
  };

  std::string schema_desc = get_schema_description();

  std::string user_prompt = std::string("Database schema:\n") + schema_desc + "\nTranslate the following natural language into a single PostgreSQL SELECT query. Return only the SQL (no explanation) and use PostgreSQL syntax (single quotes for string literals):\n" + std::string(prompt_cstr);
  // Prefer keys supplied via Postgres GUCs, fall back to environment-based
  // try_create_client() for each provider.
  ai::Client client;
  try {
    ereport(LOG, (errmsg("pg_gen_query GUCs: gemini_key='%s' openai_key='%s' anthropic_key='%s' gemini_model='%s'",
                        pg_gen_query_gemini_api_key ? pg_gen_query_gemini_api_key : "(null)",
                        pg_gen_query_openai_api_key ? pg_gen_query_openai_api_key : "(null)",
                        pg_gen_query_anthropic_api_key ? pg_gen_query_anthropic_api_key : "(null)",
                        pg_gen_query_gemini_model ? pg_gen_query_gemini_model : "(null)")));

    if (pg_gen_query_gemini_api_key && pg_gen_query_gemini_api_key[0] != '\0') {
      ereport(LOG, (errmsg("pg_gen_query: calling gemini::create_client with key length=%d", (int)strlen(pg_gen_query_gemini_api_key))));
      client = ai::gemini::create_client(std::string(pg_gen_query_gemini_api_key));
      ereport(LOG, (errmsg("pg_gen_query: returned from gemini::create_client, client.is_valid()=%d", client.is_valid())));
    } else {
      ereport(LOG, (errmsg("pg_gen_query: calling gemini::try_create_client()")));
      auto opt_gemini = ai::gemini::try_create_client();
      if (opt_gemini.has_value()) client = std::move(*opt_gemini);
      ereport(LOG, (errmsg("pg_gen_query: returned from try_create_client, client.is_valid()=%d", client.is_valid())));
    }

    ereport(LOG, (errmsg("After gemini attempt: client valid=%d provider='%s' default_model='%s' config='%s'",
              client.is_valid(),
              client.provider_name().c_str(),
              client.default_model().c_str(),
              client.config_info().c_str())));

    if (!client.is_valid()) {
      if (pg_gen_query_openai_api_key && pg_gen_query_openai_api_key[0] != '\0') {
        client = ai::openai::create_client(std::string(pg_gen_query_openai_api_key));
      } else {
        auto opt_openai = ai::openai::try_create_client();
        if (opt_openai.has_value()) client = std::move(*opt_openai);
      }
    }

    if (!client.is_valid()) {
      if (pg_gen_query_anthropic_api_key && pg_gen_query_anthropic_api_key[0] != '\0') {
        client = ai::anthropic::create_client(std::string(pg_gen_query_anthropic_api_key));
      } else {
        auto opt_anthropic = ai::anthropic::try_create_client();
        if (opt_anthropic.has_value()) client = std::move(*opt_anthropic);
      }
    }
  } catch (const std::exception &e) {
    ereport(ERROR, (errmsg("AI client configuration error: %s", e.what())));
    PG_RETURN_NULL();
  }

  if (!client.is_valid()) {
    ereport(ERROR, (errmsg("No AI provider configured. Set GEMINI_API_KEY or OPENAI_API_KEY or ANTHROPIC_API_KEY.")));
    PG_RETURN_NULL();
  }

  ai::GenerateOptions options;
  options.model = client.default_model();
  // Allow overriding model via GUC only
  if (pg_gen_query_gemini_model && pg_gen_query_gemini_model[0] != '\0') {
    options.model = std::string(pg_gen_query_gemini_model);
  }
  if (options.model.empty()) options.model = ai::openai::models::kDefaultModel;
  options.prompt = user_prompt;

  auto result = client.generate_text(options);

  if (!result) {
    std::string err = result.error_message();
    if (err.empty()) err = "unknown error";
    ereport(LOG, (errmsg("pg_gen_query: AI generation raw error: %s", err.c_str())));
    ereport(ERROR, (errmsg("AI generation failed: %s", err.c_str())));
    PG_RETURN_NULL();
  }

  std::string generated = result.text;

  // Strip code fences if present (simple match of ```...```)
  std::regex fence_re(R"(```(?:sql)?\n([\s\S]*?)\n```)" , std::regex::icase);
  std::smatch m;
  if (std::regex_search(generated, m, fence_re) && m.size() > 1) {
    generated = m[1].str();
  }

  // Basic sanitization: prefer single quotes for string literals
  try {
    std::regex string_eq_re(R"((=)\s*\"([^\"]*)\")");
    generated = std::regex_replace(generated, string_eq_re, "$1 '$2'");
  } catch (...) {
    // Ignore regex issues and use generated as-is
  }

  // Trim leading whitespace
  auto ltrim = [](std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
  };
  auto rtrim = [](std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
  };
  ltrim(generated);
  rtrim(generated);

  // Only execute if the model returned a SELECT statement (read-only)
  std::string generated_upper = generated;
  std::transform(generated_upper.begin(), generated_upper.end(), generated_upper.begin(), ::toupper);
  if (generated_upper.rfind("SELECT", 0) == 0) {
    // Execute as read-only via SPI
    if (SPI_connect() != SPI_OK_CONNECT) {
      ereport(ERROR, (errmsg("pg_gen_query: SPI_connect failed")));
      PG_RETURN_NULL();
    }
    int rc = SPI_execute(generated.c_str(), true, 0);
    if (rc != SPI_OK_SELECT && rc != SPI_OK_SELINTO) {
      SPI_finish();
      ereport(ERROR, (errmsg("pg_gen_query: SQL execution failed (rc=%d)", rc)));
      PG_RETURN_NULL();
    }
    // Build a textual representation of the result
    std::ostringstream outbuf;
    if (SPI_processed > 0 && SPI_tuptable) {
      TupleDesc tup = SPI_tuptable->tupdesc;
      SPITupleTable *tuptable = SPI_tuptable;
      // Header
      for (int col = 1; col <= tup->natts; ++col) {
        if (col > 1) outbuf << '\t';
        outbuf << NameStr(tup->attrs[col-1].attname);
      }
      outbuf << '\n';
      for (uint64_t i = 0; i < (uint64_t)SPI_processed; ++i) {
        HeapTuple ht = tuptable->vals[i];
        for (int col = 1; col <= tup->natts; ++col) {
          if (col > 1) outbuf << '\t';
          char *val = SPI_getvalue(ht, tup, col);
          outbuf << (val ? val : "\\N");
        }
        outbuf << '\n';
      }
    }
    SPI_finish();
    std::string final_out = outbuf.str();
    text* out = cstring_to_text(final_out.c_str());
    PG_RETURN_POINTER(out);
  }

  // If not a SELECT, don't execute; return the SQL string for inspection
  text* out = cstring_to_text(generated.c_str());
  PG_RETURN_POINTER(out);
}

Datum pg_ai_provider(PG_FUNCTION_ARGS) {
  ai::Client client;
  try {
    if (pg_gen_query_gemini_api_key && pg_gen_query_gemini_api_key[0] != '\0') {
      client = ai::gemini::create_client(std::string(pg_gen_query_gemini_api_key));
    } else {
      auto opt_gemini = ai::gemini::try_create_client();
      if (opt_gemini.has_value()) client = std::move(*opt_gemini);
    }

    if (!client.is_valid()) {
      if (pg_gen_query_openai_api_key && pg_gen_query_openai_api_key[0] != '\0') {
        client = ai::openai::create_client(std::string(pg_gen_query_openai_api_key));
      } else {
        auto opt_openai = ai::openai::try_create_client();
        if (opt_openai.has_value()) client = std::move(*opt_openai);
      }
    }

    if (!client.is_valid()) {
      if (pg_gen_query_anthropic_api_key && pg_gen_query_anthropic_api_key[0] != '\0') {
        client = ai::anthropic::create_client(std::string(pg_gen_query_anthropic_api_key));
      } else {
        auto opt_anthropic = ai::anthropic::try_create_client();
        if (opt_anthropic.has_value()) client = std::move(*opt_anthropic);
      }
    }
  } catch (const std::exception &e) {
    ereport(ERROR, (errmsg("AI client configuration error: %s", e.what())));
    PG_RETURN_NULL();
  }

  std::string provider = client.is_valid() ? client.provider_name() : std::string("none");
  text* out = cstring_to_text(provider.c_str());
  PG_RETURN_POINTER(out);
}

/* Initialize GUC variables */
extern "C" void _PG_init(void) {
  DefineCustomStringVariable("pg_gen_query.gemini_api_key",
                             "Gemini API key for pg_gen_query",
                             NULL,
                             &pg_gen_query_gemini_api_key,
                             "",
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);

  DefineCustomStringVariable("pg_gen_query.openai_api_key",
                             "OpenAI API key for pg_gen_query",
                             NULL,
                             &pg_gen_query_openai_api_key,
                             "",
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);

  DefineCustomStringVariable("pg_gen_query.gemini_model",
                             "Default Gemini model for pg_gen_query",
                             NULL,
                             &pg_gen_query_gemini_model,
                             "gemini-2.0-flash",
                             PGC_USERSET,
                             0,
                             NULL,
                             NULL,
                             NULL);

  DefineCustomStringVariable("pg_gen_query.anthropic_api_key",
                             "Anthropic API key for pg_gen_query",
                             NULL,
                             &pg_gen_query_anthropic_api_key,
                             "",
                             PGC_SUSET,
                             0,
                             NULL,
                             NULL,
                             NULL);
}
