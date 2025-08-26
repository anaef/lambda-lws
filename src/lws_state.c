/*
 * LWS state
 *
 * Copyright (C) 2023,2025 Andre Naef
 */


#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lws_lib.h>
#include <lws_state.h>


#if LUA_VERSION_NUM < 502
#define LUA_OK  0
#endif


#if LUA_VERSION_NUM < 502
static void luaL_requiref(lua_State *L, const char *name, lua_CFunction openf, int glb);
#endif
static void *lws_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize);
static int lws_init(lua_State *L);
static int lws_create_state(lws_ctx_t *ctx);


#if LUA_VERSION_NUM < 502
static void luaL_requiref (lua_State *L, const char *name, lua_CFunction openf, int glb) {
	lua_pushcfunction(L, openf);
	lua_pushstring(L, name);
	lua_call(L, 1, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);
	if (glb) {
		lua_pushvalue(L, -1);
		lua_setglobal(L, name);
	}
}
#endif

static void *lws_lua_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

static int lws_init (lua_State *L) {
	/* open standard libraries */
	luaL_openlibs(L);

	/* open LWS library */
	luaL_requiref(L, LWS_LIB_NAME, lws_open_lws, 1);

	return 0;
}

static int lws_create_state (lws_ctx_t *ctx) {
	lws_str_t  msg;

	ctx->L = lua_newstate(lws_lua_alloc, NULL);
	if (!ctx->L) {
		lws_log(LWS_LOG_EMERG, "failed to create Lua state");
		return -1;
	}

	/* initialize Lua state */
	lua_pushcfunction(ctx->L, lws_init);
	if (lua_pcall(ctx->L, 0, 0, 0) != LUA_OK) {
		lws_get_msg(ctx->L, -1, &msg);
		lws_log(LWS_LOG_EMERG, "failed to initialize Lua state: %s", msg.data);
		lua_close(ctx->L);
		ctx->L = NULL;
		return -1;
	}

	/* push traceback */
	lua_pushcfunction(ctx->L, lws_traceback);

	lws_log(LWS_LOG_INFO, "%s state created L:%p", LUA_VERSION, ctx->L);
	return 0;
}

void lws_close_state (lws_ctx_t *ctx) {
	lua_close(ctx->L);
	lws_log(LWS_LOG_INFO, "%s state closed L:%p req_count:%ld", LUA_VERSION, ctx->L, 
			(long)ctx->req_count);
	ctx->L = NULL;
	ctx->req_count = 0;
	ctx->state_init = 0;
	ctx->state_close = 0;
}

int lws_acquire_state (lws_ctx_t *ctx) {
	if (!ctx->L) {
		if (lws_create_state(ctx) != 0) {
			return -1;
		}
	}
	return 0;
}

void lws_release_state (lws_ctx_t *ctx) {
	size_t  memory_used;

	/* close state? */
	ctx->req_count++;
	if (ctx->state_close || (ctx->state_req_max > 0 && ctx->req_count >= ctx->state_req_max)) {
		lws_close_state(ctx);
		return;
	}

	/* perform GC as needed */
	if (ctx->state_gc > 0) {
		memory_used = (size_t)lua_gc(ctx->L, LUA_GCCOUNT, 0) * 1024
				+ lua_gc(ctx->L, LUA_GCCOUNTB, 0);
		if (memory_used >= ctx->state_gc) {
			lws_log_debug("performing GC memory_used:%zu state_gc:%zu", memory_used,
					ctx->state_gc);
			lua_gc(ctx->L, LUA_GCCOLLECT, 0);
#ifndef NDEBUG
			memory_used = (size_t)lua_gc(ctx->L, LUA_GCCOUNT, 0) * 1024
					+ lua_gc(ctx->L, LUA_GCCOUNTB, 0);
			lws_log_debug("GC complete memory_used:%zu", memory_used);
#endif			
		}
	}
}

int lws_run_state (lws_ctx_t *ctx) {
	int        rc;
	lws_str_t  msg;

	/* prepare stack */
	lua_pushcfunction(ctx->L, lws_run);
	lua_pushlightuserdata(ctx->L, ctx);  /* [traceback, function, ctx] */

	/* call */
	if (lua_pcall(ctx->L, 1, 1, 1) == LUA_OK) {
		rc = lua_tointeger(ctx->L, -1);
	} else {
		/* set error result, mark for close */
		rc = -1;
		ctx->state_close = 1;

		/* log error */
		lws_get_msg(ctx->L, -1, &msg);
		lws_log(LWS_LOG_ERR, "%s error: %s", LUA_VERSION, msg.data);
		if (!ctx->state_diagnostic) {
			goto done;
		}

		/* store error message */
		ctx->diagnostic.data = lws_alloc(msg.len);
		if (!ctx->diagnostic.data) {
			goto done;
		}
		memcpy(ctx->diagnostic.data, msg.data, msg.len);
		ctx->diagnostic.len = msg.len;
	}  /* [traceback, result] */

	/* clear result */
	done:
	lua_pop(ctx->L, 1);  /* [traceback] */

	return rc;
}
