/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>
#include <ndef/rtd_di.h>
#include <ndef/rtd_handover.h>
#include <ndef/rtd_sig.h>
#include <ndef/rtd_sp.h>
#include <ndef/rtd_text.h>
#include <ndef/rtd_uri.h>
#include <ndef/tlv.h>

static uint8_t area[600];

ZTEST(ndef_tlv, test_find_ndef)
{
	static const uint8_t t2t[] = {
		0x00, 0x01, 0x03, 0xA0, 0x10, 0x44, 0x03, 0x05,
		0xD1, 0x01, 0x01, 'U',  0x00, 0xFE, 0x00, 0x00,
	};
	struct ndef_tlv_iter it;
	struct ndef_tlv tlv;
	const uint8_t *msg;
	size_t len;

	zassert_ok(ndef_tlv_find_ndef(t2t, sizeof(t2t), &msg, &len));
	zassert_equal(msg, &t2t[8]);
	zassert_equal(len, 5U);
	zassert_ok(ndef_msg_validate(msg, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_ok(ndef_tlv_iter_init(&it, t2t, sizeof(t2t)));
	zassert_ok(ndef_tlv_iter_next(&it, &tlv));
	zassert_equal(tlv.type, NDEF_TLV_NULL);
	zassert_ok(ndef_tlv_iter_next(&it, &tlv));
	zassert_equal(tlv.type, NDEF_TLV_LOCK_CONTROL);
	zassert_equal(tlv.len, 3U);
	zassert_ok(ndef_tlv_iter_next(&it, &tlv));
	zassert_equal(tlv.type, NDEF_TLV_NDEF);
	zassert_equal(tlv.offset, 6U);
	zassert_ok(ndef_tlv_iter_next(&it, &tlv));
	zassert_equal(tlv.type, NDEF_TLV_TERMINATOR);
	zassert_equal(ndef_tlv_iter_next(&it, &tlv), -ENOENT);

	/* Empty NDEF TLV: initialized tag without message. */
	zassert_ok(ndef_tlv_find_ndef((const uint8_t *)"\x03\x00\xfe", 3U, &msg, &len));
	zassert_is_null(msg);
	zassert_equal(len, 0U);

	zassert_equal(ndef_tlv_find_ndef((const uint8_t *)"\xfe\x03\x00", 3U, &msg, &len), -ENOENT);
	zassert_equal(ndef_tlv_find_ndef((const uint8_t *)"\x03\x05\xd1", 3U, &msg, &len),
		      -EBADMSG);
	zassert_equal(ndef_tlv_find_ndef((const uint8_t *)"\x03\xff\x01", 3U, &msg, &len),
		      -EBADMSG);
	zassert_equal(ndef_tlv_find_ndef((const uint8_t *)"\x03\xff\xff\xff", 4U, &msg, &len),
		      -EBADMSG);
}

ZTEST(ndef_tlv, test_wrap)
{
	const uint8_t *msg;
	size_t len;
	size_t out;

	/* Message already at the start of the area: moved in place. */
	memset(area, 0xAA, sizeof(area));
	area[0] = 0xD1;
	zassert_ok(ndef_tlv_wrap(area, sizeof(area), area, 300U, true, &out));
	zassert_equal(out, 4U + 300U + 1U);
	zassert_mem_equal(area, "\x03\xff\x01\x2c\xd1\xaa", 6U);
	zassert_equal(area[304], NDEF_TLV_TERMINATOR);
	zassert_ok(ndef_tlv_find_ndef(area, out, &msg, &len));
	zassert_equal(len, 300U);

	memset(area, 0, sizeof(area));
	zassert_ok(ndef_tlv_wrap(area, 8U, (const uint8_t *)"\xd0\x00\x00", 3U, false, &out));
	zassert_equal(out, 5U);
	zassert_mem_equal(area, "\x03\x03\xd0\x00\x00", 5U);
	zassert_equal(ndef_tlv_wrap(area, 5U, area, 3U, true, &out), -ENOSPC);

	zassert_ok(ndef_tlv_header_encode(area, 4U, NDEF_TLV_PROPRIETARY, 0xFEU, &out));
	zassert_equal(out, 2U);
	zassert_ok(ndef_tlv_header_encode(area, 4U, NDEF_TLV_PROPRIETARY, 0xFFU, &out));
	zassert_equal(out, 4U);
	zassert_equal(ndef_tlv_header_encode(area, 4U, NDEF_TLV_NDEF, 0xFFFFU, &out), -EINVAL);
	zassert_equal(ndef_tlv_header_encode(area, 4U, NDEF_TLV_TERMINATOR, 0U, &out), -EINVAL);
	zassert_equal(ndef_tlv_header_encode(area, 3U, NDEF_TLV_NDEF, 0x100U, &out), -ENOSPC);
}

ZTEST_SUITE(ndef_tlv, NULL, NULL, NULL, NULL, NULL);

/*
 * Robustness: random mutations of valid messages must never make a decoder
 * read out of bounds or loop forever. Run on a host with AddressSanitizer
 * for full effect.
 */

static uint32_t rng_state;

static uint32_t rng(void)
{
	/* xorshift32 */
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;

	return rng_state;
}

static int ignore_data(const uint8_t *data, size_t len, void *user_data)
{
	(void)data;
	(void)len;
	(void)user_data;

	return 0;
}

static int ignore_record(const struct ndef_record *rec, void *user_data)
{
	return ndef_sig_record_data(rec, ignore_data, user_data);
}

static void exercise(const uint8_t *buf, size_t len)
{
	static char text[256];
	static uint8_t payload[256];
	struct ndef_msg_iter it;
	struct ndef_record rec;
	const uint8_t *msg;
	size_t msg_len;
	union {
		struct ndef_text text;
		struct ndef_uri uri;
		struct ndef_sp sp;
		struct ndef_sig sig;
		struct ndef_ho ho;
		struct ndef_ho_ac ac;
		struct ndef_ho_carrier hc;
		struct ndef_di di;
	} u;

	(void)ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL);
	(void)ndef_tlv_find_ndef(buf, len, &msg, &msg_len);
	(void)ndef_text_payload_decode(buf, len, &u.text);
	(void)ndef_sp_payload_decode(buf, len, &u.sp);
	(void)ndef_sig_payload_decode(buf, len, &u.sig);
	(void)ndef_ho_payload_decode(NDEF_HO_SELECT, buf, len, &u.ho);
	(void)ndef_ho_ac_payload_decode(buf, len, &u.ac);
	(void)ndef_ho_carrier_payload_decode(buf, len, &u.hc);
	(void)ndef_di_payload_decode(buf, len, &u.di);

	(void)ndef_msg_iter_init(&it, buf, len);
	while (ndef_msg_iter_next(&it, &rec) == 0) {
		(void)ndef_record_reassemble(&rec, payload, sizeof(payload));
		if (ndef_text_parse(&rec, &u.text) == 0) {
			(void)ndef_text_to_utf8(&u.text, text, sizeof(text), NULL);
		}
		if (ndef_uri_parse(&rec, &u.uri) == 0) {
			(void)ndef_uri_to_string(&u.uri, text, sizeof(text), NULL);
		}
		if (ndef_sp_parse(&rec, &u.sp) == 0) {
			struct ndef_text title;
			struct ndef_record icon;

			(void)ndef_sp_title_find(&u.sp, "en", &title);
			(void)ndef_sp_icon_get(&u.sp, 0U, &icon);
		}
		if (ndef_sig_parse(&rec, &u.sig) == 0) {
			struct ndef_sig_blob cert;

			(void)ndef_sig_cert_get(&u.sig, u.sig.cert_count - 1U, &cert);
			(void)ndef_sig_foreach_signed(buf, len, ndef_msg_iter_index(&it),
						      ignore_record, NULL);
		}
		if (ndef_ho_parse(&rec, NULL, &u.ho) == 0) {
			struct ndef_ho_ac ac;
			struct ndef_ho_ref ref;

			if (ndef_ho_ac_get(&u.ho, 0U, &ac) == 0) {
				(void)ndef_ho_ac_aux_get(&ac, (uint8_t)(ac.aux_count - 1U), &ref);
			}
		}
		if (ndef_di_parse(&rec, &u.di) == 0) {
			struct ndef_di_elem e;

			(void)ndef_di_elem_get(&u.di, u.di.count - 1U, &e);
		}
	}
}

static size_t build_corpus(uint8_t *buf, size_t size)
{
	static const struct ndef_record_desc media = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "a/b");
	static const struct ndef_ho_ref carrier = {.id = "0", .len = 1U};
	static const struct ndef_di_elem di[] = {
		{.type = NDEF_DI_MANUFACTURER, .len = 1U, .value = (const uint8_t *)"A"},
		{.type = NDEF_DI_MODEL, .len = 1U, .value = (const uint8_t *)"B"},
	};
	static const struct ndef_sig_blob cert = {.data = (const uint8_t *)"C", .len = 1U};
	const struct ndef_sig sig = {
		.version = NDEF_SIG_VERSION_2_0,
		.type = NDEF_SIG_TYPE_ECDSA_P256,
		.hash = NDEF_SIG_HASH_SHA256,
		.signature = {.data = (const uint8_t *)"S", .len = 1U},
		.cert_count = 1U,
		.certs = &cert,
	};
	struct ndef_writer w;
	struct ndef_writer sub;
	size_t len = 0;

	(void)ndef_writer_init(&w, buf, size);
	(void)ndef_sp_begin(&w, &sub);
	(void)ndef_uri_write(&sub, "https://x");
	(void)ndef_text_write(&sub, "en", "t");
	(void)ndef_sp_action_write(&sub, NDEF_SP_ACTION_DO);
	(void)ndef_sp_icon_write(&sub, "image/png", "i", 1U);
	(void)ndef_sp_end(&w, &sub);
	(void)ndef_writer_chunk_begin(&w, &media, "12", 2U);
	(void)ndef_writer_chunk(&w, "3", 1U, false);
	(void)ndef_writer_chunk(&w, "4", 1U, true);
	(void)ndef_ho_begin(&w, NDEF_HO_SELECT, NDEF_HO_VERSION_DEFAULT, &sub);
	(void)ndef_ho_ac_write(&sub, NDEF_HO_CPS_ACTIVE, &carrier, &carrier, 1U);
	(void)ndef_ho_err_write(&sub, NDEF_HO_ERR_TEMP_MEMORY, 10U);
	(void)ndef_ho_end(&w, &sub);
	(void)ndef_di_write(&w, di, 2U);
	(void)ndef_sig_write(&w, &sig);
	(void)ndef_writer_finish(&w, &len);

	return len;
}

ZTEST(ndef_tlv, test_fuzz_mutations)
{
	static uint8_t corpus[256];
	static uint8_t mutated[256];
	size_t len = build_corpus(corpus, sizeof(corpus));

	zassert_true(len > 0U);
	zassert_ok(ndef_msg_validate(corpus, len, NDEF_VALIDATE_ALL, NULL, NULL));

	rng_state = 0x12345678U;

	for (uint32_t round = 0; round < CONFIG_TEST_NDEF_FUZZ_ROUNDS; round++) {
		size_t n = len;
		uint32_t flips = 1U + rng() % 4U;

		memcpy(mutated, corpus, len);
		for (uint32_t k = 0; k < flips; k++) {
			mutated[rng() % len] ^= (uint8_t)(1U << (rng() % 8U));
		}
		if ((rng() & 3U) == 0U) {
			n = rng() % (len + 1U);
		}

		/* Exact sized copy at the end of the buffer catches over-reads. */
		memmove(&mutated[sizeof(mutated) - n], mutated, n);
		exercise(&mutated[sizeof(mutated) - n], n);

		/* Pure noise. */
		for (size_t i = 0; i < len; i++) {
			mutated[i] = (uint8_t)rng();
		}
		exercise(mutated, rng() % (len + 1U));
	}
}
