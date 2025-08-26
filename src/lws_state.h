/*
 * LWS state
 *
 * Copyright (C) 2023,2025 Andre Naef
 */


#ifndef _LWS_STATE_INCLUDED
#define _LWS_STATE_INCLUDED


#include <lws_runtime.h>


void lws_close_state(lws_ctx_t *ctx);
int lws_acquire_state(lws_ctx_t *ctx);
void lws_release_state(lws_ctx_t *ctx);
int lws_run_state(lws_ctx_t *ctx);


#endif /* _LWS_STATE_INCLUDED */
