/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>

/* "hello" in English: MB ME SR TNF=1, type "T". */
static const uint8_t msg_text[] = {
	0xD1, 0x01, 0x08, 'T', 0x02, 'e', 'n', 'h', 'e', 'l', 'l', 'o',
};

/* Two records: a URI record with an ID and an empty-payload media record. */
static const uint8_t msg_two[] = {
	0x99, 0x01, 0x04, 0x02, 'U', 'i', 'd', 0x04, 'a', '.', 'b', 0x52,
	0x0A, 0x00, 't',  'e',  'x', 't', '/', 'p',  'l', 'a', 'i', 'n',
};

static int decode_all(const uint8_t *buf, size_t len, uint32_t *count)
{
	return ndef_msg_validate(buf, len, 0U, NULL, count);
}

ZTEST(ndef_core, test_decode_short_record)
{
	struct ndef_record rec;

	zassert_ok(ndef_record_decode(msg_text, sizeof(msg_text), &rec));
	zassert_equal(rec.tnf, NDEF_TNF_WELL_KNOWN);
	zassert_equal(rec.type_len, 1U);
	zassert_equal(rec.type[0], 'T');
	zassert_equal(rec.id_len, 0U);
	zassert_is_null(rec.id);
	zassert_equal(rec.payload_len, 8U);
	zassert_equal(rec.payload, &msg_text[4]);
	zassert_equal(rec.raw_len, sizeof(msg_text));
	zassert_equal(rec.chunk_count, 1U);
}

ZTEST(ndef_core, test_decode_long_record)
{
	static const uint8_t buf[] = {
		0xC2, 0x03, 0x00, 0x00, 0x00, 0x02, 'a', '/', 'b', 0x55, 0xAA,
	};
	struct ndef_record rec;

	zassert_ok(ndef_record_decode(buf, sizeof(buf), &rec));
	zassert_equal(rec.tnf, NDEF_TNF_MEDIA);
	zassert_equal(rec.payload_len, 2U);
	zassert_equal(rec.payload[1], 0xAA);
	zassert_ok(ndef_msg_validate(buf, sizeof(buf), NDEF_VALIDATE_ALL, NULL, NULL));
}

ZTEST(ndef_core, test_iterate_two_records)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	size_t len;
	uint32_t count;

	zassert_ok(ndef_msg_iter_init(&it, msg_two, sizeof(msg_two)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(ndef_msg_iter_index(&it), 0U);
	zassert_true(ndef_record_is_wkt(&rec, "U"));
	zassert_equal(rec.id_len, 2U);
	zassert_mem_equal(rec.id, "id", 2U);
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(ndef_msg_iter_index(&it), 1U);
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)"TEXT/Plain", 10U));
	zassert_equal(rec.payload_len, 0U);
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);

	zassert_ok(ndef_msg_validate(msg_two, sizeof(msg_two), NDEF_VALIDATE_ALL, &len, &count));
	zassert_equal(len, sizeof(msg_two));
	zassert_equal(count, 2U);
}

ZTEST(ndef_core, test_every_truncation_fails)
{
	for (size_t n = 0; n < sizeof(msg_two); n++) {
		zassert_not_equal(decode_all(msg_two, n, NULL), 0, "length %zu", n);
	}
}

ZTEST(ndef_core, test_trailing_octets)
{
	uint8_t buf[sizeof(msg_text) + 2U];
	size_t len;

	memcpy(buf, msg_text, sizeof(msg_text));
	buf[sizeof(msg_text)] = 0x00;
	buf[sizeof(msg_text) + 1U] = 0xFE;

	zassert_ok(ndef_msg_validate(buf, sizeof(buf), 0U, &len, NULL));
	zassert_equal(len, sizeof(msg_text));
	zassert_equal(ndef_msg_validate(buf, sizeof(buf), NDEF_VALIDATE_EXACT, NULL, NULL),
		      -EBADMSG);
}

ZTEST(ndef_core, test_message_flags)
{
	uint8_t buf[sizeof(msg_two)];

	/* No MB on the first record. */
	memcpy(buf, msg_two, sizeof(buf));
	buf[0] &= (uint8_t)~NDEF_HDR_MB;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* MB on the second record. */
	memcpy(buf, msg_two, sizeof(buf));
	buf[11] |= NDEF_HDR_MB;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* No ME at all. */
	memcpy(buf, msg_two, sizeof(buf));
	buf[11] &= (uint8_t)~NDEF_HDR_ME;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* ME on the first record ends the message early. */
	memcpy(buf, msg_two, sizeof(buf));
	buf[0] |= NDEF_HDR_ME;
	zassert_ok(decode_all(buf, sizeof(buf), NULL));
	zassert_equal(ndef_msg_validate(buf, sizeof(buf), NDEF_VALIDATE_EXACT, NULL, NULL),
		      -EBADMSG);

	/* Empty buffer: a message holds at least one record. */
	zassert_equal(decode_all(buf, 0U, NULL), -EBADMSG);
}

ZTEST(ndef_core, test_tnf_rules)
{
	/* Empty record. */
	static const uint8_t empty[] = {0xD0, 0x00, 0x00};
	static const uint8_t empty_type[] = {0xD0, 0x01, 0x00, 'x'};
	static const uint8_t empty_payload[] = {0xD0, 0x00, 0x01, 0x00};
	static const uint8_t empty_id[] = {0xD8, 0x00, 0x00, 0x01, 'x'};
	/* Type mandatory for TNF 1 to 4. */
	static const uint8_t wkt_no_type[] = {0xD1, 0x00, 0x00};
	/* No type for unknown. */
	static const uint8_t unknown_type[] = {0xD5, 0x01, 0x00, 'x'};
	static const uint8_t unknown[] = {0xD5, 0x00, 0x01, 0x42};
	/* Unchanged outside a chunk sequence. */
	static const uint8_t unchanged[] = {0xD6, 0x00, 0x00};
	/* Reserved. */
	static const uint8_t reserved[] = {0xD7, 0x00, 0x00};

	zassert_ok(decode_all(empty, sizeof(empty), NULL));
	zassert_equal(decode_all(empty_type, sizeof(empty_type), NULL), -EBADMSG);
	zassert_equal(decode_all(empty_payload, sizeof(empty_payload), NULL), -EBADMSG);
	zassert_equal(decode_all(empty_id, sizeof(empty_id), NULL), -EBADMSG);
	zassert_equal(decode_all(wkt_no_type, sizeof(wkt_no_type), NULL), -EBADMSG);
	zassert_equal(decode_all(unknown_type, sizeof(unknown_type), NULL), -EBADMSG);
	zassert_ok(decode_all(unknown, sizeof(unknown), NULL));
	zassert_equal(decode_all(unchanged, sizeof(unchanged), NULL), -EBADMSG);
	zassert_equal(decode_all(reserved, sizeof(reserved), NULL), -EBADMSG);
}

ZTEST(ndef_core, test_reserved_tnf_is_unknown)
{
	/* Reserved TNF, with a type, followed by a regular record. */
	static const uint8_t msg[] = {0x97, 0x01, 0x01, 'x', 0x42, 0x50, 0x00, 0x00};
	struct ndef_msg_iter it;
	struct ndef_record rec;

	zassert_ok(ndef_msg_iter_init(&it, msg, sizeof(msg)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.tnf, NDEF_TNF_UNKNOWN);
	zassert_equal(rec.header & NDEF_HDR_TNF_MASK, NDEF_TNF_RESERVED);
	zassert_equal(rec.payload_len, 1U);
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);

	/* Generators must not use it: the validator rejects it. */
	zassert_equal(ndef_msg_validate(msg, sizeof(msg), 0U, NULL, NULL), -EBADMSG);
}

/* "text/plain" record chunked as "ab" + "cd" + "ef", followed by an empty record. */
static const uint8_t msg_chunked[] = {
	0xBA, 0x0A, 0x02, 0x01, 't',  'e', 'x', 't',  '/',  'p',  'l', 'a', 'i',  'n',  'Z',
	'a',  'b',  0x36, 0x00, 0x02, 'c', 'd', 0x16, 0x00, 0x02, 'e', 'f', 0x50, 0x00, 0x00,
};

ZTEST(ndef_core, test_chunked_record)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	uint8_t out[6];
	uint8_t big[8];
	uint32_t count;

	zassert_ok(decode_all(msg_chunked, sizeof(msg_chunked), &count));
	zassert_equal(count, 2U);

	zassert_ok(ndef_msg_iter_init(&it, msg_chunked, sizeof(msg_chunked)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.chunk_count, 3U);
	zassert_equal(rec.payload_len, 6U);
	zassert_equal(rec.id_len, 1U);
	zassert_equal(rec.raw_len, 27U);

	zassert_ok(ndef_record_payload_read(&rec, 1U, out, 4U));
	zassert_mem_equal(out, "bcde", 4U);
	zassert_ok(ndef_record_payload_read(&rec, 6U, out, 0U));
	zassert_equal(ndef_record_payload_read(&rec, 5U, out, 2U), -EINVAL);

	zassert_equal(ndef_record_reassemble(&rec, out, 5U), -ENOSPC);
	zassert_ok(ndef_record_reassemble(&rec, big, sizeof(big)));
	zassert_equal(rec.payload, big);
	zassert_mem_equal(big, "abcdef", 6U);

	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.tnf, NDEF_TNF_EMPTY);
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);
}

ZTEST(ndef_core, test_chunk_rules)
{
	uint8_t buf[sizeof(msg_chunked)];

	/* ME on the initial chunk. */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[0] |= NDEF_HDR_ME;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* Middle chunk that is not "unchanged". */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[17] = (uint8_t)((buf[17] & ~NDEF_HDR_TNF_MASK) | NDEF_TNF_UNKNOWN);
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* ME on a middle chunk. */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[17] |= NDEF_HDR_ME;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* MB on a middle chunk. */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[17] |= NDEF_HDR_MB;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* IL on the terminating chunk. */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[22] |= NDEF_HDR_IL;
	zassert_not_equal(decode_all(buf, sizeof(buf), NULL), 0);

	/* Terminating chunk missing: the sequence runs into an empty record. */
	memcpy(buf, msg_chunked, sizeof(buf));
	buf[22] |= NDEF_HDR_CF;
	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);

	/* Chunked empty record. */
	{
		static const uint8_t cf_empty[] = {0xB0, 0x00, 0x00, 0x56, 0x00, 0x00};

		zassert_equal(decode_all(cf_empty, sizeof(cf_empty), NULL), -EBADMSG);
	}

	/* Chunked record ending the message. */
	{
		static const uint8_t cf_last[] = {0xB5, 0x00, 0x01, 0x01, 0x56, 0x00, 0x01, 0x02};
		struct ndef_msg_iter it;
		struct ndef_record rec;

		zassert_ok(ndef_msg_iter_init(&it, cf_last, sizeof(cf_last)));
		zassert_ok(ndef_msg_iter_next(&it, &rec));
		zassert_equal(rec.tnf, NDEF_TNF_UNKNOWN);
		zassert_equal(rec.payload_len, 2U);
		zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);
	}
}

ZTEST(ndef_core, test_huge_length)
{
	/* A long record claiming 0xFFFFFFFF octets. */
	static const uint8_t buf[] = {
		0xA5, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
	};

	zassert_equal(decode_all(buf, sizeof(buf), NULL), -EBADMSG);
}

ZTEST(ndef_core, test_type_eq)
{
	static const uint8_t media[] = {0xD2, 0x17, 0x00, 'T', 'e', 'x', 't', '/', 'P',
					'l',  'a',  'i',  'n', ' ', ';', 'c', 'h', 'a',
					'r',  's',  'e',  't', '=', 'x', 'y', 'z'};
	static const uint8_t ext[] = {0xD4, 0x0D, 0x00, 'E', 'x', 'a', 'm', 'p',
				      'l',  'e',  '.',  'c', 'o', 'm', ':', 'X'};
	struct ndef_record rec;

	zassert_ok(ndef_record_decode(media, sizeof(media), &rec));
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)"text/plain", 10U));
	zassert_true(
		ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)"text/plain;a=b", 14U));
	zassert_false(ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)"text/html", 9U));
	zassert_false(
		ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL, (const uint8_t *)"text/plain", 10U));

	zassert_ok(ndef_record_decode(ext, sizeof(ext), &rec));
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL, (const uint8_t *)"example.COM:x",
					 13U));

	zassert_ok(ndef_record_decode(msg_text, sizeof(msg_text), &rec));
	zassert_true(ndef_record_is_wkt(&rec, "T"));
	zassert_false(ndef_record_is_wkt(&rec, "t"));
}

ZTEST(ndef_core, test_type_validate)
{
#define VALID(tnf, s) zassert_ok(ndef_type_validate(tnf, (const uint8_t *)(s), strlen(s)), "%s", s)
#define INVALID(tnf, s)                                                                            \
	zassert_equal(ndef_type_validate(tnf, (const uint8_t *)(s), strlen(s)), -EINVAL, "%s", s)

	VALID(NDEF_TNF_EMPTY, "");
	INVALID(NDEF_TNF_EMPTY, "x");
	VALID(NDEF_TNF_UNKNOWN, "");
	VALID(NDEF_TNF_UNCHANGED, "");

	VALID(NDEF_TNF_WELL_KNOWN, "T");
	VALID(NDEF_TNF_WELL_KNOWN, "Sp");
	VALID(NDEF_TNF_WELL_KNOWN, "Trip-to-Texas");
	VALID(NDEF_TNF_WELL_KNOWN, "0");
	VALID(NDEF_TNF_WELL_KNOWN, "a(b)+c,d-e.f:g=h@i;j$k_l!m*n'o");
	INVALID(NDEF_TNF_WELL_KNOWN, "");
	INVALID(NDEF_TNF_WELL_KNOWN, "a b");
	INVALID(NDEF_TNF_WELL_KNOWN, "a/b");
	INVALID(NDEF_TNF_WELL_KNOWN, "a?b");
	INVALID(NDEF_TNF_WELL_KNOWN, "a#b");
	/* No percent encoding in well-known types. */
	INVALID(NDEF_TNF_WELL_KNOWN, "a%2F");
	/* First character: upper (global), lower or number (local). */
	INVALID(NDEF_TNF_WELL_KNOWN, "-x");
	INVALID(NDEF_TNF_WELL_KNOWN, "(x");
	INVALID(NDEF_TNF_WELL_KNOWN, "\xc3\xa9");

	VALID(NDEF_TNF_MEDIA, "text/plain");
	VALID(NDEF_TNF_MEDIA, "application/vnd.bluetooth.ep.oob");
	VALID(NDEF_TNF_MEDIA, "text/plain; charset=utf-8");
	VALID(NDEF_TNF_MEDIA, "text/plain;a=\"b c;\\\"d\"");
	INVALID(NDEF_TNF_MEDIA, "text");
	INVALID(NDEF_TNF_MEDIA, "text/");
	INVALID(NDEF_TNF_MEDIA, "/plain");
	INVALID(NDEF_TNF_MEDIA, "text/pl ain");
	INVALID(NDEF_TNF_MEDIA, "text/plain;");
	INVALID(NDEF_TNF_MEDIA, "text/plain;a");
	INVALID(NDEF_TNF_MEDIA, "text/plain;a=\"b");

	VALID(NDEF_TNF_ABSOLUTE_URI, "http://example.com/a?b=c");
	VALID(NDEF_TNF_ABSOLUTE_URI, "http://schemas.xmlsoap.org/soap/envelope/");
	VALID(NDEF_TNF_ABSOLUTE_URI, "urn:nfc:wkt:T");
	VALID(NDEF_TNF_ABSOLUTE_URI, "x-y.z+w:%41");
	/* Non US-ASCII octets: encoding left to the application. */
	VALID(NDEF_TNF_ABSOLUTE_URI, "http://h\xc3\xa4\xc3\xa4.fi/");
	/* absolute-URI has no fragment. */
	INVALID(NDEF_TNF_ABSOLUTE_URI, "http://example.com/a#d");
	INVALID(NDEF_TNF_ABSOLUTE_URI, "example.com");
	INVALID(NDEF_TNF_ABSOLUTE_URI, "1http://x");
	INVALID(NDEF_TNF_ABSOLUTE_URI, "http://a b");
	INVALID(NDEF_TNF_ABSOLUTE_URI, "http://%4");

	VALID(NDEF_TNF_EXTERNAL, "example.com:mytype");
	VALID(NDEF_TNF_EXTERNAL, "android.com:pkg");
	VALID(NDEF_TNF_EXTERNAL, "example.com:a%C3%A9");
	INVALID(NDEF_TNF_EXTERNAL, "example.com:a%C");
	INVALID(NDEF_TNF_EXTERNAL, "example.com:a/b");
	INVALID(NDEF_TNF_EXTERNAL, "example.com");
	INVALID(NDEF_TNF_EXTERNAL, ":type");
	INVALID(NDEF_TNF_EXTERNAL, "example.com:");
	INVALID(NDEF_TNF_EXTERNAL, "exa_mple.com:x");

	INVALID(NDEF_TNF_RESERVED, "");

#undef VALID
#undef INVALID
}

ZTEST(ndef_core, test_id_validate)
{
#define VALID_ID(s) zassert_ok(ndef_id_validate((const uint8_t *)(s), strlen(s)), "%s", s)
#define INVALID_ID(s)                                                                              \
	zassert_equal(ndef_id_validate((const uint8_t *)(s), strlen(s)), -EINVAL, "%s", s)

	VALID_ID("");
	VALID_ID("0");
	VALID_ID("#part");
	VALID_ID("../a/b?c=d");
	VALID_ID("http://example.com/x#y");
	VALID_ID("caf\xc3\xa9");
	INVALID_ID("a b");
	INVALID_ID("a<b>");
	INVALID_ID("a%G0");
	INVALID_ID("a\x01");

#undef VALID_ID
#undef INVALID_ID
}

ZTEST(ndef_core, test_validate_id)
{
	/* Media record "a/b" with ID "x y". */
	static const uint8_t msg[] = {0xDA, 0x03, 0x00, 0x03, 'a', '/', 'b', 'x', ' ', 'y'};

	zassert_ok(ndef_msg_validate(msg, sizeof(msg), 0U, NULL, NULL));
	zassert_equal(ndef_msg_validate(msg, sizeof(msg), NDEF_VALIDATE_TYPES, NULL, NULL),
		      -EBADMSG);
}

ZTEST(ndef_core, test_wkt_local)
{
	zassert_true(ndef_wkt_is_local((const uint8_t *)"ac", 2U));
	zassert_true(ndef_wkt_is_local((const uint8_t *)"9x", 2U));
	zassert_false(ndef_wkt_is_local((const uint8_t *)"Hs", 2U));
	zassert_false(ndef_wkt_is_local(NULL, 0U));
}

ZTEST(ndef_core, test_find)
{
	struct ndef_record rec;

	zassert_ok(ndef_msg_find_type(msg_two, sizeof(msg_two), NDEF_TNF_MEDIA,
				      (const uint8_t *)"text/plain", 10U, &rec));
	zassert_equal(rec.raw, &msg_two[11]);
	zassert_equal(ndef_msg_find_type(msg_two, sizeof(msg_two), NDEF_TNF_WELL_KNOWN,
					 (const uint8_t *)"T", 1U, &rec),
		      -ENOENT);
	zassert_ok(ndef_msg_find_id(msg_two, sizeof(msg_two), (const uint8_t *)"id", 2U, &rec));
	zassert_true(ndef_record_is_wkt(&rec, "U"));
	zassert_equal(ndef_msg_find_id(msg_two, sizeof(msg_two), (const uint8_t *)"ID", 2U, &rec),
		      -ENOENT);
}

ZTEST(ndef_core, test_utf8)
{
	zassert_true(ndef_utf8_valid((const uint8_t *)"", 0U));
	zassert_true(
		ndef_utf8_valid((const uint8_t *)"a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 10U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xc0\x80", 2U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xe0\x80\x80", 3U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xed\xa0\x80", 3U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xf4\x90\x80\x80", 4U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xc3", 1U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\x80", 1U));
	zassert_false(ndef_utf8_valid((const uint8_t *)"\xff", 1U));
}

ZTEST(ndef_core, test_invalid_args)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;

	zassert_equal(ndef_record_decode(NULL, 3U, &rec), -EINVAL);
	zassert_equal(ndef_record_decode(msg_text, sizeof(msg_text), NULL), -EINVAL);
	zassert_equal(ndef_msg_iter_init(NULL, msg_text, 1U), -EINVAL);
	zassert_equal(ndef_msg_iter_init(&it, NULL, 1U), -EINVAL);
	zassert_equal(ndef_msg_validate(NULL, 1U, 0U, NULL, NULL), -EINVAL);
	zassert_false(ndef_record_type_eq(NULL, NDEF_TNF_EMPTY, NULL, 0U));
}

ZTEST_SUITE(ndef_core, NULL, NULL, NULL, NULL, NULL);
