/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>

/* UTF-8 "euro sign" in English, split inside the UTF-8 sequence. */
static const uint8_t chunked_text[] = {
	0xB1, 0x01, 0x04, 'T', 0x02, 'e', 'n', 0xE2, 0x56, 0x00, 0x02, 0x82, 0xAC,
};

ZTEST(ndef_validate, test_scratch_arguments)
{
	size_t len = 123U;
	uint32_t count = 456U;

	zassert_equal(ndef_msg_validate_buf(chunked_text, sizeof(chunked_text), 0U, NULL, 1U, &len,
					    &count),
		      -EINVAL);
	zassert_equal(len, 123U);
	zassert_equal(count, 456U);
}

ZTEST(ndef_validate, test_chunked_text_requires_scratch)
{
	size_t len = 123U;
	uint32_t count = 456U;
	int expected;

#if defined(CONFIG_ZNDEF_VALIDATE_RTD) && !defined(CONFIG_ZNDEF_RTD_TEXT)
	/* A disabled RTD has an opaque payload and needs no workspace. */
	expected = 0;
#else
	expected = -ENOTSUP;
#endif
	zassert_equal(ndef_msg_validate(chunked_text, sizeof(chunked_text), NDEF_VALIDATE_ALL, &len,
					&count),
		      expected);
	if (expected == 0) {
		zassert_equal(len, sizeof(chunked_text));
		zassert_equal(count, 1U);
	} else {
		zassert_equal(len, 123U);
		zassert_equal(count, 456U);
	}
}

ZTEST(ndef_validate, test_chunked_text_with_scratch)
{
	static const uint8_t payload[] = {0x02, 'e', 'n', 0xE2, 0x82, 0xAC};
	uint8_t scratch[sizeof(payload)];
	size_t len = 123U;
	uint32_t count = 456U;
	int ret;

	memset(scratch, 0, sizeof(scratch));
	ret = ndef_msg_validate_buf(chunked_text, sizeof(chunked_text), NDEF_VALIDATE_ALL, scratch,
				    sizeof(scratch), &len, &count);
#if defined(CONFIG_ZNDEF_VALIDATE_RTD)
	zassert_ok(ret);
	zassert_equal(len, sizeof(chunked_text));
	zassert_equal(count, 1U);
#if defined(CONFIG_ZNDEF_RTD_TEXT)
	zassert_mem_equal(scratch, payload, sizeof(payload));
#endif
#else
	zassert_equal(ret, -ENOTSUP);
	zassert_equal(len, 123U);
	zassert_equal(count, 456U);
#endif
}

ZTEST(ndef_validate, test_chunked_text_small_scratch)
{
	uint8_t scratch[5];
	size_t len = 123U;
	uint32_t count = 456U;
	int expected;

#if !defined(CONFIG_ZNDEF_VALIDATE_RTD)
	expected = -ENOTSUP;
#elif defined(CONFIG_ZNDEF_RTD_TEXT)
	expected = -ENOSPC;
#else
	expected = 0;
#endif
	zassert_equal(ndef_msg_validate_buf(chunked_text, sizeof(chunked_text), NDEF_VALIDATE_ALL,
					    scratch, sizeof(scratch), &len, &count),
		      expected);
	if (expected < 0) {
		zassert_equal(len, 123U);
		zassert_equal(count, 456U);
	}
}

ZTEST(ndef_validate, test_malformed_chunked_payload)
{
	uint8_t msg[sizeof(chunked_text)];
	uint8_t scratch[6];
	size_t len = 123U;
	uint32_t count = 456U;
	int expected;

	memcpy(msg, chunked_text, sizeof(msg));
	msg[sizeof(msg) - 1U] = 0xFF;

#if !defined(CONFIG_ZNDEF_VALIDATE_RTD)
	expected = -ENOTSUP;
#elif defined(CONFIG_ZNDEF_RTD_TEXT)
	expected = -EBADMSG;
#else
	expected = 0;
#endif
	zassert_equal(ndef_msg_validate_buf(msg, sizeof(msg), NDEF_VALIDATE_ALL, scratch,
					    sizeof(scratch), &len, &count),
		      expected);
	if (expected < 0) {
		zassert_equal(len, 123U);
		zassert_equal(count, 456U);
	}

	/* Structural validation is independent of payload validation and scratch. */
	zassert_ok(ndef_msg_validate(msg, sizeof(msg), NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT,
				     &len, &count));
	zassert_equal(len, sizeof(msg));
	zassert_equal(count, 1U);
	zassert_ok(ndef_msg_validate_buf(msg, sizeof(msg), 0U, NULL, 0U, NULL, NULL));
}

ZTEST(ndef_validate, test_opaque_chunked_payloads_need_no_scratch)
{
	static const uint8_t unknown_wkt[] = {
		0xB1, 0x01, 0x01, 'Z', 0x00, 0x56, 0x00, 0x01, 0xFF,
	};
	static const uint8_t media[] = {
		0xB2, 0x03, 0x01, 'a', '/', 'b', 0x00, 0x56, 0x00, 0x01, 0xFF,
	};
	int expected;

#if defined(CONFIG_ZNDEF_VALIDATE_RTD)
	expected = 0;
#else
	expected = -ENOTSUP;
#endif
	zassert_equal(
		ndef_msg_validate(unknown_wkt, sizeof(unknown_wkt), NDEF_VALIDATE_ALL, NULL, NULL),
		expected);
	zassert_equal(ndef_msg_validate_buf(unknown_wkt, sizeof(unknown_wkt), NDEF_VALIDATE_ALL,
					    NULL, 0U, NULL, NULL),
		      expected);
	zassert_equal(ndef_msg_validate(media, sizeof(media), NDEF_VALIDATE_ALL, NULL, NULL),
		      expected);
}

ZTEST(ndef_validate, test_scratch_reused_between_records)
{
	static const uint8_t msg[] = {
		0xB1, 0x01, 0x04, 'T',  0x02, 'e',  'n',  0xE2, 0x16, 0x00, 0x02, 0x82,
		0xAC, 0x31, 0x01, 0x01, 'U',  0x04, 0x56, 0x00, 0x03, 'a',  '.',  'b',
	};
	uint8_t scratch[6];
	size_t len;
	uint32_t count;
	int ret;

	ret = ndef_msg_validate_buf(msg, sizeof(msg), NDEF_VALIDATE_ALL, scratch, sizeof(scratch),
				    &len, &count);
#if defined(CONFIG_ZNDEF_VALIDATE_RTD)
	zassert_ok(ret);
	zassert_equal(len, sizeof(msg));
	zassert_equal(count, 2U);
#else
	zassert_equal(ret, -ENOTSUP);
#endif
}

ZTEST(ndef_validate, test_unsupported_rtd_versions)
{
	static const uint8_t signature[] = {0xD1, 0x03, 0x01, 'S', 'i', 'g', 0x30};
	static const uint8_t handover[] = {0xD1, 0x02, 0x01, 'H', 's', 0x20};
	uint8_t scratch[8];
	size_t len = 123U;
	uint32_t count = 456U;
	int expected_sig;
	int expected_ho;

#if !defined(CONFIG_ZNDEF_VALIDATE_RTD) || defined(CONFIG_ZNDEF_RTD_SIG)
	expected_sig = -ENOTSUP;
#else
	expected_sig = 0;
#endif
#if !defined(CONFIG_ZNDEF_VALIDATE_RTD) || defined(CONFIG_ZNDEF_RTD_HANDOVER)
	expected_ho = -ENOTSUP;
#else
	expected_ho = 0;
#endif
	zassert_equal(
		ndef_msg_validate(signature, sizeof(signature), NDEF_VALIDATE_ALL, &len, &count),
		expected_sig);
	if (expected_sig < 0) {
		zassert_equal(len, 123U);
		zassert_equal(count, 456U);
	}
	zassert_equal(ndef_msg_validate_buf(handover, sizeof(handover), NDEF_VALIDATE_ALL, scratch,
					    sizeof(scratch), NULL, NULL),
		      expected_ho);
	zassert_ok(ndef_msg_validate(signature, sizeof(signature), 0U, NULL, NULL));
	zassert_ok(ndef_msg_validate(handover, sizeof(handover), 0U, NULL, NULL));
}

ZTEST_SUITE(ndef_validate, NULL, NULL, NULL, NULL, NULL);
