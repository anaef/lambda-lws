/*
 * LWS codec tests
 *
 * Copyright (C) 2025, 2026 Andre Naef
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <lws_codec.h>


static void test_base64_encode_decode_1_block(void);
static void test_base64_encode_decode_2_blocks(void);
static void test_base64_decode_errors(void);
static void test_base64_encode_len(void);
static void test_utf8(void);
int main(void);


static void test_base64_encode_decode_1_block (void) {
	size_t   len;
	uint8_t  buf[32];

	/* 1-byte (== padding) */
	memset(buf, 0, sizeof(buf));
	memcpy(buf, "f", 1);
	len = 1;
	lws_base64_encode(buf, &len);
	assert(len == 4);
	assert(memcmp(buf, "Zg==", 4) == 0);
	len = 4;
	assert(lws_base64_decode(buf, &len) == 0);
	assert(len == 1);
	assert(memcmp(buf, "f", 1) == 0);

	/* 2-byte (= padding) */
	memset(buf, 0, sizeof(buf));
	memcpy(buf, "fo", 2);
	len = 2;
	lws_base64_encode(buf, &len);
	assert(len == 4);
	assert(memcmp(buf, "Zm8=", 4) == 0);
	len = 4;
	assert(lws_base64_decode(buf, &len) == 0);
	assert(len == 2);
	assert(memcmp(buf, "fo", 2) == 0);

	/* 3-byte (no padding) */
	memcpy(buf, "foo", 3);
	len = 3;
	lws_base64_encode(buf, &len);
	assert(len == 4);
	assert(memcmp(buf, "Zm9v", 4) == 0);
	len = 4;
	assert(lws_base64_decode(buf, &len) == 0);
	assert(len == 3);
	assert(memcmp(buf, "foo", 3) == 0);
}

static void test_base64_encode_decode_2_blocks (void) {
	size_t   len;
	uint8_t  buf[64];

	/* 6-byte (no padding) */
	memcpy(buf, "foobar", 6);
	len = 6;
	lws_base64_encode(buf, &len);
	assert(len == 8);
	assert(memcmp(buf, "Zm9vYmFy", 8) == 0);
	len = 8;
	assert(lws_base64_decode(buf, &len) == 0);
	assert(len == 6);
	assert(memcmp(buf, "foobar", 6) == 0);

	/* 5-byte (final block padded) */
	memset(buf, 0, sizeof(buf));
	memcpy(buf, "hello", 5);
	len = 5;
	lws_base64_encode(buf, &len);
	assert(len == 8);
	assert(memcmp(buf, "aGVsbG8=", 8) == 0);
	len = 8;
	assert(lws_base64_decode(buf, &len) == 0);
	assert(len == 5);
	assert(memcmp(buf, "hello", 5) == 0);
}

static void test_base64_decode_errors (void) {
	uint8_t  buf[16];
	size_t   len;

	/* invalid length (not multiple of 4) */
	memcpy(buf, "abcde", 5);
	len = 5;
	assert(lws_base64_decode(buf, &len) == -1);

	/* invalid character */
	memcpy(buf, "!!!!", 4);
	len = 4;
	assert(lws_base64_decode(buf, &len) == -1);
}

static void test_base64_encode_len (void) {
	size_t  out;

	assert(lws_base64_encode_len(0, &out) == 0 && out == 0);
	assert(lws_base64_encode_len(1, &out) == 0 && out == 4);
	assert(lws_base64_encode_len(2, &out) == 0 && out == 4);
	assert(lws_base64_encode_len(3, &out) == 0 && out == 4);
	assert(lws_base64_encode_len(4, &out) == 0 && out == 8);
	assert(lws_base64_encode_len(5, &out) == 0 && out == 8);
	assert(lws_base64_encode_len(6, &out) == 0 && out == 8);
}

static void test_utf8 (void) {
	/* valid empty sequence */
	{
		uint8_t empty[] = {0};
		assert(lws_valid_utf8(empty, 0) == 0);
	}

	/* valid ascii */
	assert(lws_valid_utf8((const uint8_t *)"hello", 5) == 0);

	/* valid minimum 2-byte sequence (U+0080) */
	{
		uint8_t minimum[] = {0xC2,0x80};
		assert(lws_valid_utf8(minimum, sizeof(minimum)) == 0);
	}

	/* valid maximum 2-byte sequence (U+07FF) */
	{
		uint8_t maximum[] = {0xDF,0xBF};
		assert(lws_valid_utf8(maximum, sizeof(maximum)) == 0);
	}

	/* valid 3-byte sequence (Euro sign) */
	{
		uint8_t euro[] = {0xE2,0x82,0xAC};
		assert(lws_valid_utf8(euro, sizeof(euro)) == 0);
	}

	/* valid minimum 3-byte sequence (U+0800) */
	{
		uint8_t minimum[] = {0xE0,0xA0,0x80};
		assert(lws_valid_utf8(minimum, sizeof(minimum)) == 0);
	}

	/* valid sequence before surrogate range (U+D7FF) */
	{
		uint8_t before[] = {0xED,0x9F,0xBF};
		assert(lws_valid_utf8(before, sizeof(before)) == 0);
	}

	/* valid sequence after surrogate range (U+E000) */
	{
		uint8_t after[] = {0xEE,0x80,0x80};
		assert(lws_valid_utf8(after, sizeof(after)) == 0);
	}

	/* valid maximum 3-byte sequence (U+FFFF) */
	{
		uint8_t maximum[] = {0xEF,0xBF,0xBF};
		assert(lws_valid_utf8(maximum, sizeof(maximum)) == 0);
	}

	/* valid minimum 4-byte sequence (U+10000) */
	{
		uint8_t minimum[] = {0xF0,0x90,0x80,0x80};
		assert(lws_valid_utf8(minimum, sizeof(minimum)) == 0);
	}

	/* valid 4-byte sequence (grinning face U+1F600) */
	{
		uint8_t smile[] = {0xF0,0x9F,0x98,0x80};
		assert(lws_valid_utf8(smile, sizeof(smile)) == 0);
	}

	/* valid maximum 4-byte sequence (U+10FFFF) */
	{
		uint8_t maximum[] = {0xF4,0x8F,0xBF,0xBF};
		assert(lws_valid_utf8(maximum, sizeof(maximum)) == 0);
	}

	/* invalid stray continuation byte */
	{
		uint8_t bad[] = {0x80};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid overlong 2-byte sequence */
	{
		uint8_t bad[] = {0xC0,0x80};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid overlong 3-byte sequence */
	{
		uint8_t bad[] = {0xE0,0x9F,0xBF};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid surrogate */
	{
		uint8_t bad[] = {0xED,0xA0,0x80};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid overlong 4-byte sequence */
	{
		uint8_t bad[] = {0xF0,0x8F,0xBF,0xBF};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid sequence above U+10FFFF */
	{
		uint8_t bad[] = {0xF4,0x90,0x80,0x80};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid continuation byte */
	{
		uint8_t bad[] = {0xE2,0x28,0xA1};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid truncated 2-byte sequence */
	{
		uint8_t bad[] = {0xC2};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid truncated 3-byte sequence */
	{
		uint8_t bad[] = {0xE0,0xA0};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}

	/* invalid truncated 4-byte sequence */
	{
		uint8_t bad[] = {0xF0,0x90,0x80};
		assert(lws_valid_utf8(bad, sizeof(bad)) == -1);
	}
}

int main (void) {
	test_base64_encode_decode_1_block();
	test_base64_encode_decode_2_blocks();
	test_base64_encode_len();
	test_base64_decode_errors();
	test_utf8();
	return EXIT_SUCCESS;
}
