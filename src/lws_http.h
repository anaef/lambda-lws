/*
 * LWS HTTP
 *
 * Copyright (C) 2023,2025 Andre Naef
 */


#ifndef _LWS_HTTP_INCLUDED
#define _LWS_HTTP_INCLUDED


#include <lws_ngx.h>


typedef struct lws_http_status_s {
	const int   code;
	lws_str_t   key;
	lws_str_t   message;
} lws_http_status_t;


lws_http_status_t *lws_find_http_status(int code);


extern lws_http_status_t lws_http_status[];
extern const int lws_http_status_n;


#endif /* _LWS_HTTP_INCLUDED */
