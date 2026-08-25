/*
 * LWS codec
 *
 * Copyright (C) 2025, 2026 Andre Naef
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
static const char b64_enc_tbl[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


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
	uint8_t  c;

	while (n > 0) {
		c = *p;
		if (c < 0x80) {
			p++;
			n--;
			continue;
		}
		if (c >= 0xc2 && c <= 0xdf) {
			if (__builtin_expect(n < 2 || (p[1] & 0xc0) != 0x80, 0)) {
				return -1;
			}
			p += 2;
			n -= 2;
		} else if (c >= 0xe0 && c <= 0xef) {
			if (__builtin_expect(n < 3 || (p[1] & 0xc0) != 0x80
					|| (p[2] & 0xc0) != 0x80
					|| (c == 0xe0 && p[1] < 0xa0)
					|| (c == 0xed && p[1] >= 0xa0), 0)) {
				return -1;
			}
			p += 3;
			n -= 3;
		} else if (c >= 0xf0 && c <= 0xf4) {
			if (__builtin_expect(n < 4 || (p[1] & 0xc0) != 0x80
					|| (p[2] & 0xc0) != 0x80
					|| (p[3] & 0xc0) != 0x80
					|| (c == 0xf0 && p[1] < 0x90)
					|| (c == 0xf4 && p[1] >= 0x90), 0)) {
				return -1;
			}
			p += 4;
			n -= 4;
		} else {
			return -1;
		}
	}
	return 0;
}
