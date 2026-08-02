/*
 * LWS shutdown
 *
 * Copyright (C) 2026 Andre Naef
 */


#ifndef _LWS_SHUTDOWN_INCLUDED
#define _LWS_SHUTDOWN_INCLUDED


#include <lws_ngx.h>


typedef enum {
	LWS_SHUTDOWN_NONE,
	LWS_SHUTDOWN_TERM,
	LWS_SHUTDOWN_FAILURE
} lws_shutdown_status_e;


int lws_shutdown_init(const lws_str_t *runtime_api);
int lws_shutdown_runtime_fd(void);
lws_shutdown_status_e lws_shutdown_status(void);
const char *lws_shutdown_error(void);
int lws_shutdown_cleanup(void);


#endif  /* _LWS_SHUTDOWN_INCLUDED */
