/*
 * LWS runtime
 *
 * Copyright (C) 2025 Andre Naef
 */


#ifndef _LWS_RUNTIME_INCLUDED
#define _LWS_RUNTIME_INCLUDED


#include <regex.h>
#include <lua.h>
#include <curl/curl.h>
#include <yyjson.h>


#ifndef LWS_STAT_CACHE_CAP
#define LWS_STAT_CACHE_CAP  1024
#endif


typedef struct lws_ctx_s  lws_ctx_t;


#include <lws_ngx.h>
#include <lws_log.h>
#include <lws_table.h>


struct lws_ctx_s {
	/* configuration */
	lws_str_t             runtime_api;            /* Lambda runtime API URL */
	lws_str_t             task_root;              /* Lambda task root directory */
	regex_t               match;                  /* path matching regular expression  */
	lws_str_t             main;                   /* filename of main Lua chunk expression */
	lws_str_t             path_info;              /* path info expression */
	lws_str_t             init;                   /* filename of init Lua chunk */
	lws_str_t             pre;                    /* filename of pre Lua chunk */
	lws_str_t             post;                   /* filename of post Lua chunk */
	int                   raw;                    /* raw mode */
	size_t                state_gc;               /* Lua state explicite GC theshold; 0 = never */
	lws_int_t             state_req_max;          /* maximum Lua state requests; 0 = unlimited */
	int                   state_diagnostic;       /* include diagnostic w/ error response */
	lws_log_level_e       log_level;              /* log level */
	int                   log_text;               /* log in text format; default JSON */

	/* state */
	CURL 			     *curl;                   /* CURL handle */
	CURLM                *curlm;                  /* CURLM handle for streaming */
	lws_table_t          *stat_cache;             /* file stat cache to reduce syscalls */
	lua_State            *L;                      /* Lua state */
	lws_int_t             req_count;              /* requests served */
	unsigned              curl_global_init:1;     /* CURL global init done */
	unsigned              streaming_init:1;       /* streaming init done */
	unsigned              state_init:1;           /* Lua state initialized */
	unsigned              state_close:1;          /* Lua state is to be closed */ 

	/* Lambda request */
	lws_table_t          *headers;                /* request headers */
	lws_str_t            *request_id;             /* request ID */
	off_t                 content_length;         /* content length; -1 if not present or invalid */
	lws_str_t             body;                   /* request body */
	size_t                body_cap;               /* request body capacity */
	yyjson_doc           *doc;                    /* in-place parsed request body */

	/* payload request */
	lws_str_t             req_method;             /* request method */
	lws_str_t             req_path;               /* request path */
	lws_str_t             req_args;               /* request arguments */
	lws_str_t             req_ip;                 /* request IP address */
	lws_str_t             req_main;               /* filename of main Lua chunk */
	lws_str_t             req_path_info;          /* path info derived from path */
	lws_table_t          *req_headers;            /* request headers */
	lws_str_t             req_body;               /* request body */
	FILE                 *req_body_file;          /* request body file */

	/* payload response */
	int                   resp_status;            /* response status code */
	lws_table_t          *resp_headers;           /* response headers */
	FILE                 *resp_body_file;         /* response body file */
	lws_str_t             resp_body;              /* response body */
	size_t                resp_body_pos;		  /* response body position for streaming */
	size_t                resp_body_cap;          /* response body capacity */
	lws_str_t             diagnostic;             /* diagnostic information */
	lws_str_t             streaming_prelude;	  /* streaming prelude */
	size_t                streaming_prelude_pos;  /* streaming prelude position */
	unsigned              likely_utf8:1;          /* response body is likely valid UTF-8 */
	unsigned              streaming:1;            /* streaming response */
	unsigned              streaming_paused:1;	  /* streaming paused */
	unsigned              streaming_eof:1;        /* streaming complete */
	unsigned              streaming_separator:4;  /* streaming separator 0-bytes sent */
};


int main(int argc, char *argv[]);


#endif  /* _LWS_RUNTIME_INCLUDED */
