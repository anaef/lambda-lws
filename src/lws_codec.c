/*
 * LWS codec
 *
 * Copyright (C) 2025 Andre Naef
 */


#include <stddef.h>
#include <stdint.h>
#include <lws_codec.h>


#ifndef __has_builtin
#define __has_builtin(x)  0
#endif
#if !__has_builtin(__builtin_expect)
#define __builtin_expect(x, y)  (x)
#endif

#define LWS_UTF8_ACCEPT  0
#define LWS_UTF8_REJECT  12


static const uint8_t b64_dec_tbl[256] = {
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, /* 0x00-0x0F */ 
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, /* 0x10-0x1F */ 
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,62  ,0x80,0x80,0x80,63  , /* 0x20-0x2F */ 
	52  ,53  ,54  ,55  ,56  ,57  ,58  ,59  ,60  ,61  ,0x80,0x80,0x80,0x80,0x80,0x80, /* 0x30-0x3F */
	0x80,0   ,1   ,2   ,3   ,4   ,5   ,6   ,7   ,8   ,9   ,10  ,11  ,12  ,13  ,14  , /* 0x40-0x4F */ 
	15  ,16  ,17  ,18  ,19  ,20  ,21  ,22  ,23  ,24  ,25  ,0x80,0x80,0x80,0x80,0x80, /* 0x50-0x5F */ 
	0x80,26  ,27  ,28  ,29  ,30  ,31  ,32  ,33  ,34  ,35  ,36  ,37  ,38  ,39  ,40  , /* 0x60-0x6F */ 
	41  ,42  ,43  ,44  ,45  ,46  ,47  ,48  ,49  ,50  ,51  ,0x80,0x80,0x80,0x80,0x80, /* 0x70-0x7F */ 
	[0x80 ... 0xFF] = 0x80                                                           /* 0x80-0xFF */ 
};
static const char b64_enc_tbl[64] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
static const uint8_t utf8d[] = {
	// The first part of the table maps bytes to character classes that
	// to reduce the size of the transition table and create bitmasks.
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

	// The second part is a transition table that maps a combination
	// of a state of the automaton and a character class to a state.
	0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
	12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
	12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
	12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
	12,36,12,12,12,12,12,12,12,12,12,12, 
};


int lws_base64_decode (uint8_t *in_out, size_t *in_out_len) {
	size_t   len, in, out, blocks, i;
	uint8_t  a, b, c, d, va, vb, vc, vd;

	len = *in_out_len;
	if (len == 0) {
		return 0;
	}
	if (len & 3) {
		return -1;
	}
	blocks = len / 4;

	in = 0;
	out = 0;

	/* leading blocks */
	if (blocks > 1) {
		for (i = 0; i < blocks - 1; i++) {
			a  = in_out[in++]; b = in_out[in++]; c = in_out[in++]; d = in_out[in++];
			va = b64_dec_tbl[a]; vb = b64_dec_tbl[b]; vc = b64_dec_tbl[c]; vd = b64_dec_tbl[d];
			if (__builtin_expect((va | vb | vc | vd) & 0x80, 0)) {
				return -1;
			}
			in_out[out++] = (uint8_t)((va << 2) | (vb >> 4));
			in_out[out++] = (uint8_t)((vb << 4) | (vc >> 2));
			in_out[out++] = (uint8_t)((vc << 6) | vd);
		}
	}

	/* final block */
	a  = in_out[in++]; b = in_out[in++]; c = in_out[in++]; d = in_out[in++];
	va = b64_dec_tbl[a]; vb = b64_dec_tbl[b];
	if (__builtin_expect(va & 0x80 || vb & 0x80, 0)) {
		return -1;
	}
	if (c == '=') {
		/* "xx==" -> 1 byte */
		if (d != '=' || in != len) {
			return -1;
		}
		in_out[out++] = (uint8_t)((va << 2) | (vb >> 4));
	} else {
		vc = b64_dec_tbl[c];
		if (__builtin_expect(vc & 0x80, 0)) {
			return -1;
		}
		if (d == '=') {
			/* "xxx=" -> 2 bytes */
			if (in != len) {
				return -1;
			}
			in_out[out++] = (uint8_t)((va << 2) | (vb >> 4));
			in_out[out++] = (uint8_t)((vb << 4) | (vc >> 2));
		} else {
			vd = b64_dec_tbl[d];
			if (__builtin_expect(vd & 0x80, 0)) {
				return -1;
			}
			/* "xxxx" -> 3 bytes */
			in_out[out++] = (uint8_t)((va << 2) | (vb >> 4));
			in_out[out++] = (uint8_t)((vb << 4) | (vc >> 2));
			in_out[out++] = (uint8_t)((vc << 6) | vd);
		}
	}

	*in_out_len = out;
	return 0;
}

void lws_base64_encode (uint8_t *in_out, size_t *in_out_len) {
	size_t    len, full, rem, out, i_in, i_out;
	uint32_t  v;
	uint8_t   b0, b1, b2;

	len = *in_out_len;
	full = len / 3;
	rem  = len % 3;
	out  = full * 4 + (rem ? 4 : 0);

	i_in  = len;
	i_out = out;

	/* handle remainder first */
	if (rem == 1) {
		b0 = in_out[--i_in];
		in_out[--i_out] = '=';
		in_out[--i_out] = '=';
		in_out[--i_out] = b64_enc_tbl[(b0 & 0x03) << 4];
		in_out[--i_out] = b64_enc_tbl[(b0 >> 2) & 0x3F];
	} else if (rem == 2) {
		b1 = in_out[--i_in];
		b0 = in_out[--i_in];
		v  = ((uint32_t)b0 << 8) | b1;
		in_out[--i_out] = '=';
		in_out[--i_out] = b64_enc_tbl[(v << 2)  & 0x3F];
		in_out[--i_out] = b64_enc_tbl[(v >> 4)  & 0x3F];
		in_out[--i_out] = b64_enc_tbl[(v >> 10) & 0x3F];
	}

	/* process full 3-byte blocks */
	while (full--) {
		b2 = in_out[--i_in];
		b1 = in_out[--i_in];
		b0 = in_out[--i_in];
		v  = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;

		in_out[--i_out] = b64_enc_tbl[ v        & 0x3F];
		in_out[--i_out] = b64_enc_tbl[(v >> 6)  & 0x3F];
		in_out[--i_out] = b64_enc_tbl[(v >> 12) & 0x3F];
		in_out[--i_out] = b64_enc_tbl[(v >> 18) & 0x3F];
	}

	*in_out_len = out;
}

int lws_base64_encode_len (size_t in_len, size_t *out_len) {
	size_t  blocks;

	blocks = in_len / 3 + (in_len % 3 ? 1 : 0);
	if (blocks > SIZE_MAX / 4) {
		return -1;
	}
	*out_len = blocks * 4;
	return 0;
}

int lws_valid_utf8 (const uint8_t *p, size_t n) {
	size_t    i;
	uint32_t  state;

	state = LWS_UTF8_ACCEPT;
	for (i = 0; i < n; i++) {
		state = utf8d[256 + state + utf8d[p[i]]];
		if (state == LWS_UTF8_REJECT) {
			return -1;
		}
	}
	return state == LWS_UTF8_ACCEPT ? 0 : -1;
}
