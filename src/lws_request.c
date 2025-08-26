/*
 * LWS request
 *
 * Copyright (C) 2025 Andre Naef
 */


#include <stdio.h>
#include <sys/stat.h>
#include <lws_runtime.h>
#include <lws_request.h>
#include <lws_interface.h>
#include <lws_state.h>
#include <lws_http.h>
#include <lws_codec.h>


typedef enum {
	LWS_FS_UNKNOWN,
	LWS_FS_FOUND,
	LWS_FS_NOT_FOUND
} lws_file_status_e;


static lws_file_status_e lws_get_file_status(lws_ctx_t *ctx, lws_str_t *filename);
static ssize_t lws_write_response_body(void *cookie, const char *data, size_t len);
static int lws_seek_response_body(void *cookie, off64_t *offset, int whence);
static int lws_string_sub(lws_str_t *dest, lws_str_t *tmpl, lws_str_t *src, regmatch_t *match);
static int lws_prepare_request(lws_ctx_t *ctx);
static int lws_prepare_response(lws_ctx_t *ctx);
static int lws_finalize_response(lws_ctx_t *ctx);


static cookie_io_functions_t response_body_io_functions = {
	.read  = NULL,
	.write = lws_write_response_body,
	.seek  = lws_seek_response_body,
	.close = NULL
};


static lws_file_status_e lws_get_file_status (lws_ctx_t *ctx, lws_str_t *filename) {
	struct stat        sb;
	lws_file_status_e  fs;

	fs = (uintptr_t)lws_table_get(ctx->stat_cache, filename);
	lws_log_debug("stat_cache get filename:%.*s fs:%d", (int)filename->len, filename->data, fs);
	if (fs != LWS_FS_UNKNOWN) {
		return fs;
	}
	fs = stat((const char *)filename->data, &sb) == 0 && S_ISREG(sb.st_mode) ? LWS_FS_FOUND
			: LWS_FS_NOT_FOUND;
	lws_table_set(ctx->stat_cache, filename, (void *)fs);
	lws_log_debug("stat_cache set filename:%.*s fs:%d", (int)filename->len, filename->data, fs);
	return fs;
}

static ssize_t lws_write_response_body (void *cookie, const char *data, size_t len) {
	char       *resp_body_new;
	size_t      required, capacity;
	lws_ctx_t  *ctx;
	lws_str_t   key, *ct;

	/* lock as needed */
	ctx = cookie;
	
	/* sanity checks */
	if (len > SIZE_MAX - ctx->resp_body.len) {
		lws_log(LWS_LOG_ERR, "response body too large");
		return -1;
	}

	/* determine required space */
	if (ctx->resp_body.len == 0) {
		lws_str_set(&key, "Content-Type");
		ct = lws_table_get(ctx->resp_headers, &key);
		ctx->likely_utf8 = ct && ((ct->len >= 9 && lws_strncmp(ct->data, "text/html", 9) == 0)
				|| (ct->len >= 10 && lws_strncmp(ct->data, "text/plain", 10) == 0)
				|| (ct->len >= 16 && lws_strncmp(ct->data, "application/json", 16) == 0));
	}
	if (ctx->likely_utf8 || ctx->streaming) {
		required = ctx->resp_body.len + len;
	} else {
		if (lws_base64_encode_len(ctx->resp_body.len + len, &required) != 0) {
			required = ctx->resp_body.len + len;
		}
	}

	/* allocate as needed */
	capacity = ctx->resp_body_cap;
	if (capacity < required) {
		if (capacity == 0) {
			capacity = 4096;
		}
		while (capacity < required) {
			if (capacity < 1024 * 1024) {
				if (capacity <= SIZE_MAX / 2) {
					capacity *= 2;
				} else {
					capacity = required;
				}
			} else {
				if (capacity <= SIZE_MAX / 3 * 2) {
					capacity = capacity + capacity / 2;
				} else {
					capacity = required;
				}
			}
		}
		resp_body_new = lws_realloc(ctx->resp_body.data, capacity);
		if (!resp_body_new) {
			return -1;
		}
		ctx->resp_body.data = resp_body_new;
		ctx->resp_body_cap = capacity;
	}
	memcpy(ctx->resp_body.data + ctx->resp_body.len, data, len);
	ctx->resp_body.len += len;

	return len;
}

static int lws_seek_response_body (void *cookie, off64_t *offset, int whence) {
	lws_ctx_t  *ctx;
	
	ctx = cookie;
	if (whence != SEEK_SET || *offset < 0 || (size_t)*offset > ctx->resp_body.len) {
		return -1;
	}
	ctx->resp_body.len = (size_t)*offset;
	return 0;
}

static int lws_string_sub (lws_str_t *dest, lws_str_t *tmpl, lws_str_t *src, regmatch_t *match) {
	int     d;
	char   *p;
	size_t  len, i, match_len;

	/* calculate length */
	len = 0;
	for (i = 0; i < tmpl->len; ) {
		if (tmpl->data[i] == '$' && i + 1 < tmpl->len && tmpl->data[i + 1] >= '0'
				&& tmpl->data[i + 1] <= '9') {
			d = tmpl->data[i + 1] - '0';
			if (match[d].rm_so < 0) {
				return -1;
			}
			match_len = (size_t)(match[d].rm_eo - match[d].rm_so);
			if (match_len > SIZE_MAX - len) {
				return -1;
			}
			len += match_len;
			i += 2;
			continue;
		}
		if (len == SIZE_MAX) {
			return -1;
		}
		len++;
		i++;
	}

	/* allocate */
	p = lws_alloc(len);
	if (!p) {
		return -1;
	}
	dest->len = len;
	dest->data = p;

	/* substitute */
	for (i = 0; i < tmpl->len; ) {
		if (tmpl->data[i] == '$' && i + 1 < tmpl->len && tmpl->data[i + 1] >= '0'
				&& tmpl->data[i + 1] <= '9') {
			d = tmpl->data[i + 1] - '0';
			match_len = (size_t)(match[d].rm_eo - match[d].rm_so);
			memcpy(p, src->data + match[d].rm_so, match_len);
			p += match_len;
			i += 2;
			continue;
		}
		*p++ = tmpl->data[i++];
	}

	return 0;
}

static int lws_prepare_request (lws_ctx_t *ctx) {
	int         i;
	lws_str_t   main;
	regmatch_t  path_match[10];

	/* match path */
	path_match[0].rm_so = 0;
	path_match[0].rm_eo = ctx->req_path.len;
	if (ctx->match.re_nsub > 0) {
		if (regexec(&ctx->match, ctx->req_path.data, 10, path_match, REG_STARTEND) != 0) {
			lws_log_debug("path not matched:%.*s", (int)ctx->req_path.len, ctx->req_path.data);
			return 404;  /* not found */
		}
	} else {
		for (i = 1; i < 10; i++) {
			path_match[i].rm_so = -1;
			path_match[i].rm_eo = -1;
		}
	}

	/* main */
	if (lws_string_sub(&main, &ctx->main, &ctx->req_path, path_match) != 0) {
		lws_log(LWS_LOG_ERR, "failed to substitute main path");
		return -1;
	}
	if (ctx->task_root.len > SIZE_MAX - 1 || main.len >= SIZE_MAX - 1 - ctx->task_root.len) {
		lws_log(LWS_LOG_ERR, "main path too long len:%zu", main.len);
		lws_free(main.data);
		return -1;
	}
	ctx->req_main.len = ctx->task_root.len + 1 + main.len;
	ctx->req_main.data = lws_alloc(ctx->req_main.len + 1);
	if (!ctx->req_main.data) {
		lws_free(main.data);
		return -1;
	}
	memcpy(ctx->req_main.data, ctx->task_root.data, ctx->task_root.len);
	ctx->req_main.data[ctx->task_root.len] = '/';
	memcpy(ctx->req_main.data + ctx->task_root.len + 1, main.data, main.len);
	ctx->req_main.data[ctx->req_main.len] = '\0';
	lws_free(main.data);
	lws_log_debug("main filename:%.*s", (int)ctx->req_main.len, ctx->req_main.data);
	if (lws_get_file_status(ctx, &ctx->req_main) != LWS_FS_FOUND) {
		return 404;  /* not found */
	}

	/* path info */
	if (ctx->path_info.len) {
		if (lws_string_sub(&ctx->req_path_info, &ctx->path_info, &ctx->req_path, path_match) != 0) {
			lws_log(LWS_LOG_ERR, "failed to substitute path info");
			return -1;
		}
		lws_log_debug("path info:%.*s", (int)ctx->req_path_info.len, ctx->req_path_info.data);
	}

	/* body */
	ctx->req_body_file = fmemopen(ctx->req_body.data, ctx->req_body.len, "rb");
	if (!ctx->req_body_file) {
		lws_log(LWS_LOG_CRIT, "failed to open request body file");
		return -1;
	}

	return 0;
}

static int lws_prepare_response (lws_ctx_t *ctx) {
	/* status */
	ctx->resp_status = 200;  /* OK */

	/* body */
	ctx->resp_body_file = fopencookie(ctx, "wb", response_body_io_functions);
	if (!ctx->resp_body_file) {
		lws_log(LWS_LOG_CRIT, "failed to open response body file");
		return -1;
	}
	if (setvbuf(ctx->resp_body_file, NULL, _IONBF, 0) != 0) {
		lws_log(LWS_LOG_ERR, "failed to set response body file buffer");
		return -1;
	}

	return 0;
}

static int lws_finalize_response (lws_ctx_t *ctx) {
	if (ctx->streaming) {
		ctx->streaming_eof = 1;
		if (lws_stream_response(ctx, 1) != 0) {
			return -1;
		}
	}
	return 0;
}

int lws_handle_request (lws_ctx_t *ctx) {
	int  rc, rc_finalize;

	if ((rc = lws_prepare_response(ctx)) != 0) {
		return rc;
	}
	if ((rc = lws_prepare_request(ctx)) != 0) {
		return rc;
	}
	if ((rc = lws_acquire_state(ctx)) != 0) {
		return rc;
	}
	if ((rc = lws_run_state(ctx)) != 0) {
		if (rc < 0) {
			rc = 500;  /* Lua error generated -> treat as 500 Internal Server Error */
		}
	}
	lws_release_state(ctx);
	if ((rc_finalize = lws_finalize_response(ctx)) != 0) {
		rc = rc_finalize;
	}
	return rc;
}

int lws_error_response (lws_ctx_t *ctx, int code) {
	int                 rc;
	yyjson_mut_doc     *doc;
	yyjson_mut_val     *root, *error;
	lws_http_status_t  *status;

	/* init state */
	rc = -1;
	doc = NULL;

	/* check response body file and truncate */
	if (!ctx->resp_body_file) {
		lws_log(LWS_LOG_CRIT, "response body file not initialized");
		goto cleanup;
	}
	if (fseek(ctx->resp_body_file, 0, SEEK_SET) != 0) {
		lws_log(LWS_LOG_ERR, "failed to seek response body file");
		goto cleanup;
	}

	/* prepare error response */
	doc = yyjson_mut_doc_new(NULL);
	if (!doc) {
		goto oom;
	}
	root = yyjson_mut_obj(doc);
	if (!root) {
		goto oom;
	}
	yyjson_mut_doc_set_root(doc, root);
	error = yyjson_mut_obj(doc);
	if (!error) {
		goto oom;
	}
	if (!yyjson_mut_obj_add_val(doc, root, "error", error)) {
		goto oom;
	}
	if (!yyjson_mut_obj_add_int(doc, error, "code", code)) {
		goto oom;
	}
	status = lws_find_http_status(code);
	if (status) {
		if (!yyjson_mut_obj_add_strn(doc, error, "message", status->message.data,
				status->message.len)) {
			goto oom;
		}
	}
	if (ctx->diagnostic.data) {
		if (!yyjson_mut_obj_add_strn(doc, error, "diagnostic", ctx->diagnostic.data,
				ctx->diagnostic.len)) {
			goto oom;
		}
	}

	/* write error response to response body file */
	if (!yyjson_mut_write_fp(ctx->resp_body_file, doc, 0, NULL, NULL)) {
		goto oom;
	}

	/* successfully wrote error response */
	rc = 0;
	goto cleanup;

	oom:
	lws_log(LWS_LOG_CRIT, "out of memory");

	cleanup:
	if (doc) {
		yyjson_mut_doc_free(doc);
	}
	return rc;
}
