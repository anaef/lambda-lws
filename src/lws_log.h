/*
 * LWS log
 *
 * Copyright (C) 2025 Andre Naef
 */


#ifndef _LWS_LOG_INCLUDED
#define _LWS_LOG_INCLUDED


#include <stdio.h>
#include <yyjson.h>


typedef enum {
	LWS_LOG_EMERG,
	LWS_LOG_ALERT,
	LWS_LOG_CRIT,
	LWS_LOG_ERR,
	LWS_LOG_WARN,
	LWS_LOG_NOTICE,
	LWS_LOG_INFO,
	LWS_LOG_DEBUG
} lws_log_level_e;


#include <lws_runtime.h>


extern lws_str_t lws_log_levels[];


void lws_log_setctx(lws_ctx_t *ctx);
void lws_log(lws_log_level_e level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#if LWS_DEBUG
#define lws_log_debug(fmt, ...)  lws_log(LWS_LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define lws_log_debug(fmt, ...)  ((void)0)
#endif


#endif  /* _LWS_LOG_INCLUDED */
