/*
 * LWS runtime interface
 *
 * Copyright (C) 2025, 2026 Andre Naef
 */


#ifndef _LWS_INTERFACE_INCLUDED
#define _LWS_INTERFACE_INCLUDED


#include <lws_runtime.h>


int lws_get_next_invocation(lws_ctx_t *ctx);
int lws_post_response(lws_ctx_t *ctx);
int lws_stream_response(lws_ctx_t *ctx, int finalize);
int lws_post_error(lws_ctx_t *ctx, const char *error_message);


#endif /* _LWS_INTERFACE_INCLUDED */
