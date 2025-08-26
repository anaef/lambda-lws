/*
 * LWS codec
 *
 * Copyright (C) 2025 Andre Naef
 */


#ifndef _LWS_CODEC_INCLUDED
#define _LWS_CODEC_INCLUDED


#include <stddef.h>
#include <stdint.h>


int lws_base64_decode(uint8_t *in_out, size_t *in_out_len);
void lws_base64_encode(uint8_t *in_out, size_t *in_out_len);
int lws_base64_encode_len(size_t in_len, size_t *out_len);
int lws_valid_utf8(const uint8_t *p, size_t n);


#endif /* _LWS_CODEC_INCLUDED */
