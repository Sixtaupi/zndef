/*
 * Copyright (c) 2026 Romain Pelletant
 * SPDX-License-Identifier: Apache-2.0
 */

/* API contracts supplement the normative vectors in test_spec_*.c. */
#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/items.h>
#include <ndef/rtd_di.h>
#include <ndef/rtd_handover.h>
#include <ndef/rtd_sig.h>
#include <ndef/rtd_sp.h>
#include <ndef/tlv.h>

ZTEST(ndef_api, test_finish_is_observational_and_writer_recovers)
{
	static const struct ndef_record_desc empty = {.tnf = NDEF_TNF_EMPTY};
	static const struct ndef_record_desc media = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "a/b");
	static const uint8_t expected[] = {0x90, 0x00, 0x00, 0x50, 0x00, 0x00};
	struct ndef_writer w;
	uint8_t buf[32];
	uint8_t *payload;
	size_t capacity;
	size_t len = 123U;

	zassert_equal(ndef_writer_finish(NULL, &len), -EINVAL);
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_equal(ndef_writer_finish(&w, &len), -ENODATA);
	zassert_equal(len, 123U);
	zassert_ok(ndef_writer_record_add(&w, &empty, NULL, 0U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, 3U);
	zassert_ok(ndef_writer_record_begin(&w, &media, &payload, &capacity));
	zassert_equal(ndef_writer_finish(&w, &len), -EBUSY);
	zassert_equal(len, 3U);
	zassert_equal(ndef_writer_record_add(&w, &empty, NULL, 0U), -EBUSY);
	zassert_equal(ndef_writer_record_end(&w, capacity + 1U), -EINVAL);
	ndef_writer_record_abort(&w);
	ndef_writer_record_abort(&w);
	zassert_equal(ndef_writer_len(&w), 3U);
	zassert_ok(ndef_writer_record_add(&w, &empty, NULL, 0U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));
}

ZTEST(ndef_api, test_payload_ranges_cross_chunk_headers)
{
	static const uint8_t msg[] = {
		0xB5, 0x00, 0x02, 'a', 'b', 0x36, 0x00, 0x01, 'c', 0x56, 0x00, 0x03, 'd', 'e', 'f',
	};
	struct ndef_msg_iter it;
	struct ndef_record rec;
	uint8_t dst[8];

	zassert_ok(ndef_msg_iter_init(&it, msg, sizeof(msg)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.chunk_count, 3U);
	for (size_t offset = 0U; offset <= 6U; offset++) {
		for (size_t len = 0U; len <= 6U - offset; len++) {
			memset(dst, 0xA5, sizeof(dst));
			zassert_ok(ndef_record_payload_read(&rec, offset, &dst[1], len));
			zassert_mem_equal(&dst[1], &"abcdef"[offset], len);
			zassert_equal(dst[0], 0xA5);
			zassert_equal(dst[len + 1U], 0xA5);
		}
	}
	zassert_equal(ndef_record_payload_read(&rec, 6U, dst, 1U), -EINVAL);
	zassert_equal(ndef_record_payload_read(&rec, SIZE_MAX, dst, 1U), -EINVAL);
	zassert_equal(ndef_record_payload_read(&rec, 1U, dst, SIZE_MAX), -EINVAL);
	zassert_equal(ndef_record_reassemble(&rec, dst, 5U), -ENOSPC);
	zassert_equal(rec.chunk_count, 3U);
	zassert_ok(ndef_record_reassemble(&rec, dst, 6U));
	zassert_equal(rec.payload, dst);
	zassert_equal(rec.chunk_count, 1U);
	zassert_mem_equal(dst, "abcdef", 6U);
}

ZTEST(ndef_api, test_items_keep_successful_prefix_on_error)
{
	static const struct ndef_record_desc empty = {.tnf = NDEF_TNF_EMPTY};
	static const struct ndef_item items[] = {
		NDEF_ITEM_MEDIA("a/b", "x", 1U),
		NDEF_ITEM_MEDIA("a/b", "too long", 8U),
	};
	static const uint8_t expected[] = {
		0x90, 0x00, 0x00, 0x52, 0x03, 0x01, 'a', '/', 'b', 'x',
	};
	struct ndef_writer w;
	uint8_t buf[12];
	size_t len;
	uint32_t count;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &empty, NULL, 0U));
	zassert_equal(ndef_items_write(&w, items, 2U), -ENOSPC);
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, &count));
	zassert_equal(count, 2U);
}

ZTEST(ndef_api, test_alternative_carrier_payload_encoding)
{
	static const struct ndef_ho_ref carrier = {.id = (const uint8_t *)"c", .len = 1U};
	static const struct ndef_ho_ref auxiliary = {.id = (const uint8_t *)"ab", .len = 2U};
	static const uint8_t expected[] = {0x01, 0x01, 'c', 0x01, 0x02, 'a', 'b'};
	struct ndef_ho_ac ac;
	struct ndef_ho_ref ref;
	uint8_t buf[sizeof(expected) + 1U];
	size_t len = 123U;

	memset(buf, 0xA5, sizeof(buf));
	zassert_equal(ndef_ho_ac_payload_encode(buf, sizeof(expected) - 1U, NDEF_HO_CPS_ACTIVE,
						&carrier, &auxiliary, 1U, &len),
		      -ENOSPC);
	zassert_equal(len, 123U);
	zassert_equal(buf[sizeof(expected)], 0xA5);
	zassert_ok(ndef_ho_ac_payload_encode(buf, sizeof(expected), NDEF_HO_CPS_ACTIVE, &carrier,
					     &auxiliary, 1U, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_equal(buf[sizeof(expected)], 0xA5);
	zassert_ok(ndef_ho_ac_payload_decode(expected, sizeof(expected), &ac));
	zassert_equal(ac.cps, NDEF_HO_CPS_ACTIVE);
	zassert_mem_equal(ac.carrier_ref, "c", 1U);
	zassert_ok(ndef_ho_ac_aux_get(&ac, 0U, &ref));
	zassert_equal(ref.len, 2U);
	zassert_mem_equal(ref.id, "ab", 2U);
	zassert_equal(ndef_ho_ac_aux_get(&ac, 1U, &ref), -ENOENT);
	zassert_equal(ndef_ho_ac_aux_get(NULL, 0U, &ref), -EINVAL);
}

ZTEST(ndef_api, test_tlv_length_boundaries_and_output_bounds)
{
	static const size_t lengths[] = {0U, 1U, 254U, 255U, 256U, 65534U};
	static const uint8_t expected[][4] = {
		{0x03, 0x00},
		{0x03, 0x01},
		{0x03, 0xFE},
		{0x03, 0xFF, 0x00, 0xFF},
		{0x03, 0xFF, 0x01, 0x00},
		{0x03, 0xFF, 0xFF, 0xFE},
	};
	uint8_t buf[6];

	for (size_t i = 0U; i < ARRAY_SIZE(lengths); i++) {
		size_t header_len = lengths[i] < 255U ? 2U : 4U;
		size_t out_len = 123U;

		memset(buf, 0xA5, sizeof(buf));
		zassert_equal(ndef_tlv_header_encode(&buf[1], header_len - 1U, NDEF_TLV_NDEF,
						     lengths[i], &out_len),
			      -ENOSPC);
		zassert_equal(out_len, 123U);
		zassert_equal(buf[0], 0xA5);
		zassert_equal(buf[header_len + 1U], 0xA5);
		zassert_ok(ndef_tlv_header_encode(&buf[1], header_len, NDEF_TLV_NDEF, lengths[i],
						  &out_len));
		zassert_equal(out_len, header_len);
		zassert_mem_equal(&buf[1], expected[i], header_len);
		zassert_equal(NDEF_TLV_SIZE(lengths[i]), header_len + lengths[i]);
		zassert_equal(buf[0], 0xA5);
		zassert_equal(buf[header_len + 1U], 0xA5);
	}
}

ZTEST(ndef_api, test_optional_decoders_reject_every_truncation)
{
	/* Fixed valid contract fixtures; these are not normative certification vectors. */
	static const uint8_t di[] = {0x00, 0x01, 'A', 0x01, 0x01, 'M'};
	static const uint8_t ac[] = {0x01, 0x01, 'c', 0x00};
	struct ndef_di information;
	struct ndef_ho_ac carrier;

	zassert_ok(ndef_di_payload_decode(di, sizeof(di), &information));
	for (size_t len = 0U; len < sizeof(di); len++) {
		zassert_equal(ndef_di_payload_decode(di, len, &information), -EBADMSG);
	}
	zassert_ok(ndef_ho_ac_payload_decode(ac, sizeof(ac), &carrier));
	for (size_t len = 0U; len < sizeof(ac); len++) {
		zassert_equal(ndef_ho_ac_payload_decode(ac, len, &carrier), -EBADMSG);
	}
}

static int stop_data(const uint8_t *data, size_t len, void *user_data)
{
	size_t *calls = user_data;

	zassert_not_null(data);
	zassert_true(len > 0U);
	(*calls)++;
	return -ECANCELED;
}

ZTEST(ndef_api, test_signature_data_propagates_callback_error)
{
	static const uint8_t msg[] = {0xD2, 0x03, 0x01, 'a', '/', 'b', 'x'};
	struct ndef_record rec;
	size_t calls = 0U;

	zassert_ok(ndef_record_decode(msg, sizeof(msg), &rec));
	zassert_equal(ndef_sig_record_data(&rec, stop_data, &calls), -ECANCELED);
	zassert_equal(calls, 1U);
	zassert_equal(ndef_sig_record_data(NULL, stop_data, &calls), -EINVAL);
	zassert_equal(ndef_sig_record_data(&rec, NULL, &calls), -EINVAL);
}

ZTEST(ndef_api, test_signature_parser_checks_type_and_chunk_state)
{
	static const uint8_t marker[] = {
		0xD1, 0x03, 0x05, 'S', 'i', 'g', 0x20, 0x00, 0x00, 0x00, 0x00,
	};
	struct ndef_record rec;
	struct ndef_sig sig;

	zassert_ok(ndef_record_decode(marker, sizeof(marker), &rec));
	zassert_ok(ndef_sig_parse(&rec, &sig));
	zassert_equal(sig.version, NDEF_SIG_VERSION_2_0);
	zassert_equal(sig.type, NDEF_SIG_TYPE_NONE);
	zassert_equal(sig.signature.len, 0U);
	zassert_equal(ndef_sig_parse(NULL, &sig), -EINVAL);
	zassert_equal(ndef_sig_parse(&rec, NULL), -EINVAL);
	rec.chunk_count = 2U;
	zassert_equal(ndef_sig_parse(&rec, &sig), -ENOTSUP);
	rec.chunk_count = 1U;
	rec.type = (const uint8_t *)"Zzz";
	zassert_equal(ndef_sig_parse(&rec, &sig), -ENOMSG);
}

ZTEST_SUITE(ndef_api, NULL, NULL, NULL, NULL, NULL);
