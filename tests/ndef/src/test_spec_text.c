/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Independent vectors from NFCForum-TS-RTD_Text_1.0 (2006-07-24). */
#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>
#include <ndef/rtd_text.h>

/* Appendix A, Table 4; MB/ME are set for a one-record NDEF message. */
static const uint8_t text_example[] = {
	0xD1, 0x01, 0x10, 0x54, 0x02, 0x65, 0x6E, 0x48, 0x65, 0x6C,
	0x6C, 0x6F, 0x2C, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21,
};

ZTEST(ndef_spec_text, test_appendix_a_canonical_octets)
{
	uint8_t encoded[sizeof(text_example) + 3U];
	struct ndef_record rec;
	struct ndef_text text;
	struct ndef_writer writer;
	char converted[14];
	size_t len;

	zassert_ok(ndef_record_decode(text_example, sizeof(text_example), &rec));
	zassert_equal(rec.tnf, NDEF_TNF_WELL_KNOWN);
	zassert_equal(rec.type_len, 1U);
	zassert_equal(rec.type[0], 0x54);
	zassert_ok(ndef_text_parse(&rec, &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF8);
	zassert_equal(text.lang_len, 2U);
	zassert_mem_equal(text.lang, "en", 2U);
	zassert_equal(text.text_len, 13U);
	zassert_mem_equal(text.text, "Hello, world!", 13U);
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), &len));
	zassert_equal(len, 13U);
	zassert_str_equal(converted, "Hello, world!");

	zassert_ok(ndef_text_payload_encode(encoded, sizeof(encoded), NDEF_TEXT_UTF8, "en", 2U,
					    "Hello, world!", 13U, &len));
	zassert_equal(len, 16U);
	zassert_mem_equal(encoded, &text_example[4], 16U);
	zassert_ok(ndef_writer_init(&writer, encoded, sizeof(encoded)));
	zassert_ok(ndef_text_write(&writer, "en", "Hello, world!"));
	zassert_ok(ndef_writer_finish(&writer, &len));
	zassert_equal(len, sizeof(text_example));
	zassert_mem_equal(encoded, text_example, sizeof(text_example));
}

ZTEST(ndef_spec_text, test_status_rfu_is_ignored_and_never_written)
{
	/* Section 3.2.1, Table 3: bit 6 ignored on read, zero on write. */
	static const uint8_t utf8[] = {0x42, 'e', 'n', 'X'};
	static const uint8_t utf16[] = {0xC2, 'e', 'n', 0xFE, 0xFF, 0x00, 'X'};
	struct ndef_text text;
	uint8_t encoded[8];
	char converted[2];
	size_t len;

	zassert_ok(ndef_text_payload_decode(utf8, sizeof(utf8), &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF8);
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), NULL));
	zassert_str_equal(converted, "X");
	zassert_ok(ndef_text_payload_decode(utf16, sizeof(utf16), &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF16);
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), NULL));
	zassert_str_equal(converted, "X");
	zassert_ok(ndef_text_payload_encode(encoded, sizeof(encoded), NDEF_TEXT_UTF8, "en", 2U, "X",
					    1U, &len));
	zassert_equal(encoded[0], 0x02);
	zassert_ok(ndef_text_payload_encode(encoded, sizeof(encoded), NDEF_TEXT_UTF16, "en", 2U,
					    &utf16[3], 4U, &len));
	zassert_equal(encoded[0], 0x82);
}

ZTEST(ndef_spec_text, test_language_syntax_in_both_directions)
{
	/* Section 3.3 references RFC 3066; these test its ABNF, not IANA registration. */
	static const char *const valid[] = {
		"en", "en-US", "fr-CA", "i-klingon", "x-private", "de-1996", "zh-Hant-TW",
	};
	static const char *const invalid[] = {
		"",       "en_US",     "en-",          "-en",   "en--US",
		"1en",    "abcdefghi", "en-123456789", "en US", "en.US",
		"en\x7F", "e\xC3\xA9", "e\x01",
	};
	uint8_t payload[80];
	struct ndef_text text;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(valid); i++) {
		size_t n = strlen(valid[i]);

		payload[0] = (uint8_t)n;
		memcpy(&payload[1], valid[i], n);
		payload[n + 1U] = 'X';
		zassert_ok(ndef_text_payload_decode(payload, n + 2U, &text), "%s", valid[i]);
		zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8,
						    valid[i], n, "X", 1U, &len));
	}
	for (size_t i = 0U; i < ARRAY_SIZE(invalid); i++) {
		size_t n = strlen(invalid[i]);

		payload[0] = (uint8_t)n;
		memcpy(&payload[1], invalid[i], n);
		zassert_equal(ndef_text_payload_decode(payload, n + 1U, &text), -EBADMSG,
			      "invalid language case %zu", i);
		zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8,
						       invalid[i], n, "", 0U, &len),
			      -EINVAL, "invalid language case %zu", i);
	}
	/* Explicit embedded NUL: strlen must not hide the invalid third byte. */
	payload[0] = 3U;
	memcpy(&payload[1], "e\0n", 3U);
	zassert_equal(ndef_text_payload_decode(payload, 4U, &text), -EBADMSG);
	zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8, "e\0n", 3U,
					       "", 0U, &len),
		      -EINVAL);
}

ZTEST(ndef_spec_text, test_language_length_and_payload_bounds)
{
	uint8_t payload[66];
	char language[64];
	struct ndef_text text;
	size_t len;

	/* Seven-byte subtags separated by hyphens; exercise all six length bits. */
	for (size_t i = 0U; i < sizeof(language); i++) {
		language[i] = ((i % 8U) == 7U) ? '-' : 'a';
	}
	zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8, language, 63U,
					    NULL, 0U, &len));
	zassert_equal(payload[0], 0x3F);
	zassert_equal(len, 64U);
	zassert_ok(ndef_text_payload_decode(payload, len, &text));
	zassert_equal(text.lang_len, 63U);
	zassert_equal(text.text_len, 0U);
	zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8, language,
					       64U, NULL, 0U, &len),
		      -EINVAL);

	payload[0] = 2U;
	payload[1] = 'e';
	payload[2] = 'n';
	for (size_t n = 0U; n < 3U; n++) {
		zassert_equal(ndef_text_payload_decode(payload, n, &text), -EBADMSG);
	}
	zassert_ok(ndef_text_payload_decode(payload, 3U, &text));
	zassert_equal(text.text_len, 0U);
	zassert_equal(
		ndef_text_payload_encode(payload, 2U, NDEF_TEXT_UTF8, "en", 2U, NULL, 0U, &len),
		-ENOSPC);
	zassert_ok(ndef_text_payload_encode(payload, 3U, NDEF_TEXT_UTF8, "en", 2U, NULL, 0U, &len));
}

ZTEST(ndef_spec_text, test_utf8_scalar_boundaries_and_octet_length)
{
	/* Unicode scalar boundaries, including NUL; length is bytes, not characters. */
	static const uint8_t scalars[] = {
		0x00, 0x7F, 0xC2, 0x80, 0xDF, 0xBF, 0xE0, 0xA0, 0x80, 0xED, 0x9F, 0xBF, 0xEE,
		0x80, 0x80, 0xEF, 0xBF, 0xBF, 0xF0, 0x90, 0x80, 0x80, 0xF4, 0x8F, 0xBF, 0xBF,
	};
	uint8_t payload[3U + sizeof(scalars)];
	char converted[sizeof(scalars) + 1U];
	struct ndef_text text;
	size_t len;

	payload[0] = 2U;
	payload[1] = 'e';
	payload[2] = 'n';
	memcpy(&payload[3], scalars, sizeof(scalars));
	zassert_ok(ndef_text_payload_decode(payload, sizeof(payload), &text));
	zassert_equal(text.text_len, sizeof(scalars));
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), &len));
	zassert_equal(len, sizeof(scalars));
	zassert_mem_equal(converted, scalars, sizeof(scalars));
	zassert_equal(converted[sizeof(scalars)], '\0');
	zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8, "en", 2U,
					    scalars, sizeof(scalars), &len));
	zassert_mem_equal(&payload[3], scalars, sizeof(scalars));
}

ZTEST(ndef_spec_text, test_invalid_utf8_is_rejected)
{
	static const struct {
		uint8_t bytes[4];
		size_t len;
	} cases[] = {
		{{0x80}, 1U},
		{{0xC0, 0xAF}, 2U},
		{{0xC1, 0x80}, 2U},
		{{0xE0, 0x80, 0x80}, 3U},
		{{0xED, 0xA0, 0x80}, 3U},
		{{0xF0, 0x80, 0x80, 0x80}, 4U},
		{{0xF4, 0x90, 0x80, 0x80}, 4U},
		{{0xF5, 0x80, 0x80, 0x80}, 4U},
		{{0xFF}, 1U},
		{{0xC2}, 1U},
		{{0xE2, 0x82}, 2U},
		{{0xF0, 0x90, 0x80}, 3U},
		{{0xE2, 0x28, 0xA1}, 3U},
	};
	uint8_t payload[8];
	char converted[8];
	struct ndef_text text;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		payload[0] = 2U;
		payload[1] = 'e';
		payload[2] = 'n';
		memcpy(&payload[3], cases[i].bytes, cases[i].len);
		zassert_equal(ndef_text_payload_decode(payload, 3U + cases[i].len, &text), -EBADMSG,
			      "UTF-8 case %zu", i);
		zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8,
						       "en", 2U, cases[i].bytes, cases[i].len,
						       &len),
			      -EINVAL, "UTF-8 case %zu", i);
		text.encoding = NDEF_TEXT_UTF8;
		text.text = cases[i].bytes;
		text.text_len = cases[i].len;
		zassert_equal(ndef_text_to_utf8(&text, converted, sizeof(converted), NULL),
			      -EBADMSG, "UTF-8 case %zu", i);
	}
}

ZTEST(ndef_spec_text, test_utf16_bom_endianness_and_surrogate_boundaries)
{
	/* Section 3.4: BOM tolerated; absent BOM defaults to BE. */
	static const uint8_t be[] = {0x00, 'A',  0x20, 0xAC, 0xD8, 0x00,
				     0xDC, 0x00, 0xDB, 0xFF, 0xDF, 0xFF};
	static const uint8_t be_bom[] = {0xFE, 0xFF, 0x00, 'A',  0x20, 0xAC, 0xD8,
					 0x00, 0xDC, 0x00, 0xDB, 0xFF, 0xDF, 0xFF};
	static const uint8_t le_bom[] = {0xFF, 0xFE, 'A',  0x00, 0xAC, 0x20, 0x00,
					 0xD8, 0x00, 0xDC, 0xFF, 0xDB, 0xFF, 0xDF};
	static const uint8_t expected[] = {'A',  0xE2, 0x82, 0xAC, 0xF0, 0x90,
					   0x80, 0x80, 0xF4, 0x8F, 0xBF, 0xBF};
	const struct {
		const uint8_t *bytes;
		size_t len;
	} cases[] = {{be, sizeof(be)}, {be_bom, sizeof(be_bom)}, {le_bom, sizeof(le_bom)}};
	uint8_t payload[20];
	char converted[sizeof(expected) + 1U];
	struct ndef_text text;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		payload[0] = 0x82;
		payload[1] = 'e';
		payload[2] = 'n';
		memcpy(&payload[3], cases[i].bytes, cases[i].len);
		zassert_ok(ndef_text_payload_decode(payload, cases[i].len + 3U, &text));
		zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), &len));
		zassert_equal(len, sizeof(expected));
		zassert_mem_equal(converted, expected, sizeof(expected));
		zassert_equal(converted[len], '\0');
		zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF16, "en",
						    2U, cases[i].bytes, cases[i].len, &len));
		zassert_equal(payload[0], 0x82);
		zassert_equal(len, cases[i].len + 3U);
		zassert_mem_equal(&payload[3], cases[i].bytes, cases[i].len);
	}
}

ZTEST(ndef_spec_text, test_invalid_utf16_is_rejected_before_conversion)
{
	static const struct {
		uint8_t bytes[6];
		size_t len;
	} cases[] = {
		{{0x00}, 1U},
		{{0xD8, 0x00}, 2U},
		{{0xDC, 0x00}, 2U},
		{{0xD8, 0x00, 0x00, 'A'}, 4U},
		{{0xD8, 0x00, 0xD8, 0x01}, 4U},
		{{0xFE, 0xFF, 0xD8, 0x00}, 4U},
		{{0xFF, 0xFE, 0x00, 0xDC}, 4U},
		{{0xFF, 0xFE, 'A'}, 3U},
	};
	uint8_t payload[10];
	char converted[16];
	struct ndef_text text;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		payload[0] = 0x82;
		payload[1] = 'e';
		payload[2] = 'n';
		memcpy(&payload[3], cases[i].bytes, cases[i].len);
		zassert_equal(ndef_text_payload_decode(payload, cases[i].len + 3U, &text), -EBADMSG,
			      "UTF-16 case %zu", i);
		zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF16,
						       "en", 2U, cases[i].bytes, cases[i].len,
						       &len),
			      -EINVAL, "UTF-16 case %zu", i);
		text.encoding = NDEF_TEXT_UTF16;
		text.text = cases[i].bytes;
		text.text_len = cases[i].len;
		zassert_equal(ndef_text_to_utf8(&text, converted, sizeof(converted), NULL),
			      -EBADMSG, "UTF-16 case %zu", i);
	}
}

ZTEST(ndef_spec_text, test_line_breaks_require_crlf)
{
	/* Section 3.2.1: line breaks MUST use CRLF in UTF-8 and UTF-16. */
	static const char *const invalid[] = {"\n", "\r", "\n\r", "\r\r", "a\rb", "a\nb"};
	static const uint8_t valid_be[] = {0x00, 'a', 0x00, 0x0D, 0x00, 0x0A, 0x00, 'b'};
	static const uint8_t valid_le[] = {0xFF, 0xFE, 'a',  0x00, 0x0D,
					   0x00, 0x0A, 0x00, 'b',  0x00};
	uint8_t payload[24];
	uint8_t utf16[16];
	struct ndef_text text;
	size_t len;

	zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8, "en", 2U,
					    "a\r\nb", 4U, &len));
	zassert_ok(ndef_text_payload_decode(payload, len, &text));
	zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF16, "en", 2U,
					    valid_be, sizeof(valid_be), &len));
	zassert_ok(ndef_text_payload_decode(payload, len, &text));
	zassert_ok(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF16, "en", 2U,
					    valid_le, sizeof(valid_le), &len));
	zassert_ok(ndef_text_payload_decode(payload, len, &text));

	for (size_t i = 0U; i < ARRAY_SIZE(invalid); i++) {
		size_t n = strlen(invalid[i]);

		payload[0] = 0x02;
		payload[1] = 'e';
		payload[2] = 'n';
		memcpy(&payload[3], invalid[i], n);
		zassert_equal(ndef_text_payload_decode(payload, n + 3U, &text), -EBADMSG);
		zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF8,
						       "en", 2U, invalid[i], n, &len),
			      -EINVAL);
		for (size_t j = 0U; j < n; j++) {
			utf16[2U * j] = 0U;
			utf16[2U * j + 1U] = (uint8_t)invalid[i][j];
		}
		payload[0] = 0x82;
		memcpy(&payload[3], utf16, 2U * n);
		zassert_equal(ndef_text_payload_decode(payload, 2U * n + 3U, &text), -EBADMSG);
		zassert_equal(ndef_text_payload_encode(payload, sizeof(payload), NDEF_TEXT_UTF16,
						       "en", 2U, utf16, 2U * n, &len),
			      -EINVAL);
	}
}

ZTEST(ndef_spec_text, test_display_policy_does_not_rewrite_payload)
{
	/* Display filtering/collapsing is an application SHOULD, not payload conversion. */
	static const uint8_t payload[] = {0x02, 'e', 'n', 'a', ' ', ' ', 'b', 0x08, 0x09, 0x1B};
	struct ndef_text text;
	char converted[8];
	size_t len;

	zassert_ok(ndef_text_payload_decode(payload, sizeof(payload), &text));
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), &len));
	zassert_equal(len, sizeof(payload) - 3U);
	zassert_mem_equal(converted, &payload[3], sizeof(payload) - 3U);
}

ZTEST(ndef_spec_text, test_conversion_capacity_empty_bom_and_invalid_encoding)
{
	static const uint8_t euro[] = {0x82, 'e', 'n', 0x20, 0xAC};
	static const uint8_t empty_bom[] = {0x82, 'e', 'n', 0xFF, 0xFE};
	struct ndef_text text;
	char converted[4];
	size_t len = 123U;

	zassert_ok(ndef_text_payload_decode(euro, sizeof(euro), &text));
	zassert_equal(ndef_text_to_utf8(&text, converted, 3U, &len), -ENOSPC);
	zassert_equal(len, 123U);
	zassert_ok(ndef_text_to_utf8(&text, converted, sizeof(converted), &len));
	zassert_equal(len, 3U);
	zassert_mem_equal(converted, "\xE2\x82\xAC\0", 4U);
	zassert_ok(ndef_text_payload_decode(empty_bom, sizeof(empty_bom), &text));
	zassert_ok(ndef_text_to_utf8(&text, converted, 1U, &len));
	zassert_equal(len, 0U);
	zassert_equal(converted[0], '\0');
	text.encoding = (enum ndef_text_encoding)2;
	zassert_equal(ndef_text_to_utf8(&text, converted, sizeof(converted), &len), -EINVAL);
}

ZTEST(ndef_spec_text, test_record_type_chunking_and_invalid_arguments)
{
	struct ndef_record rec;
	struct ndef_text text;
	struct ndef_writer writer;
	uint8_t encoded[24];
	char converted[16];
	size_t len = 321U;

	zassert_ok(ndef_record_decode(text_example, sizeof(text_example), &rec));
	rec.chunk_count = 2U;
	zassert_equal(ndef_text_parse(&rec, &text), -ENOTSUP);
	rec.chunk_count = 1U;
	rec.tnf = NDEF_TNF_MEDIA;
	zassert_equal(ndef_text_parse(&rec, &text), -ENOMSG);
	rec.tnf = NDEF_TNF_WELL_KNOWN;
	rec.type = (const uint8_t *)"t";
	zassert_equal(ndef_text_parse(&rec, &text), -ENOMSG);
	zassert_equal(ndef_text_parse(NULL, &text), -EINVAL);
	zassert_equal(ndef_text_payload_decode(NULL, 0U, &text), -EINVAL);
	zassert_equal(ndef_text_payload_decode(&text_example[4], 16U, NULL), -EINVAL);
	zassert_equal(ndef_text_payload_encode(NULL, 24U, NDEF_TEXT_UTF8, "en", 2U, "X", 1U, &len),
		      -EINVAL);
	zassert_equal(
		ndef_text_payload_encode(encoded, 24U, NDEF_TEXT_UTF8, NULL, 2U, "X", 1U, &len),
		-EINVAL);
	zassert_equal(
		ndef_text_payload_encode(encoded, 24U, NDEF_TEXT_UTF8, "en", 2U, NULL, 1U, &len),
		-EINVAL);
	zassert_equal(
		ndef_text_payload_encode(encoded, 24U, NDEF_TEXT_UTF8, "en", 2U, "X", 1U, NULL),
		-EINVAL);
	zassert_equal(ndef_text_payload_encode(encoded, 24U, (enum ndef_text_encoding)2, "en", 2U,
					       "X", 1U, &len),
		      -EINVAL);
	zassert_equal(len, 321U);
	zassert_equal(ndef_text_to_utf8(NULL, converted, sizeof(converted), NULL), -EINVAL);
	zassert_ok(ndef_text_payload_decode(&text_example[4], 16U, &text));
	zassert_equal(ndef_text_to_utf8(&text, NULL, 16U, NULL), -EINVAL);
	zassert_equal(ndef_text_to_utf8(&text, converted, 0U, NULL), -EINVAL);
	zassert_equal(ndef_text_to_utf8(&text, converted, 13U, NULL), -ENOSPC);

	zassert_ok(ndef_writer_init(&writer, encoded, sizeof(encoded)));
	zassert_equal(ndef_text_write(&writer, NULL, "X"), -EINVAL);
	zassert_equal(ndef_text_write(&writer, "en", NULL), -EINVAL);
	zassert_equal(ndef_writer_len(&writer), 0U);
}

ZTEST_SUITE(ndef_spec_text, NULL, NULL, NULL, NULL, NULL);
