/*
 * LWS request
 *
 * Copyright (C) 2025 Andre Naef
 */


#ifndef _LWS_REQUEST_INCLUDED
#define _LWS_REQUEST_INCLUDED


#include <lws_runtime.h>


int lws_handle_request(lws_ctx_t *ctx);
int lws_error_response(lws_ctx_t *ctx, int code);


#endif /* _LWS_REQUEST_INCLUDED */
