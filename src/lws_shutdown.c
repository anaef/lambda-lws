/*
 * LWS shutdown
 *
 * Copyright (C) 2026 Andre Naef
 */


#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <lws_log.h>
#include <lws_ngx.h>
#include <lws_shutdown.h>
#include <curl/curl.h>


#define LWS_USERAGENT                "lambda-lws/0.9"
#define LWS_EXTENSION_NAME           "lambda-lws"
#define LWS_EXTENSION_VERSION        "2020-01-01"
#define LWS_EXTENSION_ID             "Lambda-Extension-Identifier"
#define LWS_EXTENSION_ID_CAP         256


typedef struct {
	int                 runtime_pipefd[2];
	int                 extension_pipefd[2];
	struct sigaction    old_sigterm;
	CURL               *curl;
	CURLM              *curlm;
	struct curl_slist  *headers;
	pthread_t           thread;
	char                extension_id[LWS_EXTENSION_ID_CAP];
	char                error[CURL_ERROR_SIZE];
	unsigned            runtime_pipe_init:1;
	unsigned            extension_pipe_init:1;
	unsigned            signal_init:1;
	unsigned            thread_init:1;
	unsigned            curl_added:1;
} lws_shutdown_ctx_t;


static void lws_shutdown_handle_sigterm(int sig);
static int lws_shutdown_set_pipe_flags(int fd);
static void lws_shutdown_curl_reset(CURL *curl);
static size_t lws_shutdown_handle_header(char *buffer, size_t size, size_t nitems, void *userdata);
static size_t lws_shutdown_discard(char *buffer, size_t size, size_t nitems, void *userdata);
static void lws_shutdown_notify(int fd, char notification);
static void lws_shutdown_fail(const char *message);
static void *lws_shutdown_thread(void *data);


static lws_shutdown_ctx_t lws_shutdown_ctx;
static atomic_int lws_shutdown_requested;
static atomic_int lws_shutdown_failed;
static atomic_int lws_shutdown_stopping;


_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "shutdown request atomic must be lock-free");


static void lws_shutdown_handle_sigterm (int sig) {
	int   saved_errno;
	char  notification;

	(void)sig;
	saved_errno = errno;
	atomic_store_explicit(&lws_shutdown_requested, 1, memory_order_release);
	notification = 'T';
	(void)write(lws_shutdown_ctx.runtime_pipefd[1], &notification, 1);
	errno = saved_errno;
}

static int lws_shutdown_set_pipe_flags (int fd) {
	int  flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		return -1;
	}
	flags = fcntl(fd, F_GETFD);
	if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		return -1;
	}
	return 0;
}

static void lws_shutdown_curl_reset (CURL *curl) {
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
	curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, LWS_USERAGENT);
}

static size_t lws_shutdown_handle_header (char *buffer, size_t size, size_t nitems,
		void *userdata) {
	char                *colon, *value;
	size_t               len, name_len, value_len;
	lws_shutdown_ctx_t  *ctx;

	ctx = userdata;
	if (nitems && size > SIZE_MAX / nitems) {
		return 0;
	}
	len = size * nitems;
	colon = memchr(buffer, ':', len);
	if (!colon) {
		return len;
	}
	name_len = (size_t)(colon - buffer);
	if (name_len != sizeof(LWS_EXTENSION_ID) - 1 || lws_strncasecmp(buffer, LWS_EXTENSION_ID,
			name_len) != 0) {
		return len;
	}
	value = colon + 1;
	while (value < buffer + len && (*value == ' ' || *value == '\t')) {
		value++;
	}
	value_len = (size_t)(buffer + len - value);
	while (value_len && (value[value_len - 1] == ' ' || value[value_len - 1] == '\t' ||
			value[value_len - 1] == '\r' || value[value_len - 1] == '\n')) {
		value_len--;
	}
	if (!value_len || value_len >= sizeof(ctx->extension_id)) {
		return 0;
	}
	memcpy(ctx->extension_id, value, value_len);
	ctx->extension_id[value_len] = '\0';
	return len;
}

static size_t lws_shutdown_discard (char *buffer, size_t size, size_t nitems, void *userdata) {
	(void)buffer;
	(void)userdata;
	if (nitems && size > SIZE_MAX / nitems) {
		return 0;
	}
	return size * nitems;
}

static void lws_shutdown_notify (int fd, char notification) {
	(void)write(fd, &notification, 1);
}

static void lws_shutdown_fail (const char *message) {
	if (atomic_load_explicit(&lws_shutdown_requested, memory_order_acquire)
			|| atomic_load_explicit(&lws_shutdown_stopping, memory_order_acquire)) {
		return;
	}
	snprintf(lws_shutdown_ctx.error, sizeof(lws_shutdown_ctx.error), "%s", message);
	atomic_store_explicit(&lws_shutdown_failed, 1, memory_order_release);
	lws_shutdown_notify(lws_shutdown_ctx.runtime_pipefd[1], 'F');
}

static void *lws_shutdown_thread (void *data) {
	int                 remain;
	char                error[CURL_ERROR_SIZE];
	long                code;
	CURLMsg            *msg;
	CURLMcode           mres;
	struct curl_waitfd  waitfd;
	lws_shutdown_ctx_t *ctx;

	/* initialize extension poll */
	ctx = data;
	if (curl_multi_add_handle(ctx->curlm, ctx->curl) != CURLM_OK) {
		lws_shutdown_fail("failed to add internal extension handle to multi handle");
		return NULL;
	}
	ctx->curl_added = 1;
	waitfd.fd = ctx->extension_pipefd[0];
	waitfd.events = CURL_WAIT_POLLIN;
	waitfd.revents = 0;

	/* poll extension */
	while (!atomic_load_explicit(&lws_shutdown_stopping, memory_order_acquire)) {
		mres = curl_multi_perform(ctx->curlm, &remain);
		if (mres != CURLM_OK) {
			snprintf(error, sizeof(error), "internal extension multi error code:%d str:%s",
					mres, curl_multi_strerror(mres));
			lws_shutdown_fail(error);
			break;
		}
		if (!remain) {
			msg = curl_multi_info_read(ctx->curlm, &remain);
			if (msg && msg->msg == CURLMSG_DONE && msg->data.result != CURLE_OK) {
				snprintf(error, sizeof(error), "internal extension error code:%d str:%s",
						msg->data.result, curl_easy_strerror(msg->data.result));
				lws_shutdown_fail(error);
			} else if (msg && msg->msg == CURLMSG_DONE && curl_easy_getinfo(
				msg->easy_handle, CURLINFO_RESPONSE_CODE, &code) == CURLE_OK) {
				snprintf(error, sizeof(error),
						"internal extension poll completed unexpectedly HTTP status:%ld", code);
				lws_shutdown_fail(error);
			} else {
				lws_shutdown_fail("internal extension poll completed unexpectedly");
			}
			break;
		}
		mres = curl_multi_wait(ctx->curlm, &waitfd, 1, 1000, NULL);
		if (mres != CURLM_OK) {
			snprintf(error, sizeof(error), "internal extension wait error code:%d str:%s",
					mres, curl_multi_strerror(mres));
			lws_shutdown_fail(error);
			break;
		}
	}

	/* cleanup extension poll */
	if (curl_multi_remove_handle(ctx->curlm, ctx->curl) == CURLM_OK) {
		ctx->curl_added = 0;
	}
	return NULL;
}

int lws_shutdown_init (const lws_str_t *runtime_api) {
	int                 rc;
	char                extension_header[LWS_EXTENSION_ID_CAP + sizeof(LWS_EXTENSION_ID) + 2];
	char                extension_url[256];
	long                code;
	CURLcode            res;
	struct curl_slist  *headers;
	struct sigaction    action;
	lws_shutdown_ctx_t *ctx;

	/* initialize state */
	ctx = &lws_shutdown_ctx;
	lws_memzero(ctx, sizeof(*ctx));
	ctx->runtime_pipefd[0] = -1;
	ctx->runtime_pipefd[1] = -1;
	ctx->extension_pipefd[0] = -1;
	ctx->extension_pipefd[1] = -1;
	atomic_store_explicit(&lws_shutdown_requested, 0, memory_order_release);
	atomic_store_explicit(&lws_shutdown_failed, 0, memory_order_release);
	atomic_store_explicit(&lws_shutdown_stopping, 0, memory_order_release);

	/* initialize runtime pipe */
	if (pipe(ctx->runtime_pipefd) != 0) {
		lws_log(LWS_LOG_CRIT, "failed to initialize runtime shutdown pipe");
		return -1;
	}
	ctx->runtime_pipe_init = 1;
	if (lws_shutdown_set_pipe_flags(ctx->runtime_pipefd[0]) != 0 ||
			lws_shutdown_set_pipe_flags(ctx->runtime_pipefd[1]) != 0) {
		lws_log(LWS_LOG_CRIT, "failed to configure runtime shutdown pipe");
		return -1;
	}

	/* initialize extension pipe */
	if (pipe(ctx->extension_pipefd) != 0) {
		lws_log(LWS_LOG_CRIT, "failed to initialize extension shutdown pipe");
		return -1;
	}
	ctx->extension_pipe_init = 1;
	if (lws_shutdown_set_pipe_flags(ctx->extension_pipefd[0]) != 0 ||
			lws_shutdown_set_pipe_flags(ctx->extension_pipefd[1]) != 0) {
		lws_log(LWS_LOG_CRIT, "failed to configure extension shutdown pipe");
		return -1;
	}

	/* install signal handler */
	lws_memzero(&action, sizeof(action));
	action.sa_handler = lws_shutdown_handle_sigterm;
	if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGTERM, &action, &ctx->old_sigterm) != 0) {
		lws_log(LWS_LOG_CRIT, "failed to install SIGTERM handler");
		return -1;
	}
	ctx->signal_init = 1;

	/* initialize extension handles */
	ctx->curl = curl_easy_init();
	if (!ctx->curl) {
		lws_log(LWS_LOG_CRIT, "failed to create internal extension easy handle");
		return -1;
	}
	ctx->curlm = curl_multi_init();
	if (!ctx->curlm) {
		lws_log(LWS_LOG_CRIT, "failed to create internal extension multi handle");
		return -1;
	}

	/* register internal extension */
	headers = curl_slist_append(NULL, "Lambda-Extension-Name: " LWS_EXTENSION_NAME);
	if (!headers) {
		lws_log(LWS_LOG_CRIT, "out of memory");
		return -1;
	}
	ctx->headers = headers;
	headers = curl_slist_append(ctx->headers, "Content-Type: application/json");
	if (!headers) {
		lws_log(LWS_LOG_CRIT, "out of memory");
		return -1;
	}
	ctx->headers = headers;
	snprintf(extension_url, sizeof(extension_url), "http://%.*s/%s/extension/register",
			(int)runtime_api->len, runtime_api->data, LWS_EXTENSION_VERSION);
	lws_shutdown_curl_reset(ctx->curl);
	ctx->error[0] = '\0';
	curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->error);
	curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, extension_url);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, ctx->headers);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, "{\"events\":[]}");
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, 13L);
	curl_easy_setopt(ctx->curl, CURLOPT_HEADERFUNCTION, lws_shutdown_handle_header);
	curl_easy_setopt(ctx->curl, CURLOPT_HEADERDATA, ctx);
	curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, lws_shutdown_discard);
	res = curl_easy_perform(ctx->curl);
	if (res != CURLE_OK) {
		lws_log(LWS_LOG_CRIT, "failed to register internal extension code:%d str:%s", res,
				ctx->error[0] ? ctx->error : curl_easy_strerror(res));
		return -1;
	}
	if (curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &code) != CURLE_OK) {
		lws_log(LWS_LOG_CRIT, "failed to get internal extension HTTP status code");
		return -1;
	}
	if (code < 200 || code >= 300) {
		lws_log(LWS_LOG_CRIT, "failed to register internal extension HTTP status:%ld", code);
		return -1;
	}
	if (!ctx->extension_id[0]) {
		lws_log(LWS_LOG_CRIT, "internal extension identifier missing");
		return -1;
	}
	curl_slist_free_all(ctx->headers);
	ctx->headers = NULL;

	/* prepare extension poll */
	rc = snprintf(extension_header, sizeof(extension_header), "%s: %s", LWS_EXTENSION_ID,
			ctx->extension_id);
	if (rc < 0 || (size_t)rc >= sizeof(extension_header)) {
		lws_log(LWS_LOG_CRIT, "internal extension identifier too long");
		return -1;
	}
	ctx->headers = curl_slist_append(NULL, extension_header);
	if (!ctx->headers) {
		lws_log(LWS_LOG_CRIT, "out of memory");
		return -1;
	}
	snprintf(extension_url, sizeof(extension_url), "http://%.*s/%s/extension/event/next",
			(int)runtime_api->len, runtime_api->data, LWS_EXTENSION_VERSION);
	lws_shutdown_curl_reset(ctx->curl);
	ctx->error[0] = '\0';
	curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->error);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_URL, extension_url);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, ctx->headers);
	curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, lws_shutdown_discard);

	/* start extension poll */
	rc = pthread_create(&ctx->thread, NULL, lws_shutdown_thread, ctx);
	if (rc != 0) {
		lws_log(LWS_LOG_CRIT, "failed to create internal extension thread error:%d", rc);
		return -1;
	}
	ctx->thread_init = 1;
	return 0;
}

int lws_shutdown_runtime_fd () {
	return lws_shutdown_ctx.runtime_pipefd[0];
}

lws_shutdown_status_e lws_shutdown_status () {
	if (atomic_load_explicit(&lws_shutdown_failed, memory_order_acquire)) {
		return LWS_SHUTDOWN_FAILURE;
	}
	if (atomic_load_explicit(&lws_shutdown_requested, memory_order_acquire)) {
		return LWS_SHUTDOWN_TERM;
	}
	return LWS_SHUTDOWN_NONE;
}

const char *lws_shutdown_error () {
	return lws_shutdown_ctx.error;
}

int lws_shutdown_cleanup () {
	int                 rc;
	lws_shutdown_ctx_t *ctx;

	ctx = &lws_shutdown_ctx;
	if (ctx->thread_init) {
		atomic_store_explicit(&lws_shutdown_stopping, 1, memory_order_release);
		lws_shutdown_notify(ctx->extension_pipefd[1], 'S');
		rc = pthread_join(ctx->thread, NULL);
		if (rc != 0) {
			lws_log(LWS_LOG_CRIT, "failed to join internal extension thread error:%d", rc);
			return -1;
		}
		ctx->thread_init = 0;
	}
	if (ctx->curl_added) {
		if (curl_multi_remove_handle(ctx->curlm, ctx->curl) != CURLM_OK) {
			lws_log(LWS_LOG_CRIT, "failed to remove internal extension handle");
			return -1;
		}
		ctx->curl_added = 0;
	}
	if (ctx->curl) {
		curl_easy_cleanup(ctx->curl);
		ctx->curl = NULL;
	}
	if (ctx->curlm) {
		curl_multi_cleanup(ctx->curlm);
		ctx->curlm = NULL;
	}
	if (ctx->headers) {
		curl_slist_free_all(ctx->headers);
		ctx->headers = NULL;
	}
	if (ctx->signal_init) {
		if (sigaction(SIGTERM, &ctx->old_sigterm, NULL) != 0) {
			lws_log(LWS_LOG_CRIT, "failed to restore SIGTERM handler");
			return -1;
		}
		ctx->signal_init = 0;
	}
	if (ctx->extension_pipe_init) {
		close(ctx->extension_pipefd[0]);
		close(ctx->extension_pipefd[1]);
		ctx->extension_pipefd[0] = -1;
		ctx->extension_pipefd[1] = -1;
		ctx->extension_pipe_init = 0;
	}
	if (ctx->runtime_pipe_init) {
		close(ctx->runtime_pipefd[0]);
		close(ctx->runtime_pipefd[1]);
		ctx->runtime_pipefd[0] = -1;
		ctx->runtime_pipefd[1] = -1;
		ctx->runtime_pipe_init = 0;
	}
	return 0;
}
