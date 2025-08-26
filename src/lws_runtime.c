/*
 * LWS runtime
 *
 * Copyright (C) 2025 Andre Naef
 */


#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <lws_runtime.h>
#include <lws_interface.h>
#include <lws_request.h>
#include <lws_state.h>


static void lws_handle_sigterm(int sig);
static void lws_getenv_str(const char *name, lws_str_t *value);
static int lws_getenv_int(const char *name, lws_int_t *value);
static int lws_getenv_size(const char *name, size_t *value);
static int lws_getenv_flag(const char *name, int *value);


static volatile sig_atomic_t keep_running = 1;
static const char *const lws_log_levels[] = {
	"EMERG", "ALERT", "CRIT", "ERR", "WARN", "NOTICE", "INFO", "DEBUG"
};


/*
 * main
 */

static void lws_handle_sigterm (int sig) {
	(void)lws_cancel_poll();
	keep_running = 0;
}

static void lws_getenv_str (const char *name, lws_str_t *value) {
	char   *s;

	s = getenv(name);
	if (!s || *s == '\0') {
		lws_str_null(value);
	} else {
		value->len = strlen(s);;
		value->data = s;
	}
}

static int lws_getenv_int (const char *name, lws_int_t *value) {
	char  *s, *end;
	long   v;

	s = getenv(name);
	if (!s || *s == '\0') {
		*value = 0;
		return 0;
	}
	errno = 0;
	v = strtol(s, &end, 10);
	if (errno != 0 || *end != '\0') {
		return -1;
	}
	*value = (lws_int_t)v;
	return 0;
}

static int lws_getenv_size (const char *name, size_t *value) {
	char               *s, *end;
	size_t              mult;
	unsigned long       base, result;

	s = getenv(name);
	if (!s || *s == '\0') {
		*value = 0;
		return 0;
	}
	errno = 0;
	base = strtoul(s, &end, 10);
	if (errno != 0 || end == s) {
		return -1;
	}
	if (*end != '\0') {
		if (end[1] != '\0') {
			return -1;
		}
		switch (*end) {
			case 'k':
				mult = 1024UL;
				break;

			case 'm':
				mult = 1024UL * 1024UL;
				break;

			default:
				return -1;
		}
	} else {
		mult = 1UL;
	}
	if (base > SIZE_MAX / mult) {
		return -1;
	}
	result = base * mult;
	*value = (size_t)result;
	return 0;
}

static int lws_getenv_flag (const char *name, int *value) {
	char  *s;
	
	s = getenv(name);
	if (!s || *s == '\0') {
		*value = 0;
		return 0;
	}
	if (s[0] == 'o' && s[1] == 'n' && s[2] == '\0') {
		*value = 1;
	} else if (s[0] == 'o' && s[1] == 'f' && s[2] == 'f' && s[3] == '\0') {
		*value = 0;
	} else {
		return -1;
	}
	return 0;
}

void lws_log (lws_log_level_e level, const char *fmt, ...) {
	char       tbuf[21];
	time_t     now;
	va_list    ap;
	struct tm  tm;

	if (level < LWS_LOG_EMERG) {
		level = LWS_LOG_EMERG;
	} else if (level > LWS_LOG_DEBUG) {
		level = LWS_LOG_DEBUG;
	}
	now = time(NULL);
	gmtime_r(&now, &tm);
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", &tm);
	flockfile(stdout);
	fprintf(stdout, "%s [%s] (LWS) ", tbuf, lws_log_levels[level]);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	fflush(stdout);
	funlockfile(stdout);
}

int main (int argc, char *argv[]) {
	int        rc, rc_request;
	lws_ctx_t  ctx;
	lws_str_t  match;

	/* log start */
	lws_log_debug("runtime starting pid:%d", getpid());

	/* assume success */
	rc = EXIT_SUCCESS;

	/* setup signal handler */
	signal(SIGTERM, lws_handle_sigterm);

	/* init context */
	lws_memzero(&ctx, sizeof(ctx));
	ctx.content_length = -1;

	/* get runtime API */
	lws_getenv_str("AWS_LAMBDA_RUNTIME_API", &ctx.runtime_api);
	if (!ctx.runtime_api.len) {
		lws_log(LWS_LOG_EMERG, "AWS_LAMBDA_RUNTIME_API not set");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_log_debug("runtime API url:%.*s", (int)ctx.runtime_api.len, ctx.runtime_api.data);

	/* get task root */
	lws_getenv_str("LAMBDA_TASK_ROOT", &ctx.task_root);
	if (!ctx.task_root.len) {
		lws_log(LWS_LOG_EMERG, "LAMBDA_TASK_ROOT not set");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_log_debug("task root dir:%.*s", (int)ctx.task_root.len, ctx.task_root.data);

	/* initialize libcurl */
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
		lws_log(LWS_LOG_EMERG, "failed to initialize libcurl");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	ctx.curl_global_init = 1;
	ctx.curl = curl_easy_init();
	if (!ctx.curl) {
		lws_log(LWS_LOG_EMERG, "failed to create libcurl easy handle");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	ctx.curlm = curl_multi_init();
	if (!ctx.curlm) {
		lws_log(LWS_LOG_EMERG, "failed to create libcurl multi handle");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}

	/* get LWS configuration */
	lws_getenv_str("LWS_MATCH", &match);
	if (match.len) {
		if (regcomp(&ctx.match, match.data, REG_EXTENDED) != 0) {
			lws_post_error(&ctx, "failed to compile LWS_MATCH regex");
			rc = EXIT_FAILURE;
			goto global_cleanup;
		}
	}
	lws_getenv_str("LWS_MAIN", &ctx.main);
	if (!ctx.main.len) {
		lws_post_error(&ctx, "LWS_MAIN not set");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_getenv_str("LWS_PATH_INFO", &ctx.path_info);
	lws_getenv_str("LWS_INIT", &ctx.init);
	lws_getenv_str("LWS_PRE", &ctx.pre);
	lws_getenv_str("LWS_POST", &ctx.post);
	if (lws_getenv_flag("LWS_RAW", &ctx.raw) != 0) {
		lws_post_error(&ctx, "bad LWS_RAW value");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	if (lws_getenv_size("LWS_GC", &ctx.state_gc) != 0) {
		lws_post_error(&ctx, "bad LWS_GC value");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	if (lws_getenv_int("LWS_REQ_MAX", &ctx.state_req_max) != 0) {
		lws_post_error(&ctx, "bad LWS_REQ_MAX value");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	if (lws_getenv_flag("LWS_DIAGNOSTIC", &ctx.state_diagnostic) != 0) {
		lws_post_error(&ctx, "bad LWS_DIAGNOSTIC value");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}

	/* initailize stat cache */
	ctx.stat_cache = lws_table_create(32);
	if (!ctx.stat_cache) {
		lws_post_error(&ctx, "failed to create stat cache");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_table_set_dup(ctx.stat_cache, 1);
	lws_table_set_cap(ctx.stat_cache, LWS_STAT_CACHE_CAP);

	/* initialize the header tables */
	ctx.req_headers = lws_table_create(32);
	if (!ctx.req_headers) {
		lws_post_error(&ctx, "failed to create request headers table");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_table_set_free(ctx.req_headers, 1);
	lws_table_set_ci(ctx.req_headers, 1);
	ctx.resp_headers = lws_table_create(32);
	if (!ctx.resp_headers) {
		lws_post_error(&ctx, "failed to create response headers table");
		rc = EXIT_FAILURE;
		goto global_cleanup;
	}
	lws_table_set_dup(ctx.resp_headers, 1);
	lws_table_set_free(ctx.resp_headers, 1);
	lws_table_set_ci(ctx.resp_headers, 1);

	/* request main loop */
	while (keep_running && rc == EXIT_SUCCESS) {
		/* get invocation */
		if (lws_get_next_invocation(&ctx) != 0) {
			if (keep_running) {
				if (ctx.request_id[0] != '\0') {
					/* we got a request ID and can inform the runtime */
					if (lws_post_error(&ctx, "failed to get next invocation") != 0) {
						rc = EXIT_FAILURE;
					}
				} else {
					/* no request ID -> cannot inform the runtime -> exit */
					rc = EXIT_FAILURE;
				}
			} else {
				lws_log(LWS_LOG_NOTICE, "received term signal");
			}
			goto request_cleanup;
		}

		/* handle request */
		if ((rc_request = lws_handle_request(&ctx)) != 0) {
			/* error */
			if (rc_request < 0) {
				/* failure */
				if (lws_post_error(&ctx, "failed to handle request") != 0) {
					rc = EXIT_FAILURE;
				}
				goto request_cleanup;
			} else {
				/* http status code */
				if (!ctx.streaming) {
					/* generate error response */
					if (rc_request < 100 || rc_request >= 600) {
						lws_log(LWS_LOG_ERR, "invalid status code:%d", rc_request);
						rc_request = 500;  /* internal server error */
					}
					if (lws_error_response(&ctx, rc_request) != 0) {
						if (lws_post_error(&ctx, "failed to generate error response") != 0) {
							rc = EXIT_FAILURE;
						}
						goto request_cleanup;
					}
				} else {
					lws_log(LWS_LOG_ERR, "ignoring HTTP status code after streaming response");
				}
			}
		}

		/* send response, unless a streaming response was sent */
		if (!ctx.streaming) {
			if (lws_post_response(&ctx) != 0) {
				if (lws_post_error(&ctx, "failed to post response") != 0) {
					rc = EXIT_FAILURE;
				}
				goto request_cleanup;
			}
		}

		/* request cleanup */
		request_cleanup:

		/* Lambda request cleanup */
		ctx.header_len = 0;
		ctx.request_id[0] = '\0';
		ctx.content_length = -1;
		if (ctx.body.data) {
			lws_free(ctx.body.data);
			ctx.body.data = NULL;
			ctx.body.len = 0;
			ctx.body_cap = 0;
		}
		if (ctx.doc) {
			yyjson_doc_free(ctx.doc);
			ctx.doc = NULL;
		}
		ctx.got_trace = 0;
		ctx.got_deadline = 0;

		/* payload request cleanup */
		lws_str_null(&ctx.req_method);
		lws_str_null(&ctx.req_path);
		lws_str_null(&ctx.req_args);
		lws_str_null(&ctx.req_ip);
		if (ctx.req_main.data) {
			lws_free(ctx.req_main.data);
			lws_str_null(&ctx.req_main);
		}
		if (ctx.req_path_info.data) {
			lws_free(ctx.req_path_info.data);
			lws_str_null(&ctx.req_path_info);
		}
		lws_table_clear(ctx.req_headers);
		if (ctx.req_body_file) {
			if (fclose(ctx.req_body_file) != 0) {
				lws_log(LWS_LOG_ERR, "failed to close request body file");
			}
			ctx.req_body_file = NULL;
		}

		/* payload response cleanup */
		lws_table_clear(ctx.resp_headers);
		if (ctx.resp_body_file) {
			if (fclose(ctx.resp_body_file) != 0) {
				lws_log(LWS_LOG_ERR, "failed to close response body file");
			}
			ctx.resp_body_file = NULL;
		}
		if (ctx.resp_body.data) {
			lws_free(ctx.resp_body.data);
			lws_str_null(&ctx.resp_body);
			ctx.resp_body_pos = 0;
			ctx.resp_body_cap = 0;
		}
		if (ctx.diagnostic.data) {
			lws_free(ctx.diagnostic.data);
			lws_str_null(&ctx.diagnostic);
		}
		if (ctx.streaming_prelude.data) {
			lws_free(ctx.streaming_prelude.data);
			lws_str_null(&ctx.streaming_prelude);
			ctx.streaming_prelude_pos = 0;
		}
		ctx.likely_utf8 = 0;
		ctx.streaming = 0;
		ctx.streaming_paused = 0;
		ctx.streaming_eof = 0;
		ctx.streaming_separator = 0;
	}

	/* global cleanup */
	global_cleanup:

	/* cleanup libcurl */
	if (ctx.curl) {
		curl_easy_cleanup(ctx.curl);
	}
	if (ctx.curlm) {
		curl_multi_cleanup(ctx.curlm);
	}
	if (ctx.curl_global_init) {
		curl_global_cleanup();
	}

	/* cleanup configuration */
	if (ctx.match.re_nsub > 0) {
		regfree(&ctx.match);
	}

	/* cleanup state */
	if (ctx.stat_cache) {
		lws_table_free(ctx.stat_cache);
	}
	if (ctx.L) {
		lws_close_state(&ctx);
	}

	/* cleanup payload request */
	if (ctx.req_headers) {
		lws_table_free(ctx.req_headers);
	}

	/* cleanup payload response */
	if (ctx.resp_headers) {
		lws_table_free(ctx.resp_headers);
	}

	return rc;
}
