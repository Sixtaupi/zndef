/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>

/*
 * Oracles are wire octets and field values derived from NFCForum-TS-NDEF_1.0,
 * not the output of another zndef encoder. Validation flags deliberately omit
 * RTD payload semantics: NDEF section 3.2.12 makes payloads opaque.
 */
#define STRUCTURE_AND_TYPES (NDEF_VALIDATE_EXACT | NDEF_VALIDATE_TYPES)

ZTEST(ndef_spec, test_normal_short_field_order)
{
	/* NDEF sections 3.1, 3.2: TYPE and ID follow all length fields. */
	static const uint8_t normal[] = {
		0xC9, 0x02, 0x00, 0x00, 0x00, 0x03, 0x02, 'X', 'Y', 'a', 'b', 0xDE, 0xAD, 0xBE,
	};
	static const uint8_t short_record[] = {
		0xD9, 0x02, 0x03, 0x02, 'X', 'Y', 'a', 'b', 0xDE, 0xAD, 0xBE,
	};
	static const uint8_t payload[] = {0xDE, 0xAD, 0xBE};
	struct ndef_record rec;

	zassert_ok(ndef_record_decode(normal, sizeof(normal), &rec));
	zassert_equal(rec.type_len, 2U);
	zassert_equal(rec.id_len, 2U);
	zassert_equal(rec.payload_len, 3U);
	zassert_equal(rec.type, normal + 7U);
	zassert_equal(rec.id, normal + 9U);
	zassert_equal(rec.payload, normal + 11U);
	zassert_mem_equal(rec.payload, payload, sizeof(payload));
	zassert_equal(rec.raw_len, sizeof(normal));
	zassert_ok(ndef_msg_validate(normal, sizeof(normal), STRUCTURE_AND_TYPES, NULL, NULL));

	zassert_ok(ndef_record_decode(short_record, sizeof(short_record), &rec));
	zassert_equal(rec.type, short_record + 4U);
	zassert_equal(rec.id, short_record + 6U);
	zassert_equal(rec.payload, short_record + 8U);
	zassert_mem_equal(rec.payload, payload, sizeof(payload));
	zassert_ok(ndef_msg_validate(short_record, sizeof(short_record), STRUCTURE_AND_TYPES, NULL,
				     NULL));

	for (size_t n = 0; n < sizeof(normal); n++) {
		zassert_equal(ndef_record_decode(normal, n, &rec), -EBADMSG, "length %zu", n);
	}
}

ZTEST(ndef_spec, test_mixed_short_normal_message)
{
	/* Sections 3.2.4, 4.3: both orders and both SR layouts are accepted. */
	static const uint8_t normal_first[] = {
		0x85, 0x00, 0x00, 0x00, 0x00, 0x01, 'A', 0x55, 0x00, 0x01, 'B',
	};
	static const uint8_t short_first[] = {
		0x95, 0x00, 0x01, 'A', 0x45, 0x00, 0x00, 0x00, 0x00, 0x01, 'B',
	};
	size_t len;
	uint32_t count;

	zassert_ok(ndef_msg_validate(normal_first, sizeof(normal_first), STRUCTURE_AND_TYPES, &len,
				     &count));
	zassert_equal(len, sizeof(normal_first));
	zassert_equal(count, 2U);
	zassert_ok(ndef_msg_validate(short_first, sizeof(short_first), STRUCTURE_AND_TYPES, &len,
				     &count));
	zassert_equal(len, sizeof(short_first));
	zassert_equal(count, 2U);
}

ZTEST(ndef_spec, test_all_header_flag_combinations)
{
	/*
	 * Exhaust all 32 combinations of MB, ME, CF, SR and IL for TNF=Unknown,
	 * empty payload, optional present-but-zero ID_LENGTH. Single-record decode
	 * does not impose message/chunk ordering (sections 2.3.1, 2.3.3).
	 */
	for (unsigned int flags = 0U; flags < 32U; flags++) {
		uint8_t wire[7] = {0};
		uint8_t header = (uint8_t)(0x05U | (flags << 3));
		size_t len = ((header & 0x10U) != 0U) ? 3U : 6U;
		struct ndef_record rec;
		int expected = -EBADMSG;

		wire[0] = header;
		if ((header & 0x08U) != 0U) {
			len++;
		}
		zassert_ok(ndef_record_decode(wire, len, &rec), "header %02x", header);
		zassert_equal(rec.raw_len, len);
		zassert_equal(rec.id_len, 0U);
		zassert_is_null(rec.id);
		if ((header & 0xE0U) == 0xC0U) {
			expected = 0;
		}
		zassert_equal(ndef_msg_validate(wire, len, STRUCTURE_AND_TYPES, NULL, NULL),
			      expected, "header %02x", header);
	}
}

ZTEST(ndef_spec, test_tnf_wire_matrix)
{
	/* Sections 3.2.6-3.2.11; the reserved-TNF parser fallback is a SHOULD. */
	static const struct {
		uint8_t wire[7];
		size_t len;
		int logical_result;
		enum ndef_tnf decoded_tnf;
	} vectors[] = {
		{{0xD0, 0x00, 0x00}, 3U, 0, NDEF_TNF_EMPTY},
		{{0xD1, 0x01, 0x00, 'X'}, 4U, 0, NDEF_TNF_WELL_KNOWN},
		{{0xD2, 0x03, 0x00, 'a', '/', 'b'}, 6U, 0, NDEF_TNF_MEDIA},
		{{0xD3, 0x02, 0x00, 'u', ':'}, 5U, 0, NDEF_TNF_ABSOLUTE_URI},
		{{0xD4, 0x03, 0x00, 'd', ':', 'x'}, 6U, 0, NDEF_TNF_EXTERNAL},
		{{0xD5, 0x00, 0x01, 0xFF}, 4U, 0, NDEF_TNF_UNKNOWN},
		{{0xD6, 0x00, 0x01, 0xFF}, 4U, -EBADMSG, NDEF_TNF_UNCHANGED},
		{{0xD7, 0x00, 0x01, 0xFF}, 4U, -EBADMSG, NDEF_TNF_UNKNOWN},
	};
	struct ndef_record rec;

	for (size_t n = 0; n < ARRAY_SIZE(vectors); n++) {
		zassert_ok(ndef_record_decode(vectors[n].wire, vectors[n].len, &rec), "%zu", n);
		zassert_equal(rec.tnf, vectors[n].decoded_tnf, "%zu", n);
		zassert_equal(ndef_msg_validate(vectors[n].wire, vectors[n].len,
						STRUCTURE_AND_TYPES, NULL, NULL),
			      vectors[n].logical_result, "%zu", n);
	}
}

ZTEST(ndef_spec, test_tnf_forbidden_fields)
{
	static const struct {
		uint8_t wire[7];
		size_t len;
	} invalid[] = {
		{{0xD0, 0x01, 0x00, 'X'}, 4U},
		{{0xD0, 0x00, 0x01, 'X'}, 4U},
		{{0xD8, 0x00, 0x00, 0x01, 'i'}, 5U},
		{{0xF0, 0x00, 0x00}, 3U},
		{{0xD1, 0x00, 0x00}, 3U},
		{{0xD2, 0x00, 0x00}, 3U},
		{{0xD3, 0x00, 0x00}, 3U},
		{{0xD4, 0x00, 0x00}, 3U},
		{{0xD5, 0x01, 0x00, 'X'}, 4U},
		{{0x16, 0x01, 0x00, 'X'}, 4U},
		{{0x1E, 0x00, 0x00, 0x00}, 4U},
		{{0x1E, 0x00, 0x00, 0x01, 'i'}, 5U},
	};
	static const uint8_t empty_with_zero_id_length[] = {0xD8, 0x00, 0x00, 0x00};
	struct ndef_record rec;

	for (size_t n = 0; n < ARRAY_SIZE(invalid); n++) {
		zassert_equal(ndef_record_decode(invalid[n].wire, invalid[n].len, &rec), -EBADMSG,
			      "vector %zu", n);
	}
	/* An ID_LENGTH of zero is allowed even when IL is set (3.2.8). */
	zassert_ok(ndef_msg_validate(empty_with_zero_id_length, sizeof(empty_with_zero_id_length),
				     STRUCTURE_AND_TYPES, NULL, NULL));
}

ZTEST(ndef_spec, test_writer_payload_size_boundaries)
{
	static const struct ndef_record_desc unknown = {.tnf = NDEF_TNF_UNKNOWN};
	static const struct {
		size_t len;
		uint8_t header[6];
		size_t header_len;
	} vectors[] = {
		{0U, {0xD5, 0x00, 0x00}, 3U},
		{1U, {0xD5, 0x00, 0x01}, 3U},
		{254U, {0xD5, 0x00, 0xFE}, 3U},
		{255U, {0xD5, 0x00, 0xFF}, 3U},
		{256U, {0xC5, 0x00, 0x00, 0x00, 0x01, 0x00}, 6U},
		{257U, {0xC5, 0x00, 0x00, 0x00, 0x01, 0x01}, 6U},
	};
	uint8_t payload[257];
	uint8_t buf[270];
	struct ndef_writer w;
	struct ndef_record rec;
	size_t len;

	memset(payload, 0xA5, sizeof(payload));
	for (size_t n = 0; n < ARRAY_SIZE(vectors); n++) {
		uint8_t *in_place;
		size_t cap;

		zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
		zassert_ok(ndef_writer_record_add(&w, &unknown, payload, vectors[n].len));
		zassert_ok(ndef_writer_finish(&w, &len));
		zassert_equal(len, vectors[n].header_len + vectors[n].len);
		zassert_mem_equal(buf, vectors[n].header, vectors[n].header_len);
		if (vectors[n].len != 0U) {
			zassert_mem_equal(buf + vectors[n].header_len, payload, vectors[n].len);
		}
		zassert_ok(ndef_record_decode(buf, len, &rec));
		zassert_equal(rec.payload_len, vectors[n].len);

		/* The in-place encoder must obey the same independent wire oracle. */
		zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
		zassert_ok(ndef_writer_record_begin(&w, &unknown, &in_place, &cap));
		zassert_true(cap >= vectors[n].len);
		memcpy(in_place, payload, vectors[n].len);
		zassert_ok(ndef_writer_record_end(&w, vectors[n].len));
		zassert_ok(ndef_writer_finish(&w, &len));
		zassert_equal(len, vectors[n].header_len + vectors[n].len);
		zassert_mem_equal(buf, vectors[n].header, vectors[n].header_len);
		if (vectors[n].len != 0U) {
			zassert_mem_equal(buf + vectors[n].header_len, payload, vectors[n].len);
		}
	}
}

ZTEST(ndef_spec, test_max_type_and_id_lengths)
{
	/* Sections 3.2.7, 3.2.8, 4.3: both lengths are unsigned 8-bit values. */
	static const uint8_t prefix[] = {0xD9, 0xFF, 0x01, 0xFF};
	uint8_t type[255];
	uint8_t id[255];
	uint8_t buf[515];
	const struct ndef_record_desc desc = {
		.tnf = NDEF_TNF_WELL_KNOWN,
		.type = type,
		.type_len = 255U,
		.id = id,
		.id_len = 255U,
	};
	struct ndef_writer w;
	struct ndef_record rec;
	size_t len;

	memset(type, 'X', sizeof(type));
	memset(id, 'i', sizeof(id));
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc, "Z", 1U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, 515U);
	zassert_mem_equal(buf, prefix, sizeof(prefix));
	zassert_mem_equal(buf + 4U, type, sizeof(type));
	zassert_mem_equal(buf + 259U, id, sizeof(id));
	zassert_equal(buf[514], 'Z');
	zassert_ok(ndef_record_decode(buf, len, &rec));
	zassert_equal(rec.type_len, 255U);
	zassert_equal(rec.id_len, 255U);
	zassert_equal(rec.payload_len, 1U);
	zassert_ok(ndef_msg_validate(buf, len, STRUCTURE_AND_TYPES, NULL, NULL));
	zassert_equal(ndef_record_decode(buf, 258U, &rec), -EBADMSG);
	zassert_equal(ndef_record_decode(buf, 514U, &rec), -EBADMSG);
}

ZTEST(ndef_spec, test_extreme_declared_lengths_are_bounded)
{
	/* A length is not evidence that its bytes exist: never read past input. */
	static const uint8_t max_payload[] = {0xC5, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
	static const uint8_t high_octet[] = {0xC5, 0x00, 0x01, 0x00, 0x00, 0x00, 'A'};
	static const uint8_t partial_long_length[] = {0xC5, 0x00, 0x00, 0x00, 0x00};
	struct ndef_record rec;

	zassert_equal(ndef_record_decode(max_payload, sizeof(max_payload), &rec), -EBADMSG);
	zassert_equal(ndef_record_decode(high_octet, sizeof(high_octet), &rec), -EBADMSG);
	zassert_equal(ndef_record_decode(partial_long_length, sizeof(partial_long_length), &rec),
		      -EBADMSG);
}

ZTEST(ndef_spec, test_chunk_writer_wire_and_logical_view)
{
	/* Sections 2.3.3, 3.2.6, 3.2.11: TYPE/ID occur in initial chunk only. */
	static const uint8_t expected[] = {
		0xB9, 0x01, 0x01, 0x01, 'X', 'i', 'A', 0x36, 0x00, 0x01, 'B', 0x56, 0x00, 0x01, 'C',
	};
	static const struct ndef_record_desc desc = {
		.tnf = NDEF_TNF_WELL_KNOWN,
		.type = "X",
		.type_len = 1U,
		.id = "i",
		.id_len = 1U,
	};
	struct ndef_writer w;
	struct ndef_msg_iter it;
	struct ndef_record rec;
	uint8_t buf[32];
	uint8_t payload[3];
	size_t len;
	uint32_t count;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_chunk_begin(&w, &desc, "A", 1U));
	zassert_ok(ndef_writer_chunk(&w, "B", 1U, false));
	zassert_ok(ndef_writer_chunk(&w, "C", 1U, true));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_ok(
		ndef_msg_validate(expected, sizeof(expected), STRUCTURE_AND_TYPES, NULL, &count));
	zassert_equal(count, 1U);
	zassert_ok(ndef_record_decode(expected, sizeof(expected), &rec));
	zassert_equal(rec.payload_len, 1U);
	zassert_equal(rec.raw_len, 7U);
	zassert_ok(ndef_msg_iter_init(&it, expected, sizeof(expected)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.chunk_count, 3U);
	zassert_equal(rec.payload_len, 3U);
	zassert_equal(rec.raw_len, sizeof(expected));
	zassert_ok(ndef_record_payload_read(&rec, 0U, payload, sizeof(payload)));
	zassert_mem_equal(payload, "ABC", 3U);
	zassert_ok(ndef_record_payload_read(&rec, 1U, payload, 2U));
	zassert_mem_equal(payload, "BC", 2U);
	zassert_equal(ndef_msg_iter_next(&it, &rec), -ENOENT);

	for (size_t n = 0; n < sizeof(expected); n++) {
		zassert_equal(ndef_msg_validate(expected, n, STRUCTURE_AND_TYPES, NULL, NULL),
			      -EBADMSG, "length %zu", n);
	}
}

ZTEST(ndef_spec, test_chunk_forbidden_message_flags)
{
	static const uint8_t valid[] = {
		0xB1, 0x01, 0x01, 'X', 'A', 0x36, 0x00, 0x01, 'B', 0x56, 0x00, 0x01, 'C',
	};
	static const struct {
		size_t offset;
		uint8_t header;
	} invalid[] = {
		{0U, 0x31}, /* Missing MB. */
		{0U, 0xF1}, /* ME on initial chunk. */
		{5U, 0xB6}, /* MB on middle chunk. */
		{5U, 0x76}, /* ME on middle chunk. */
		{5U, 0x35}, /* Middle TNF is not Unchanged. */
		{9U, 0xD6}, /* MB on terminating chunk. */
		{9U, 0x55}, /* Terminating TNF is not Unchanged. */
		{9U, 0x16}, /* No ME before EOF. */
		{9U, 0x76}, /* Last chunk still has CF. */
	};
	uint8_t buf[sizeof(valid)];

	for (size_t n = 0; n < ARRAY_SIZE(invalid); n++) {
		memcpy(buf, valid, sizeof(buf));
		buf[invalid[n].offset] = invalid[n].header;
		zassert_equal(ndef_msg_validate(buf, sizeof(buf), STRUCTURE_AND_TYPES, NULL, NULL),
			      -EBADMSG, "vector %zu", n);
	}
}

ZTEST(ndef_spec, test_zero_length_chunks_and_il_restriction)
{
	static const uint8_t zero_chunks[] = {0xB1, 0x01, 0x00, 'X', 0x56, 0x00, 0x00};
	static const uint8_t continuation_with_il[] = {
		0xB1, 0x01, 0x00, 'X', 0x5E, 0x00, 0x00, 0x00,
	};
	static const uint8_t continuation_with_type[] = {
		0xB1, 0x01, 0x00, 'X', 0x56, 0x01, 0x00, 'X',
	};
	struct ndef_msg_iter it;
	struct ndef_record rec;

	zassert_ok(ndef_msg_validate(zero_chunks, sizeof(zero_chunks), STRUCTURE_AND_TYPES, NULL,
				     NULL));
	zassert_ok(ndef_msg_iter_init(&it, zero_chunks, sizeof(zero_chunks)));
	zassert_ok(ndef_msg_iter_next(&it, &rec));
	zassert_equal(rec.chunk_count, 2U);
	zassert_equal(rec.payload_len, 0U);
	zassert_equal(ndef_msg_validate(continuation_with_il, sizeof(continuation_with_il),
					STRUCTURE_AND_TYPES, NULL, NULL),
		      -EBADMSG);
	zassert_equal(ndef_msg_validate(continuation_with_type, sizeof(continuation_with_type),
					STRUCTURE_AND_TYPES, NULL, NULL),
		      -EBADMSG);
}

ZTEST(ndef_spec, test_nested_message_wire_and_opaque_payload)
{
	/* Section 2.3.1 permits nesting in PAYLOAD, never through overlapping flags. */
	static const struct ndef_record_desc outer = {.tnf = NDEF_TNF_UNKNOWN};
	static const struct ndef_record_desc empty = {.tnf = NDEF_TNF_EMPTY};
	static const uint8_t expected[] = {0xD5, 0x00, 0x03, 0xD0, 0x00, 0x00};
	static const uint8_t opaque[] = {0xD5, 0x00, 0x01, 0xFF};
	uint8_t buf[16];
	struct ndef_writer w;
	struct ndef_writer sub;
	struct ndef_record rec;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_nested_begin(&w, &outer, NULL, 0U, &sub));
	zassert_ok(ndef_writer_record_add(&sub, &empty, NULL, 0U));
	zassert_ok(ndef_writer_nested_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_ok(ndef_record_decode(expected, sizeof(expected), &rec));
	zassert_ok(
		ndef_msg_validate(rec.payload, rec.payload_len, STRUCTURE_AND_TYPES, NULL, NULL));
	/* Section 3.2.12: generic NDEF validation cannot assign payload semantics. */
	zassert_ok(ndef_msg_validate(opaque, sizeof(opaque), STRUCTURE_AND_TYPES, NULL, NULL));
}

ZTEST_SUITE(ndef_spec, NULL, NULL, NULL, NULL, NULL);
