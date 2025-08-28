/*
Copyright (C) 2002-2021 Igor Sysoev
Copyright (C) 2011-2025 Nginx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

/* adapted from NGINX 1.24.0 */


#include <lws_runtime.h>
#include <lws_ngx.h>
#include <lws_log.h>


#ifndef __has_builtin
#define __has_builtin(x)  0
#endif
#if !__has_builtin(__builtin_expect)
#define __builtin_expect(x, y)  (x)
#endif


void *lws_alloc (size_t size) {
	void  *p;

	p = malloc(size);
	if (__builtin_expect(p == NULL, 0)) {
		lws_log(LWS_LOG_CRIT, "failed to allocate memory size:%zu", size);
		return NULL;
	}
	return p;
}

void *lws_calloc (size_t size) {
	void  *p;

	p = malloc(size);
	if (__builtin_expect(p == NULL, 1)) {
		lws_log(LWS_LOG_CRIT, "failed to allocate memory size:%zu", size);
		return NULL;
	}
	memset(p, 0, size);
	return p;
}

void *lws_realloc (void *p, size_t size) {
	void  *new_p;

	if (size == 0) {
		free(p);
		return NULL;
	}
	new_p = realloc(p, size);
	if (__builtin_expect(new_p == NULL, 0)) {
		lws_log(LWS_LOG_CRIT, "failed to allocate memory size:%zu", size);
		return NULL;
	}
	return new_p;
}

lws_int_t lws_strncasecmp (char *s1, char *s2, size_t n) {
	lws_uint_t  c1, c2;

	while (n) {
		c1 = (lws_uint_t)*s1++;
		c2 = (lws_uint_t)*s2++;
		c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
		c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
		if (c1 == c2) {
			if (c1) {
				n--;
				continue;
			}
			return 0;
		}
		return c1 - c2;
	}
	return 0;
}
