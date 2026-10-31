/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>

/* Payloads here are arbitrary: RTD payload checks are not wanted. */
#define STRUCTURE (NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT)

static uint8_t buf[512];
static uint8_t ref[512];
static uint8_t big[300];

static const struct ndef_record_desc desc_t = NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, "T");
static const struct ndef_record_desc desc_media = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "a/b");

static void before(void *fixture)
{
	(void)fixture;
	memset(buf, 0xEE, sizeof(buf));
	memset(ref, 0xEE, sizeof(ref));
	for (size_t i = 0; i < sizeof(big); i++) {
		big[i] = (uint8_t)i;
	}
}

ZTEST(ndef_writer, test_single_record)
{
	static const uint8_t expected[] = {0xD1, 0x01, 0x02, 'T', 0x00, 'x'};
	struct ndef_writer w;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_equal(ndef_writer_finish(&w, &len), -ENODATA);
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00x", 2U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
}

ZTEST(ndef_writer, test_flags_follow_records)
{
	struct ndef_record_desc desc = desc_media;
	struct ndef_writer w;
	size_t len;

	desc.id = "i";
	desc.id_len = 1U;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00", 1U));
	zassert_equal(buf[0], NDEF_HDR_MB | NDEF_HDR_ME | NDEF_HDR_SR | NDEF_TNF_WELL_KNOWN);
	zassert_ok(ndef_writer_record_add(&w, &desc, big, sizeof(big)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00", 1U));
	zassert_ok(ndef_writer_finish(&w, &len));

	zassert_equal(buf[0], NDEF_HDR_MB | NDEF_HDR_SR | NDEF_TNF_WELL_KNOWN);
	zassert_equal(buf[5], NDEF_HDR_IL | NDEF_TNF_MEDIA);
	zassert_equal(len, 5U + NDEF_RECORD_SIZE(3U, 1U, sizeof(big)) + 5U);
	zassert_equal(buf[len - 5U], NDEF_HDR_ME | NDEF_HDR_SR | NDEF_TNF_WELL_KNOWN);
	zassert_ok(ndef_msg_validate(buf, len, STRUCTURE, NULL, NULL));
}

ZTEST(ndef_writer, test_long_record_round_trip)
{
	struct ndef_writer w;
	struct ndef_record rec;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc_media, big, sizeof(big)));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, NDEF_RECORD_SIZE(3U, 0U, sizeof(big)));
	zassert_equal(buf[0] & NDEF_HDR_SR, 0U);

	zassert_ok(ndef_msg_find_type(buf, len, NDEF_TNF_MEDIA, (const uint8_t *)"a/b", 3U, &rec));
	zassert_equal(rec.payload_len, sizeof(big));
	zassert_mem_equal(rec.payload, big, sizeof(big));
}

ZTEST(ndef_writer, test_no_space)
{
	struct ndef_writer w;

	zassert_ok(ndef_writer_init(&w, buf, 10U));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "ab", 2U));
	zassert_equal(ndef_writer_record_add(&w, &desc_t, "abcd", 4U), -ENOSPC);
	zassert_equal(ndef_writer_len(&w), 6U);
	zassert_equal(buf[0] & NDEF_HDR_ME, NDEF_HDR_ME);
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "", 0U));
	zassert_equal(ndef_writer_len(&w), 10U);
	zassert_ok(ndef_msg_validate(buf, 10U, STRUCTURE, NULL, NULL));
}

ZTEST(ndef_writer, test_invalid_desc)
{
	struct ndef_record_desc desc = {.tnf = NDEF_TNF_EMPTY};
	struct ndef_writer w;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc, NULL, 0U));
	zassert_equal(ndef_writer_record_add(&w, &desc, "x", 1U), -EINVAL);

	desc.tnf = NDEF_TNF_UNCHANGED;
	zassert_equal(ndef_writer_record_add(&w, &desc, NULL, 0U), -EINVAL);
	desc.tnf = NDEF_TNF_RESERVED;
	zassert_equal(ndef_writer_record_add(&w, &desc, NULL, 0U), -EINVAL);
	desc.tnf = NDEF_TNF_WELL_KNOWN;
	zassert_equal(ndef_writer_record_add(&w, &desc, NULL, 0U), -EINVAL);
	desc.tnf = NDEF_TNF_MEDIA;
	desc.type = "text";
	desc.type_len = 4U;
	zassert_equal(ndef_writer_record_add(&w, &desc, NULL, 0U), -EINVAL);
	desc.tnf = NDEF_TNF_UNKNOWN;
	zassert_equal(ndef_writer_record_add(&w, &desc, NULL, 0U), -EINVAL);
	desc.type_len = 0U;
	zassert_ok(ndef_writer_record_add(&w, &desc, "x", 1U));
	desc.id = "a b";
	desc.id_len = 3U;
	zassert_equal(ndef_writer_record_add(&w, &desc, "x", 1U), -EINVAL);
	zassert_equal(ndef_writer_record_add(&w, &desc_t, NULL, 1U), -EINVAL);
}

ZTEST(ndef_writer, test_open_payload_short)
{
	struct ndef_record_desc desc = desc_media;
	struct ndef_writer w;
	uint8_t *payload;
	size_t cap;
	size_t len;
	size_t ref_len;

	desc.id = "id";
	desc.id_len = 2U;

	zassert_ok(ndef_writer_init(&w, ref, sizeof(ref)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00", 1U));
	zassert_ok(ndef_writer_record_add(&w, &desc, "hello", 5U));
	zassert_ok(ndef_writer_finish(&w, &ref_len));

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00", 1U));
	zassert_ok(ndef_writer_record_begin(&w, &desc, &payload, &cap));
	zassert_equal(ndef_writer_record_add(&w, &desc_t, "", 0U), -EBUSY);
	zassert_equal(ndef_writer_finish(&w, &len), -EBUSY);
	memcpy(payload, "hello", 5U);
	zassert_equal(ndef_writer_record_end(&w, cap + 1U), -EINVAL);
	zassert_ok(ndef_writer_record_end(&w, 5U));
	zassert_ok(ndef_writer_finish(&w, &len));

	zassert_equal(len, ref_len);
	zassert_mem_equal(buf, ref, len);
}

ZTEST(ndef_writer, test_open_payload_long)
{
	struct ndef_writer w;
	uint8_t *payload;
	size_t cap;
	size_t len;
	size_t ref_len;

	zassert_ok(ndef_writer_init(&w, ref, sizeof(ref)));
	zassert_ok(ndef_writer_record_add(&w, &desc_media, big, sizeof(big)));
	zassert_ok(ndef_writer_finish(&w, &ref_len));

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_begin(&w, &desc_media, &payload, &cap));
	memcpy(payload, big, sizeof(big));
	zassert_ok(ndef_writer_record_end(&w, sizeof(big)));
	zassert_ok(ndef_writer_finish(&w, &len));

	zassert_equal(len, ref_len);
	zassert_mem_equal(buf, ref, len);
}

ZTEST(ndef_writer, test_abort)
{
	struct ndef_writer w;
	uint8_t *payload;
	size_t cap;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "\x00", 1U));
	zassert_ok(ndef_writer_record_begin(&w, &desc_media, &payload, &cap));
	ndef_writer_record_abort(&w);
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, 5U);
	zassert_ok(ndef_msg_validate(buf, len, STRUCTURE, NULL, NULL));
	zassert_equal(ndef_writer_record_end(&w, 0U), -EINVAL);
}

ZTEST(ndef_writer, test_chunks)
{
	static const uint8_t expected[] = {
		0xB2, 0x03, 0x02, 'a', '/', 'b', '1', '2', 0x36, 0x00, 0x00, 0x56, 0x00, 0x01, '3',
	};
	struct ndef_record_desc desc = {.tnf = NDEF_TNF_EMPTY};
	struct ndef_msg_iter it;
	struct ndef_record rec;
	struct ndef_writer w;
	uint8_t out[3];
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_equal(ndef_writer_chunk(&w, "x", 1U, true), -EINVAL);
	zassert_equal(ndef_writer_chunk_begin(&w, &desc, NULL, 0U), -EINVAL);
	zassert_ok(ndef_writer_chunk_begin(&w, &desc_media, "12", 2U));
	zassert_equal(ndef_writer_record_add(&w, &desc_t, "", 0U), -EBUSY);
	zassert_equal(ndef_writer_finish(&w, &len), -EBUSY);
	zassert_ok(ndef_writer_chunk(&w, NULL, 0U, false));
	zassert_ok(ndef_writer_chunk(&w, "3", 1U, true));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));

	/* A record after the chunked one moves ME. */
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "", 0U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(buf[11], 0x16);

	zassert_ok(ndef_msg_iter_init(&it, buf, len));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.chunk_count, 3U);
	zassert_ok(ndef_record_payload_read(&rec, 0U, out, 3U));
	zassert_mem_equal(out, "123", 3U);
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);
}

ZTEST(ndef_writer, test_chunk_after_record)
{
	struct ndef_writer w;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc_t, "", 0U));
	zassert_ok(ndef_writer_chunk_begin(&w, &desc_media, big, sizeof(big)));
	zassert_equal(buf[0] & NDEF_HDR_ME, 0U);
	zassert_ok(ndef_writer_chunk(&w, big, 10U, true));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, STRUCTURE, NULL, NULL));
}

ZTEST(ndef_writer, test_nested)
{
	static const struct ndef_record_desc desc_sp = NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, "Sp");
	static const uint8_t expected[] = {
		0xD1, 0x02, 0x0A, 'S',  'p',  0x01, 0x91, 0x01,
		0x01, 'U',  0x00, 0x51, 0x01, 0x00, 'T',
	};
	struct ndef_writer w;
	struct ndef_writer sub;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_nested_begin(&w, &desc_sp, "\x01", 1U, &sub));
	zassert_ok(ndef_writer_record_add(&sub, &desc_t, "", 0U));
	zassert_equal(ndef_writer_nested_end(&w, &sub), 0);
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, 10U);

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_nested_begin(&w, &desc_sp, "\x01", 1U, &sub));
	{
		static const struct ndef_record_desc desc_u =
			NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, "U");

		zassert_ok(ndef_writer_record_add(&sub, &desc_u, "\x00", 1U));
	}
	zassert_ok(ndef_writer_record_add(&sub, &desc_t, "", 0U));
	zassert_ok(ndef_writer_nested_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
}

ZTEST(ndef_writer, test_nested_no_space)
{
	static const struct ndef_record_desc desc_sp = NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, "Sp");
	struct ndef_writer w;
	struct ndef_writer sub;

	zassert_ok(ndef_writer_init(&w, buf, 8U));
	zassert_equal(ndef_writer_nested_begin(&w, &desc_sp, "ab", 2U, &sub), -ENOSPC);
	zassert_ok(ndef_writer_nested_begin(&w, &desc_sp, NULL, 0U, &sub));
	zassert_equal(ndef_writer_record_add(&sub, &desc_t, "", 0U), -ENOSPC);
	zassert_ok(ndef_writer_nested_end(&w, &sub));
	zassert_equal(ndef_writer_len(&w), 5U);
}

ZTEST_SUITE(ndef_writer, NULL, NULL, before, NULL, NULL);
