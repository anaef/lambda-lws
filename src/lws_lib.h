/*
 * LWS library
 *
 * Copyright (C) 2023,2025 Andre Naef
 */


#ifndef _LWS_LIBRARY_INCLUDED
#define _LWS_LIBRARY_INCLUDED


#include <lua.h>
#include <lws_runtime.h>


#define LWS_LIB_NAME             "lws"                      /* library name */
#define LWS_REQUEST_CTX          "lws.request_ctx"          /* request context metatable */
#define LWS_REQUEST_CTX_CURRENT  "lws.request_ctx_current"  /* current request context */
#define LWS_TABLE                "lws.table"                /* table metatable */
#define LWS_YYJSON_ARR           "lws.yyjson_arr"           /* yyjson array metatable */
#define LWS_YYJSON_OBJ           "lws.yyjson_obj"           /* yyjson object metatable */
#define LWS_YYJSON_OBJ_ITER      "lws.yyjson_obj_iter"      /* yyjson object iterator metatable */
#define LWS_RESPONSE             "lws.response"             /* response metatable */
#define LWS_CHUNKS               "lws.chunks"               /* loaded chunks */
#define LWS_FILE                 "lws.file"                 /* file environment (Lua 5.1) */


typedef struct lws_lua_request_ctx_s lws_lua_request_ctx_t;
typedef struct lws_lua_table_s lws_lua_table_t;
typedef struct lws_lua_yyjson_val_s lws_lua_yyjson_val_t;
typedef struct lws_lua_yyjson_obj_iter_s lws_lua_yyjson_obj_iter_t;

typedef enum {
	LWS_LC_INIT,
	LWS_LC_PRE,
	LWS_LC_MAIN,
	LWS_LC_POST
} lws_lua_chunk_e;

struct lws_lua_request_ctx_s {
	lws_ctx_t          *ctx;               /* request context */
	lws_lua_chunk_e     chunk;             /* current chunk */
	lws_lua_table_t    *response_headers;  /* response headers */
	unsigned            complete:1;        /* request is complete */
	unsigned            sealed:1;          /* response header is sealed */
};

struct lws_lua_table_s {
	lws_table_t  *t;           /* table */
	unsigned      readonly:1;  /* read-only access */
	unsigned      external:1;  /* managed externally */
};

struct lws_lua_yyjson_val_s {
	yyjson_val  *v;  /* yyjson value */
};

struct lws_lua_yyjson_obj_iter_s {
	yyjson_obj_iter  iter;  /* yyjson object iterator */
};


void lws_get_msg(lua_State *L, int index, lws_str_t *msg);
int lws_traceback(lua_State *L);
int lws_open_lws(lua_State *L);
int lws_run(lua_State *L);


#endif /* _LWS_LIBRARY_INCLUDED */
