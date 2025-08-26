/*
 * LWS runtime interface
 *
 * Copyright (C) 2025 Andre Naef
 */


#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <lws_runtime.h>
#include <lws_interface.h>
#include <lws_ngx.h>
#include <lws_codec.h>
#include <curl/curl.h>


#define LWS_USERAGENT               "lambda-lws/0.9"
#define LWS_CURL_OFF_T_MAX          (size_t)((curl_off_t)(~(curl_off_t)0 >> 1))
#define LWS_CONTENT_LENGTH          "Content-Length"
#define LWS_LAMBDA_TRACE_ID         "Lambda-Runtime-Trace-Id"
#define LWS_LAMBDA_DEADLINE_MS      "Lambda-Runtime-Deadline-Ms"
#define LWS_LAMBDA_REQUEST_ID       "Lambda-Runtime-Aws-Request-Id"
#define LWS_LAMBDA_STREAMING_CT     "Content-Type: application/vnd.awslambda.http-integration-response"
#define LWS_LAMBDA_STREAMING_MODE   "Lambda-Runtime-Function-Response-Mode: streaming"
#define LWS_LAMBDA_TRACE_ID_ENV     "_X_AMZN_TRACE_ID"
#define LWS_LAMBDA_DEADLINE_ENV     "_DEADLINE_MS"
#define LWS_LAMBDA_RUNTIME_VERSION  "2018-06-01"
#define LWS_LAMBDA_PAYLOAD_VERSION  "2.0"
#define LWS_LAMBDA_SEPARATOR_LEN    8
#define LWS_CURL_INITIATE           1
#define LWS_CURL_FINALIZE           2

/* libcurl */
static void lws_curl_reset(lws_ctx_t *ctx);
static int lws_curl_perform(lws_ctx_t *ctx, int flags);
static int lws_handle_socket(void *userdata, curl_socket_t fd, curlsocktype purpose);
static size_t lws_handle_header(char *buffer, size_t size, size_t nitems, void *userdata);
static size_t lws_handle_write(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t lws_handle_read(char *ptr, size_t size, size_t nmemb, void *userdata);

/* runtime interface */
static int lws_add_cookie(yyjson_mut_doc *doc, yyjson_mut_val *arr, char *data, size_t start,
		size_t end);
static int lws_add_headers(lws_ctx_t *ctx, yyjson_mut_doc *doc);


static volatile int cancel_poll = 0;
static volatile int in_poll = 0;
static volatile curl_socket_t curl_socket = CURL_SOCKET_BAD;


/*
 * libcurl
 */

static void lws_curl_reset (lws_ctx_t *ctx) {
	curl_easy_reset(ctx->curl);
	curl_easy_setopt(ctx->curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_CONNECTTIMEOUT, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 0L);
	curl_easy_setopt(ctx->curl, CURLOPT_TCP_NODELAY, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, LWS_USERAGENT);
}

static int lws_curl_perform (lws_ctx_t *ctx, int flags) {
	int        remain, rc;
	long       code;
	CURLMsg   *msg;
	CURLMcode  mres;

	/* add the handle */
	if (flags & LWS_CURL_INITIATE) {
		if (curl_multi_add_handle(ctx->curlm, ctx->curl) != CURLM_OK) {
			lws_log(LWS_LOG_CRIT, "failed to add libcurl easy handle to multi handle");
			return -1;
		}
	}

	/* drive the request */
	while (1) {
		mres = curl_multi_perform(ctx->curlm, &remain);
		if (mres != CURLM_OK) {
			lws_log(LWS_LOG_CRIT, "libcurl multi error code:%d str:%s", mres,
					curl_multi_strerror(mres));	
			return -1;
		}
		if (!(flags & LWS_CURL_FINALIZE) && (ctx->streaming_paused || !remain)) {
			return 0;  /* not supposed to finalize and done for now */
		} else if (!remain) {
			break;  /* request complete */
		}
		curl_multi_wait(ctx->curlm, NULL, 0, 1000, NULL);
	}

	/* reap the result */
	rc = -1;
	while (1) {
		msg = curl_multi_info_read(ctx->curlm, &remain);
		if (!msg) {
			lws_log(LWS_LOG_CRIT, "failed to read libcurl multi info");
			goto cleanup;
		}
		if (msg->msg != CURLMSG_DONE) {
			continue;
		}
		if (msg->data.result != CURLE_OK) {
			if (cancel_poll) {
				lws_log(LWS_LOG_NOTICE, "poll cancelled");
			} else {
				lws_log(LWS_LOG_CRIT, "libcurl easy error code:%d str:%s",
						msg->data.result, curl_easy_strerror(msg->data.result));
			}
			goto cleanup;
		}
		if (curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code) != CURLE_OK) {
			lws_log(LWS_LOG_CRIT, "failed to get HTTP status code");
			goto cleanup;
		}
		if (code < 200 || code >= 300) {
			lws_log(LWS_LOG_CRIT, "bad HTTP status code:%ld", code);
			goto cleanup;
		}
		break;
	}

	/* successfuly finalized request */
	rc = 0;

	cleanup:
	if (curl_multi_remove_handle(ctx->curlm, ctx->curl) != CURLM_OK) {
		lws_log(LWS_LOG_CRIT, "failed to remove libcurl easy handle from multi handle");
		return -1;
	}

	return rc;
}

static int lws_handle_socket (void *userdata, curl_socket_t fd, curlsocktype purpose) {
	if (purpose != CURLSOCKTYPE_IPCXN) {
		return 0;
	}
	if (cancel_poll) {
		return -1;  /* prevent reconnect attempt after forcefully closing socket */
	}
	curl_socket = fd;  /* get hold of socket to forcefully close to cancel poll */
	lws_log_debug("captured socket fd:%d", fd);
	return 0;
}

static size_t lws_handle_header (char *buffer, size_t size, size_t nitems, void *userdata) {
	char          *eol, *colon, *val;
	size_t         len, remain, copy, line_len, name_len, val_len, shift;
	lws_ctx_t     *ctx;
	unsigned long  ul;

	/* sanity checks */
	ctx = userdata;
	if (nitems && size > SIZE_MAX / nitems) {
		lws_log(LWS_LOG_ERR, "header size overflow size:%zu nitems:%zu", size, nitems);
		return 0;
	}
	len = size * nitems;
	remain = len;

	/* loop on input buffer */
	while (remain > 0) {
		copy = sizeof(ctx->header) - ctx->header_len;
		if (copy == 0) {
			lws_log(LWS_LOG_ERR, "single header line too large");
			return 0;
		}
		if (copy > remain) {
			copy = remain;
		}
		memcpy(ctx->header + ctx->header_len, buffer + (len - remain), copy);
		ctx->header_len += copy;
		remain -= copy;

		/* loop on lines */
		for (;;) {
			eol = NULL;
			for (line_len = 0; line_len + 1 < ctx->header_len; line_len++) {
				if (ctx->header[line_len] == '\r' && ctx->header[line_len + 1] == '\n') {
					eol = ctx->header + line_len;
					break;
				}
			}
			if (!eol) {
				break;
			}
			colon = memchr(ctx->header, ':', line_len);
			if (colon != NULL) {
				name_len = colon - ctx->header;
				while (name_len && (ctx->header[name_len - 1] == ' '
						|| ctx->header[name_len - 1] == '\t')) {
					name_len--;
				}
				val = colon + 1;
				while ((size_t)(val - ctx->header) < line_len && (*val == ' ' || *val == '\t')) {
					val++;
				}
				val_len = line_len - (val - ctx->header);
				while (val_len && (val[val_len - 1] == ' ' || val[val_len - 1] == '\t')) {
					val_len--;
				}
				switch (name_len) {
					case sizeof(LWS_CONTENT_LENGTH) - 1:
						if (lws_strncasecmp(ctx->header, LWS_CONTENT_LENGTH, name_len) == 0) {
							val[val_len] = '\0';
							errno = 0;
							ul = strtoul(val, &eol, 10);
							if (errno == 0 && eol == val + val_len && ul <= SSIZE_MAX) {
								ctx->content_length = (ssize_t)ul;
								lws_log_debug("header content_length:%zd", ctx->content_length);
							}
						}
						break;

					case sizeof(LWS_LAMBDA_TRACE_ID) - 1:
						if (lws_strncasecmp(ctx->header, LWS_LAMBDA_TRACE_ID, name_len) == 0) {
							val[val_len] = '\0';
							if (setenv(LWS_LAMBDA_TRACE_ID_ENV, val, 1) != 0) {
								lws_log(LWS_LOG_ERR, "failed to set " LWS_LAMBDA_TRACE_ID_ENV);
								return 0;
							}
							ctx->got_trace = 1;
							lws_log_debug("header trace_id:%s", val);
						}
						break;

					case sizeof(LWS_LAMBDA_DEADLINE_MS) - 1:
						if (lws_strncasecmp(ctx->header, LWS_LAMBDA_DEADLINE_MS, name_len) == 0) {
							val[val_len] = '\0';
							if (setenv(LWS_LAMBDA_DEADLINE_ENV, val, 1) != 0) {
								lws_log(LWS_LOG_ERR, "failed to set " LWS_LAMBDA_DEADLINE_ENV);
								return 0;
							}
							ctx->got_deadline = 1;
							lws_log_debug("header deadline_ms:%s", val);
						}
						break;

					case sizeof(LWS_LAMBDA_REQUEST_ID) - 1:
						if (lws_strncasecmp(ctx->header, LWS_LAMBDA_REQUEST_ID, name_len) == 0) {
							if (val_len >= sizeof(ctx->request_id)) {
								lws_log(LWS_LOG_ERR, "request ID too long val_len:%zu", val_len);
								return 0;
							}
							memcpy(ctx->request_id, val, val_len);
							ctx->request_id[val_len] = '\0';
							lws_log_debug("header request_id:%s", ctx->request_id);
						}
						break;
				}
			}
			shift = line_len + 2;
			memmove(ctx->header, ctx->header + shift, ctx->header_len - shift);
			ctx->header_len -= shift;
		}
	}
	return len;
}

static size_t lws_handle_write (char *ptr, size_t size, size_t nmemb, void *userdata) {
	char       *body_new;
	size_t      len, required, capacity;
	lws_ctx_t  *ctx;

	/* sanity checks */
	ctx = userdata;
	if (nmemb && size > SIZE_MAX / nmemb) {
		lws_log(LWS_LOG_ERR, "request body size overflow size:%zu nmemb:%zu", size, nmemb);
		return 0;
	}
	len = size * nmemb;
	if (len > SIZE_MAX - YYJSON_PADDING_SIZE - ctx->body.len) {
		lws_log(LWS_LOG_ERR, "request body too large len:%zu body.len:%zu", len, ctx->body.len);
		return 0;
	}

	/* determine required space */
	required = ctx->body.len + len + YYJSON_PADDING_SIZE;
	if (ctx->body_cap == 0 && ctx->content_length >= 0 && required < (size_t)ctx->content_length
			+ YYJSON_PADDING_SIZE) {
		/* use content length hint */
		required = (size_t)ctx->content_length + YYJSON_PADDING_SIZE;
	}

	/* allocate as needed */
	capacity = ctx->body_cap;
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
		body_new = lws_realloc(ctx->body.data, capacity);
		if (!body_new) {
			return 0;
		}
		ctx->body.data = body_new;
		ctx->body_cap = capacity;
	}

	/* copy data */
	memcpy(ctx->body.data + ctx->body.len, ptr, len);
	ctx->body.len += len;

	return len;
}

static size_t lws_handle_read (char *ptr, size_t size, size_t nmemb, void *userdata) {
	size_t      len, remain, n;
	lws_ctx_t  *ctx;

	/* sanity checks */
	ctx = userdata;
	if (nmemb && size > SIZE_MAX / nmemb) {
		lws_log(LWS_LOG_ERR, "request body size overflow size:%zu nmemb:%zu", size, nmemb);
		return CURL_READFUNC_ABORT;
	}
	len = size * nmemb;
	remain = len;

	/* send prelude */
	if (ctx->streaming_prelude_pos < ctx->streaming_prelude.len) {
		n = ctx->streaming_prelude.len - ctx->streaming_prelude_pos;
		if (n > remain) {
			n = remain;
		}
		memcpy(ptr, ctx->streaming_prelude.data + ctx->streaming_prelude_pos, n);
		ctx->streaming_prelude_pos += n;
		remain -= n;
		ptr += n;
	}

	/* send separator */
	if (ctx->streaming_separator < LWS_LAMBDA_SEPARATOR_LEN) {
		n = LWS_LAMBDA_SEPARATOR_LEN - ctx->streaming_separator;
		if (n > remain) {
			n = remain;
		}
		lws_memzero(ptr, n);
		ctx->streaming_separator += n;
		remain -= n;
		ptr += n;
	}

	/* send response data, end, or pause streaming */
	if (ctx->resp_body_pos < ctx->resp_body.len) {
		n = ctx->resp_body.len - ctx->resp_body_pos;
		if (n > remain) {
			n = remain;
		}
		memcpy(ptr, ctx->resp_body.data + ctx->resp_body_pos, n);
		ctx->resp_body_pos += n;
		remain -= n;
		ptr += n;
	}

	/* nothing sent? */
	if (remain == len) {
		if (ctx->streaming_eof) {
			return 0;  /* end of data */
		} else {
			ctx->streaming_paused = 1;
			return CURL_READFUNC_PAUSE;  /* pause streaming */
		}
	}

	return len - remain;
}


/*
 * runtime interface
 */

int lws_get_next_invocation (lws_ctx_t *ctx) {
	char         invocation_url[256], *cookie_ptr;
	size_t       idx, max, cookie_len, cookie_total_len;
	lws_str_t    key, *value;
	lws_uint_t   cookie_count;
	yyjson_val  *root, *request_context, *http, *headers, *cookies, *val, *h_key, *h_val;

	/* prepare invocation URL */
	snprintf(invocation_url, sizeof(invocation_url), "http://%.*s/%s/runtime/invocation/next",
			(int)ctx->runtime_api.len, ctx->runtime_api.data, LWS_LAMBDA_RUNTIME_VERSION);

	/* get invocation */
	lws_curl_reset(ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, invocation_url);
	curl_easy_setopt(ctx->curl, CURLOPT_SOCKOPTFUNCTION, lws_handle_socket);
	curl_easy_setopt(ctx->curl, CURLOPT_HEADERFUNCTION, lws_handle_header);
	curl_easy_setopt(ctx->curl, CURLOPT_HEADERDATA, ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, lws_handle_write);
	curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx);
	in_poll = 1;
	if (lws_curl_perform(ctx, LWS_CURL_INITIATE | LWS_CURL_FINALIZE) != 0) {
		in_poll = 0;
		return -1;
	}
	in_poll = 0;
	if (ctx->request_id[0] == '\0') {
		lws_log(LWS_LOG_CRIT, "missing request ID header");
		return -1;
	}

	/* clear env if we didn't get the corresponding headers */
	if (!ctx->got_trace) {
		if (unsetenv(LWS_LAMBDA_TRACE_ID_ENV) != 0) {
			lws_log(LWS_LOG_ERR, "failed to clear " LWS_LAMBDA_TRACE_ID_ENV);
			return -1;
		}
	}
	if (!ctx->got_deadline) {
		if (unsetenv(LWS_LAMBDA_DEADLINE_ENV) != 0) {
			lws_log(LWS_LOG_ERR, "failed to clear " LWS_LAMBDA_DEADLINE_ENV);
			return -1;
		}
	}

	/* handle regular and raw mode */
	if (!ctx->raw) {
		/* regular mode */
	
		/* parse inplace */
		memset(ctx->body.data + ctx->body.len, 0, YYJSON_PADDING_SIZE);
		ctx->doc = yyjson_read_opts(ctx->body.data, ctx->body.len, YYJSON_READ_INSITU, NULL, NULL);
		if (!ctx->doc) {
			lws_log(LWS_LOG_ERR, "failed to parse request body as JSON");
			return -1;
		}

		/* version */
		root = yyjson_doc_get_root(ctx->doc);
		if (!root) {
			lws_log(LWS_LOG_ERR, "root not found");
			return -1;
		}
		val = yyjson_obj_get(root, "version");
		if (!val || !yyjson_is_str(val)) {
			lws_log(LWS_LOG_ERR, "version not found in root");
			return -1;
		}
		if (strcmp(yyjson_get_str(val), LWS_LAMBDA_PAYLOAD_VERSION) != 0) {
			lws_log(LWS_LOG_ERR, "unsupported payload version:%s", yyjson_get_str(val));
			return -1;
		}

		/* path */
		val = yyjson_obj_get(root, "rawPath");
		if (!val || !yyjson_is_str(val)) {
			lws_log(LWS_LOG_ERR, "rawPath not found");
			return -1;
		}
		ctx->req_path.data = (char *)yyjson_get_str(val);
		ctx->req_path.len = yyjson_get_len(val);

		/* method */
		request_context = yyjson_obj_get(root, "requestContext");
		if (!request_context) {
			lws_log(LWS_LOG_ERR, "requestContext object not found");
			return -1;
		}
		http = yyjson_obj_get(request_context, "http");
		if (!http) {
			lws_log(LWS_LOG_ERR, "http object not found");
			return -1;
		}
		val = yyjson_obj_get(http, "method");
		if (!val || !yyjson_is_str(val)) {
			lws_log(LWS_LOG_ERR, "method not found");
			return -1;
		}
		ctx->req_method.data = (char *)yyjson_get_str(val);
		ctx->req_method.len = yyjson_get_len(val);

		/* args */
		val = yyjson_obj_get(root, "rawQueryString");
		if (val) {
			if (!yyjson_is_str(val)) {
				lws_log(LWS_LOG_ERR, "rawQueryString not a string");
				return -1;
			}
			ctx->req_args.data = (char *)yyjson_get_str(val);
			ctx->req_args.len = yyjson_get_len(val);
		} else {
			lws_str_set(&ctx->req_args, "");
		}

		/* source IP */
		val = yyjson_obj_get(http, "sourceIp");
		if (val) {
			if (!yyjson_is_str(val)) {
				lws_log(LWS_LOG_ERR, "sourceIp not a string");
				return -1;
			}
			ctx->req_ip.data = (char *)yyjson_get_str(val);
			ctx->req_ip.len = yyjson_get_len(val);
		}

		/* headers */
		headers = yyjson_obj_get(root, "headers");
		if (headers) {
			if (!yyjson_is_obj(headers)) {
				lws_log(LWS_LOG_ERR, "headers not an object");
				return -1;
			}
			yyjson_obj_foreach(headers, idx, max, h_key, h_val) {
				if (!yyjson_is_str(h_key) || !yyjson_is_str(h_val)) {
					lws_log(LWS_LOG_ERR, "header key or value not a string");
					return -1;
				}
				key.data = (char *)yyjson_get_str(h_key);
				key.len = yyjson_get_len(h_key);
				value = lws_alloc(sizeof(lws_str_t));
				if (!value) {
					return -1;
				}
				value->data = (char *)yyjson_get_str(h_val);
				value->len = yyjson_get_len(h_val);
				if (lws_table_set(ctx->req_headers, &key, value) != 0) {
					lws_free(value);
					return -1;
				}
			}
		}

		/* cookies */
		cookies = yyjson_obj_get(root, "cookies");
		if (cookies) {
			if (!yyjson_is_arr(cookies)) {
				lws_log(LWS_LOG_ERR, "cookies not an array");
				return -1;
			}
			cookie_total_len = 0;
			cookie_count = 0;
			yyjson_arr_foreach(cookies, idx, max, val) {
				if (!yyjson_is_str(val)) {
					lws_log(LWS_LOG_ERR, "cookie value not a string");
					return -1;
				}
				cookie_len = yyjson_get_len(val);
				if (cookie_len == 0) {
					continue;
				}
				if (cookie_len > SIZE_MAX - sizeof(lws_str_t) - 2
						|| cookie_total_len > SIZE_MAX - sizeof(lws_str_t) - 2 - cookie_len) {
					lws_log(LWS_LOG_ERR, "cookies header too large");
					return -1;
				}
				cookie_total_len += cookie_len + (cookie_count ? 2 : 0);
				cookie_count++;
			}
			if (cookie_count) {
				lws_str_set(&key, "Cookie");
				value = lws_alloc(sizeof(lws_str_t) + cookie_total_len);
				if (!value) {
					return -1;
				}
				value->data = (char *)value + sizeof(lws_str_t);
				value->len = cookie_total_len;
				cookie_ptr = value->data;
				cookie_count = 0;
				yyjson_arr_foreach(cookies, idx, max, val) {
					cookie_len = yyjson_get_len(val);
					if (cookie_len == 0) {
						continue;
					}
					if (cookie_count) {
						memcpy(cookie_ptr, ", ", 2);
						cookie_ptr += 2;
					}
					memcpy(cookie_ptr, yyjson_get_str(val), cookie_len);
					cookie_ptr += cookie_len;
					cookie_count++;
				}
				if (lws_table_set(ctx->req_headers, &key, value) != 0) {
					lws_free(value);
					return -1;
				}
			}
		}

		/* request body */
		val = yyjson_obj_get(root, "body");
		if (val) {
			if (!yyjson_is_str(val)) {
				lws_log(LWS_LOG_ERR, "request body not a string");
				return -1;
			}
			ctx->req_body.data = (char *)yyjson_get_str(val);
			ctx->req_body.len = yyjson_get_len(val);
			val = yyjson_obj_get(root, "isBase64Encoded");
			if (!val || !yyjson_is_bool(val)) {
				lws_log(LWS_LOG_ERR, "isBase64Encoded not found");
				return -1;
			}
			if (yyjson_is_true(val)) {
				if (lws_base64_decode((uint8_t *)ctx->req_body.data, &ctx->req_body.len) != 0) {
					lws_log(LWS_LOG_ERR, "failed to decode base64 request body");
					return -1;
				}
				ctx->req_body.data[ctx->req_body.len] = '\0';
			}
		} else {
			lws_str_set(&ctx->req_body, "");
		}
	} else {
		/* raw mode */
		ctx->req_body = ctx->body;
	}

	return 0;
}

static int lws_add_cookie (yyjson_mut_doc *doc, yyjson_mut_val *arr, char *data, size_t start,
		size_t end) {
	yyjson_mut_val  *val;

	while (start < end && (data[start] == ' ' || data[start] == '\t')) {
		start++;
	}
	while (end > start && (data[end - 1] == ' ' || data[end - 1] == '\t')) {
		end--;
	}
	if (start < end) {
		val = yyjson_mut_strn(doc, data + start, end - start);
		if (!val) {
			return -1;
		}
		if (!yyjson_mut_arr_add_val(arr, val)) {
			return -1;
		}
	}
	return 0;
}

static int lws_add_headers (lws_ctx_t *ctx, yyjson_mut_doc *doc) {
	char            *sc_data;
	void            *value;
	size_t           sc_len, sc_start, sc_pos;
	lws_str_t       *key, sc_key, *sc_value;
	yyjson_mut_val  *root, *headers, *h_key, *h_val, *cookies_arr;

	/* status code */
	root = yyjson_mut_doc_get_root(doc);
	if (!yyjson_mut_obj_add_int(doc, root, "statusCode", ctx->resp_status)) {
		return -1;
	}

	/* headers */
	headers = yyjson_mut_obj(doc);
	if (!headers) {
		return -1;
	}
	if (!yyjson_mut_obj_add_val(doc, root, "headers", headers)) {
		return -1;
	}
	lws_str_set(&sc_key, "Set-Cookie");
	key = NULL;
	while (lws_table_next(ctx->resp_headers, key, &key, &value) == 0) {
		if (key->len == sc_key.len && lws_strncasecmp(key->data, sc_key.data, sc_key.len) == 0) {
			continue;
		}
		h_key = yyjson_mut_strn(doc, key->data, key->len);
		if (!h_key) {
			return -1;
		}
		h_val = yyjson_mut_strn(doc, ((lws_str_t *)value)->data, ((lws_str_t *)value)->len);
		if (!h_val) {
			return -1;
		}
		if (!yyjson_mut_obj_add(headers, h_key, h_val)) {
			return -1;
		}
	}

	/* cookies */
	sc_value = lws_table_get(ctx->resp_headers, &sc_key);
	if (sc_value) {
		sc_data = sc_value->data;
		sc_len = sc_value->len;
		cookies_arr = yyjson_mut_arr(doc);
		if (!cookies_arr) {
			return -1;
		}
		if (!yyjson_mut_obj_add_val(doc, root, "cookies", cookies_arr)) {
			return -1;
		}
		sc_start = 0;
		for (sc_pos = 0; sc_pos < sc_len; sc_pos++) {
			if (sc_data[sc_pos] == ',') {
				if (lws_add_cookie(doc, cookies_arr, sc_data, sc_start, sc_pos) != 0) {
					return -1;
				}
				sc_start = sc_pos + 1;
			}
		}
		if (lws_add_cookie(doc, cookies_arr, sc_data, sc_start, sc_len) != 0) {
			return -1;
		}
	}

	return 0;
}

int lws_post_response (lws_ctx_t *ctx) {
	int                 rc, base64;
	char                response_url[256], *resp_body_new, *json, *post;
	size_t              base64_len, json_len, post_len;
	yyjson_mut_doc     *doc;
	yyjson_mut_val     *root;
	struct curl_slist  *curl_headers;

	/* init state */
	rc = -1;
	curl_headers = NULL;
	doc = NULL;
	json = NULL;

	/* prepare response URL */
	snprintf(response_url, sizeof(response_url),
			"http://%.*s/%s/runtime/invocation/%s/response", (int)ctx->runtime_api.len,
			ctx->runtime_api.data, LWS_LAMBDA_RUNTIME_VERSION, ctx->request_id);

	/* prepare Lambda headers */
	curl_headers = curl_slist_append(curl_headers, "Content-Type: application/json");
	if (!curl_headers) {
		goto oom;
	}

	/* handle regular and raw mode */
	if (!ctx->raw) {
		/* regular mode */

		/* prepare Lambda body */
		doc = yyjson_mut_doc_new(NULL);
		if (!doc) {
			goto oom;
		}
		root = yyjson_mut_obj(doc);
		if (!root) {
			goto oom;
		}
		yyjson_mut_doc_set_root(doc, root);

		/* add headers */
		if (lws_add_headers(ctx, doc) != 0) {
			goto oom;
		}

		/* body */
		base64 = lws_valid_utf8((uint8_t *)ctx->resp_body.data, ctx->resp_body.len) != 0;
		if (base64) {
			if (lws_base64_encode_len(ctx->resp_body.len, &base64_len) != 0) {
				lws_log(LWS_LOG_ERR, "response body base64 encoding too large");
				goto cleanup;
			}
			if (ctx->resp_body_cap < base64_len) {
				resp_body_new = lws_realloc(ctx->resp_body.data, base64_len);
				if (!resp_body_new) {
					goto cleanup;
				}
				ctx->resp_body.data = resp_body_new;
				ctx->resp_body_cap = base64_len;
			}
			lws_base64_encode((uint8_t *)ctx->resp_body.data, &ctx->resp_body.len);
		}
		if (!yyjson_mut_obj_add_strn(doc, root, "body", ctx->resp_body.data, ctx->resp_body.len)) {
			goto oom;
		}
		if (!yyjson_mut_obj_add_bool(doc, root, "isBase64Encoded", base64)) {
			goto oom;
		}
	} else {
		/* raw mode */
		if (ctx->resp_body.len == 0) {
			/* no response body -> send JSON null */
			doc = yyjson_mut_doc_new(NULL);
			if (!doc) {
				goto oom;
			}
			root = yyjson_mut_null(doc);
			if (!root) {
				goto oom;
			}
			yyjson_mut_doc_set_root(doc, root);
		} else {
			/* send response body directly */
			if (ctx->resp_body.len > LWS_CURL_OFF_T_MAX) {
				lws_log(LWS_LOG_ERR, "response body too large len:%zu", ctx->resp_body.len);
				goto cleanup;
			}
			post = ctx->resp_body.data;
			post_len = ctx->resp_body.len;
		}
	}

	/* create JSON */
	if (doc) {
		json = yyjson_mut_write(doc, 0, &json_len);
		if (!json) {
			goto oom;
		}
		if (json_len > LWS_CURL_OFF_T_MAX) {
			lws_log(LWS_LOG_ERR, "response JSON too large len:%zu", json_len);
			goto cleanup;
		}
		post = json;
		post_len = json_len;
	}

	/* post response */
	lws_curl_reset(ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, response_url);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, curl_headers);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)post_len);
	if (lws_curl_perform(ctx, LWS_CURL_INITIATE | LWS_CURL_FINALIZE) != 0) {
		goto cleanup;
	}

	/* successfully posted response */
	rc = 0;
	goto cleanup;

	oom:
	lws_log(LWS_LOG_CRIT, "out of memory");

	cleanup:
	if (curl_headers != NULL) {
		curl_slist_free_all(curl_headers);
	}
	if (doc) {
		yyjson_mut_doc_free(doc);
	}
	if (json) {
		lws_free(json);
	}

	return rc;
}

int lws_stream_response (lws_ctx_t *ctx, int finish) {
	int                 rc;
	char                response_url[256];
	yyjson_mut_doc     *doc;
	yyjson_mut_val     *root;
	struct curl_slist  *curl_headers;

	/* raw mode? */
	if (ctx->raw) {
		lws_log(LWS_LOG_ERR, "streaming not supported in raw mode");
		return -1;
	}

	/* already streaming? */
	if (ctx->streaming) {
		if (ctx->streaming_paused) {
			if (curl_easy_pause(ctx->curl, CURLPAUSE_CONT) != CURLE_OK) {
				lws_log(LWS_LOG_ERR, "failed to resume streaming response");
				return -1;
			}
			ctx->streaming_paused = 0;
		}
		return lws_curl_perform(ctx, finish ? LWS_CURL_FINALIZE : 0);
	}

	/* init state */
	rc = -1;
	curl_headers = NULL;
	doc = NULL;

	/* prepare response URL */
	snprintf(response_url, sizeof(response_url),
			"http://%.*s/%s/runtime/invocation/%s/response", (int)ctx->runtime_api.len,
			ctx->runtime_api.data, LWS_LAMBDA_RUNTIME_VERSION, ctx->request_id);

	/* prepare Lambda headers */
	curl_headers = curl_slist_append(curl_headers, LWS_LAMBDA_STREAMING_CT);
	if (!curl_headers) {
		goto oom;
	}
	curl_headers = curl_slist_append(curl_headers, LWS_LAMBDA_STREAMING_MODE);
	if (!curl_headers) {
		goto oom;
	}
	curl_headers = curl_slist_append(curl_headers, "Expect:");  /* prevent "Expect: 100-continue" */
	if (!curl_headers) {
		goto oom;
	}

	/* prepare Lambda body */
	doc = yyjson_mut_doc_new(NULL);
	if (!doc) {
		goto oom;
	}
	root = yyjson_mut_obj(doc);
	if (!root) {
		goto oom;
	}
	yyjson_mut_doc_set_root(doc, root);

	/* add headers */
	if (lws_add_headers(ctx, doc) != 0) {
		goto oom;
	}

	/* create JSON */
	ctx->streaming_prelude.data = yyjson_mut_write(doc, 0, &ctx->streaming_prelude.len);
	if (!ctx->streaming_prelude.data) {
		goto oom;
	}

	/* initiate posting of streaming response */
	lws_curl_reset(ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, response_url);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, curl_headers);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)-1);
	curl_easy_setopt(ctx->curl, CURLOPT_READFUNCTION, lws_handle_read);
	curl_easy_setopt(ctx->curl, CURLOPT_READDATA, ctx);
	if (lws_curl_perform(ctx, LWS_CURL_INITIATE) != 0) {
		goto cleanup;
	}

	/* successfully initiated streaming response */
	ctx->streaming = 1;
	rc = 0;
	goto cleanup;

	oom:
	lws_log(LWS_LOG_CRIT, "out of memory");

	cleanup:
	if (curl_headers != NULL) {
		curl_slist_free_all(curl_headers);
	}
	if (doc) {
		yyjson_mut_doc_free(doc);
	}

	return rc;
}

int lws_post_error (lws_ctx_t *ctx, const char *error_message) {
	int                 rc;
	char                error_url[256], *json;
	size_t              json_len;
	yyjson_mut_doc     *doc;
	yyjson_mut_val     *root, *arr;
	struct curl_slist  *curl_headers;

	/* log error, prepare error url */
	if (ctx->request_id[0] == '\0') {
		lws_log(LWS_LOG_EMERG, "initialization error msg:%s", error_message);
		snprintf(error_url, sizeof(error_url), "http://%.*s/%s/runtime/init/error",
				(int)ctx->runtime_api.len, ctx->runtime_api.data, LWS_LAMBDA_RUNTIME_VERSION);
	} else {
		lws_log(LWS_LOG_ERR, "request error id:%s msg:%s", ctx->request_id, error_message);
		snprintf(error_url, sizeof(error_url),
				"http://%.*s/%s/runtime/invocation/%s/error", (int)ctx->runtime_api.len,
				ctx->runtime_api.data, LWS_LAMBDA_RUNTIME_VERSION, ctx->request_id);
	}

	/* init state */
	rc = -1;
	curl_headers = NULL;
	doc = NULL;
	json = NULL;

	/* prepare headers */
	curl_headers = curl_slist_append(curl_headers, "Content-Type: application/json");
	if (!curl_headers) {
		goto oom;
	}

	/* create error document */
	doc = yyjson_mut_doc_new(NULL);
	if (!doc) {
		goto oom;
	}
	root = yyjson_mut_obj(doc);
	if (!root) {
		goto oom;
	}
	yyjson_mut_doc_set_root(doc, root);
	if (!yyjson_mut_obj_add_str(doc, root, "errorMessage", error_message)) {
		goto oom;
	}
	if (!yyjson_mut_obj_add_null(doc, root, "errorType")) {
		goto oom;
	}
	arr = yyjson_mut_arr(doc);
	if (!arr) {
		goto oom;
	}
	if (!yyjson_mut_obj_add_val(doc, root, "stackTrace", arr)) {
		goto oom;
	}

	/* create JSON */
	json = yyjson_mut_write(doc, 0, &json_len);
	if (!json) {
		goto oom;
	}
	if (json_len > LWS_CURL_OFF_T_MAX) {
		lws_log(LWS_LOG_ERR, "error JSON too large len:%zu", json_len);
		goto cleanup;
	}

	/* post error */
	lws_curl_reset(ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, error_url);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, curl_headers);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, json);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)json_len);
	if (lws_curl_perform(ctx, LWS_CURL_INITIATE | LWS_CURL_FINALIZE) != 0) {
		goto cleanup;
	}

	/* successfully posted error */
	rc = 0;
	goto cleanup;

	oom:
	lws_log(LWS_LOG_CRIT, "out of memory");

	cleanup:
	if (curl_headers) {
		curl_slist_free_all(curl_headers);
	}
	if (doc) {
		yyjson_mut_doc_free(doc);
	}
	if (json) {
		lws_free(json);
	}
	return rc;
}

int lws_cancel_poll () {
	if (!in_poll || curl_socket == CURL_SOCKET_BAD) {
		return -1;
	}
	cancel_poll = 1;
	if (close(curl_socket) != 0) {
		return -1;
	}
	curl_socket = CURL_SOCKET_BAD;
	return 0;
}
