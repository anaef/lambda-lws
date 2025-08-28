/*
 * LWS log
 *
 * Copyright (C) 2025 Andre Naef
 */


#include <lws_log.h>
#include <lws_ngx.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>


#define LWS_LOG_LEVEL(lit)  { lit, sizeof(lit) - 1 }


lws_str_t lws_log_levels[] = {
	lws_string("EMERG"),
	lws_string("ALERT"),
	lws_string("CRIT"),
	lws_string("ERR"),
	lws_string("WARN"),
	lws_string("NOTICE"),
	lws_string("INFO"),
	lws_string("DEBUG"),
	lws_null_string
};
static lws_ctx_t *ctx = NULL;


void lws_log_setctx (lws_ctx_t *c) {
	ctx = c;
}

void lws_log (lws_log_level_e level, const char *fmt, ...) {
	int              res;;
	char             ts[20], tsm[25], msg[1024];
	size_t           tsm_len, msg_len;
	va_list          ap;
	struct tm        tm;
	struct timeval   tv;
	yyjson_mut_doc  *doc;
	yyjson_mut_val  *root;

	/* log? */
	if (!ctx || level > ctx->log_level) {
		return;
	}

	/* clip level */
	if (level < LWS_LOG_EMERG) {
		level = LWS_LOG_EMERG;
	} else if (level > LWS_LOG_DEBUG) {
		level = LWS_LOG_DEBUG;
	}

	/* get and format time */
	gettimeofday(&tv, NULL);
	gmtime_r(&tv.tv_sec, &tm);
	strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm);
	tsm_len = (size_t)snprintf(tsm, sizeof(tsm), "%s.%03ldZ", ts, tv.tv_usec / 1000);	

	/* format message */
	va_start(ap, fmt);
	res = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (res >= 0) {
		msg_len = (res < (int)sizeof(msg)) ? (size_t)res : sizeof(msg) - 1;
	} else {
		res = snprintf(msg, sizeof(msg), "log formatting error fmt:%s", fmt);
		msg_len = (res < (int)sizeof(msg)) ? (size_t)res : sizeof(msg) - 1;
	}

	/* handle JSON and text mode */
	doc = NULL;
	if (!ctx->log_text) {
		/* JSON mode */
		doc = yyjson_mut_doc_new(NULL);
		if (!doc) {
			goto textmode;
		}
		root = yyjson_mut_obj(doc);
		if (!root) {
			yyjson_mut_doc_free(doc);
			goto textmode;
		}
		yyjson_mut_doc_set_root(doc, root);
		if (!yyjson_mut_obj_add_strn(doc, root, "ts", tsm, strlen(tsm))) {
			yyjson_mut_doc_free(doc);
			goto textmode;
		}
		if (!yyjson_mut_obj_add_strn(doc, root, "level", lws_log_levels[level].data,
				lws_log_levels[level].len)) {
			yyjson_mut_doc_free(doc);
			goto textmode;
		}
		if (!yyjson_mut_obj_add_strn(doc, root, "msg", msg, msg_len)) {
			yyjson_mut_doc_free(doc);
			goto textmode;
		}
		if (ctx->request_id.len) {
			if (!yyjson_mut_obj_add_strn(doc, root, "requestId", ctx->request_id.data,
					ctx->request_id.len)) {
				yyjson_mut_doc_free(doc);
				goto textmode;
			}
		}

		/* output log line */
		flockfile(stdout);
		if (!yyjson_mut_write_fp(stdout, doc, 0, NULL, NULL)) {
			yyjson_mut_doc_free(doc);
			funlockfile(stdout);
			goto textmode;
		}
		fputc('\n', stdout);
		fflush(stdout);
		funlockfile(stdout);
		yyjson_mut_doc_free(doc);
		return;
	}

	textmode:
	flockfile(stdout);
	if (ctx->request_id.len) {
		fprintf(stdout, "%.*s [%.*s] %.*s %.*s\n",
				(int)tsm_len, tsm,
				(int)lws_log_levels[level].len, lws_log_levels[level].data,
				(int)ctx->request_id.len, ctx->request_id.data,
				(int)msg_len, msg);
	} else {
		fprintf(stdout, "%.*s [%.*s] %.*s\n",
				(int)tsm_len, tsm,
				(int)lws_log_levels[level].len, lws_log_levels[level].data,
				(int)msg_len, msg);
	}
	fflush(stdout);
	funlockfile(stdout);
}
