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


#ifndef _LWS_NGX_INCLUDED
#define _LWS_NGX_INCLUDED


#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


typedef intptr_t   lws_int_t;
typedef uintptr_t  lws_uint_t;


#define lws_free(p)          free(p)
#define lws_memzero(buf, n)  (void)memset(buf, 0, n)

void *lws_alloc(size_t size);
void *lws_calloc(size_t size);
void *lws_realloc(void *p, size_t size);

typedef struct lws_str_s {
	size_t  len;
	char   *data;
} lws_str_t;

#define lws_tolower(c)          (char)((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define lws_strncmp(s1, s2, n)  strncmp(s1, s2, n)
#define lws_string(lit)         { sizeof(lit) - 1, (char *)lit }
#define lws_str_set(str, lit)   (str)->len = sizeof(lit) - 1; (str)->data = (char *)lit
#define lws_str_null(str)       (str)->len = 0; (str)->data = NULL

lws_int_t lws_strncasecmp(char *s1, char *s2, size_t n);


typedef struct lws_queue_s  lws_queue_t;
struct lws_queue_s {
	lws_queue_t  *prev;
	lws_queue_t  *next;
};

#define lws_queue_init(q)                               \
	(q)->prev = q;                                      \
	(q)->next = q

#define lws_queue_empty(h)                              \
	(h == (h)->prev)

#define lws_queue_insert_head(h, x)                     \
	(x)->next = (h)->next;                              \
	(x)->next->prev = x;                                \
	(x)->prev = h;                                      \
	(h)->next = x

#define lws_queue_insert_tail(h, x)                     \
	(x)->prev = (h)->prev;                              \
	(x)->prev->next = x;                                \
	(x)->next = h;                                      \
	(h)->prev = x

#define lws_queue_head(h)                               \
	(h)->next

#define lws_queue_last(h)                               \
	(h)->prev

#define lws_queue_sentinel(h)                           \
	(h)

#define lws_queue_next(q)                               \
	(q)->next

#define lws_queue_prev(q)                               \
	(q)->prev

#define lws_queue_remove(x)                             \
	(x)->next->prev = (x)->prev;                        \
	(x)->prev->next = (x)->next

#define lws_queue_split(h, q, n)                        \
	(n)->prev = (h)->prev;                              \
	(n)->prev->next = n;                                \
	(n)->next = q;                                      \
	(h)->prev = (q)->prev;                              \
	(h)->prev->next = h;                                \
	(q)->prev = n;

#define lws_queue_add(h, n)                             \
	(h)->prev->next = (n)->next;                        \
	(n)->next->prev = (h)->prev;                        \
	(h)->prev = (n)->prev;                              \
	(h)->prev->next = h;

#define lws_queue_data(q, type, link)                   \
	(type *)((char *) q - offsetof(type, link))


#endif  /* _LWS_NGX_INCLUDED */
