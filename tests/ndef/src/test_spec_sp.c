/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/rtd_sp.h>

/*
 * Independent octet vectors from NFCForum-SmartPoster_RTD_1.0, 2006-07-24.
 * References below use the specification's section and appendix numbering.
 * These vectors are not produced by the encoder under test.
 */

/* Appendix A.1, Table 4: 23 octets, including the outer NDEF header. */
static const uint8_t spec_sp_a1[] = {
	0xD1, 0x02, 0x12, 'S', 'p', 0xD1, 0x01, 0x0E, 'U', 0x01, 'n', 'f',
	'c',  '-',  'f',  'o', 'r', 'u',  'm',  '.',  'o', 'r',  'g',
};

/*
 * Appendix A.2, Table 5: preserve all tabulated octets, including 0x49.
 * Its final offset is 77, hence 78 octets total, not the prose's "69".
 */
static const uint8_t spec_sp_a2[] = {
	0xD1, 0x02, 0x49, 'S',  'p', 0x81, 0x01, 0x00, 0x00, 0x00, 0x0E, 'U',  0x01, 'n', 'f', 'c',
	'-',  'f',  'o',  'r',  'u', 'm',  '.',  'o',  'r',  'g',  0x11, 0x03, 0x01, 'a', 'c', 't',
	0x00, 0x11, 0x01, 0x12, 'T', 0x05, 'e',  'n',  '-',  'U',  'S',  'H',  'e',  'l', 'l', 'o',
	',',  ' ',  'w',  'o',  'r', 'l',  'd',  0x51, 0x01, 0x13, 'T',  0x02, 'f',  'i', 'M', 'o',
	'r',  'j',  'e',  'n',  's', ',',  ' ',  'm',  'a',  'a',  'i',  'l',  'm',  'a',
};

/* Minimal independently encoded nested message used by negative mutations. */
static const uint8_t spec_sp_uri_only[] = {0xD1, 0x01, 0x02, 'U', 0x00, 'x'};

ZTEST(ndef_spec_sp, test_appendix_a1_exact_octets)
{
	struct ndef_record rec;
	struct ndef_sp sp;
	char uri[32];

	zassert_equal(sizeof(spec_sp_a1), 23U);
	zassert_ok(ndef_record_decode(spec_sp_a1, sizeof(spec_sp_a1), &rec));
	zassert_equal(rec.raw_len, 23U);
	zassert_equal(rec.payload_len, 18U);
	zassert_equal(rec.type_len, 2U);
	zassert_mem_equal(rec.type, "Sp", 2U);
	zassert_ok(ndef_sp_parse(&rec, &sp));
	zassert_equal(sp.msg, &spec_sp_a1[5]);
	zassert_equal(sp.msg_len, 18U);
	zassert_ok(ndef_uri_to_string(&sp.uri, uri, sizeof(uri), NULL));
	zassert_str_equal(uri, "http://www.nfc-forum.org");
	zassert_equal(sp.title_count, 0U);
	zassert_equal(sp.icon_count, 0U);
	zassert_false(sp.has_action);
	zassert_false(sp.has_size);
	zassert_is_null(sp.type);
}

ZTEST(ndef_spec_sp, test_appendix_a2_long_and_short_records)
{
	struct ndef_record rec;
	struct ndef_sp sp;
	struct ndef_text title;

	zassert_equal(sizeof(spec_sp_a2), 78U);
	zassert_ok(ndef_record_decode(spec_sp_a2, sizeof(spec_sp_a2), &rec));
	zassert_equal(rec.payload_len, 73U);
	zassert_ok(ndef_sp_parse(&rec, &sp));
	zassert_equal(sp.title_count, 2U);
	zassert_true(sp.has_action);
	zassert_equal(sp.action, 0U);
	zassert_equal(sp.uri.code, 0x01U);
	zassert_equal(sp.uri.field_len, 13U);
	zassert_mem_equal(sp.uri.field, "nfc-forum.org", 13U);
	zassert_ok(ndef_sp_title_get(&sp, 0U, &title));
	zassert_equal(title.lang_len, 5U);
	zassert_mem_equal(title.lang, "en-US", 5U);
	zassert_equal(title.text_len, 12U);
	zassert_mem_equal(title.text, "Hello, world", 12U);
	zassert_ok(ndef_sp_title_get(&sp, 1U, &title));
	zassert_equal(title.lang_len, 2U);
	zassert_mem_equal(title.lang, "fi", 2U);
	zassert_equal(title.text_len, 16U);
	zassert_mem_equal(title.text, "Morjens, maailma", 16U);
	zassert_equal(ndef_sp_title_get(&sp, 2U, &title), -ENOENT);
}

/* Section 3.2.1 names the case-sensitive well-known type "Sp". */
ZTEST(ndef_spec_sp, test_outer_type_name)
{
	uint8_t message[sizeof(spec_sp_a1)];
	struct ndef_record rec;
	struct ndef_sp sp;

	memcpy(message, spec_sp_a1, sizeof(message));
	message[4] = 'P';
	zassert_ok(ndef_record_decode(message, sizeof(message), &rec));
	zassert_equal(ndef_sp_parse(&rec, &sp), -ENOMSG);
	message[3] = 's';
	message[4] = 'p';
	zassert_ok(ndef_record_decode(message, sizeof(message), &rec));
	zassert_equal(ndef_sp_parse(&rec, &sp), -ENOMSG);
}

/* Sections 3.2.1 and 3.3.1: exactly one URI; titles and metadata optional. */
ZTEST(ndef_spec_sp, test_uri_cardinality)
{
	static const uint8_t no_uri[] = {0xD1, 0x01, 0x04, 'T', 0x02, 'e', 'n', 'x'};
	static const uint8_t two_uris[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x02, 'U', 0x00, 'y',
	};
	static const uint8_t missing_uri_payload[] = {0xD1, 0x01, 0x00, 'U'};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(spec_sp_uri_only, sizeof(spec_sp_uri_only), &sp));
	zassert_equal(ndef_sp_payload_decode(no_uri, sizeof(no_uri), &sp), -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(two_uris, sizeof(two_uris), &sp), -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(missing_uri_payload, sizeof(missing_uri_payload), &sp),
		      -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(spec_sp_uri_only, 0U, &sp), -EBADMSG);
}

/* Section 3.2.2: no particular ordering of the nested records is assumed. */
ZTEST(ndef_spec_sp, test_metadata_before_uri)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x04, 'T',  0x02, 'e', 'n', 'A', 0x11, 0x01, 0x04, 's',  0x12, 0x34,
		0x56, 0x78, 0x11, 0x03, 0x01, 'a', 'c', 't', 0x02, 0x11, 0x01, 0x09, 't',  't',
		'e',  'x',  't',  '/',  'h',  't', 'm', 'l', 0x51, 0x01, 0x02, 'U',  0x00, 'x',
	};
	struct ndef_sp sp;
	struct ndef_text title;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_true(sp.has_action);
	zassert_equal(sp.action, 2U);
	zassert_true(sp.has_size);
	zassert_equal(sp.size, 0x12345678U);
	zassert_equal(sp.type_len, 9U);
	zassert_mem_equal(sp.type, "text/html", 9U);
	zassert_equal(sp.title_count, 1U);
	zassert_ok(ndef_sp_title_get(&sp, 0U, &title));
	zassert_mem_equal(title.text, "A", 1U);
}

/* Sections 3.2.1 and 3.3.2: language identifiers must not repeat. */
ZTEST(ndef_spec_sp, test_duplicate_title_language_case_insensitive)
{
	uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x01, 0x04, 'T', 0x02,
		'e',  'n',  'A',  0x51, 0x01, 0x04, 'T',  0x02, 'e',  'n', 'B',
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -EBADMSG);
	payload[19] = 'E';
	payload[20] = 'N';
	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -EBADMSG);
}

/* Distinct complete identifiers remain distinct, including regional tags. */
ZTEST(ndef_spec_sp, test_distinct_languages_and_exact_lookup)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x01, 0x07, 'T',  0x05, 'e',
		'n',  '-',  'U',  'S',  'A',  0x11, 0x01, 0x04, 'T',  0x02, 'e',  'n',
		'B',  0x51, 0x01, 0x07, 'T',  0x05, 'e',  'n',  '-',  'G',  'B',  'C',
	};
	struct ndef_sp sp;
	struct ndef_text title;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.title_count, 3U);
	zassert_ok(ndef_sp_title_find(&sp, "EN", &title));
	zassert_mem_equal(title.text, "B", 1U);
	zassert_ok(ndef_sp_title_find(&sp, "en-GB", &title));
	zassert_mem_equal(title.text, "C", 1U);
	zassert_equal(ndef_sp_title_find(&sp, "de", &title), -ENOENT);
}

/* Section 3.3.2 refers to the Text RTD, not arbitrary opaque title bytes. */
ZTEST(ndef_spec_sp, test_malformed_title_payload)
{
	static const uint8_t truncated_language[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x02, 'T', 0x02, 'e',
	};
	static const uint8_t invalid_utf8[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x04, 'T', 0x02, 'e', 'n', 0xFF,
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(truncated_language, sizeof(truncated_language), &sp),
		      -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(invalid_utf8, sizeof(invalid_utf8), &sp), -EBADMSG);
}

/* Section 3.3.3 and Table 2. RFU policy remains with the application. */
ZTEST(ndef_spec_sp, test_action_values_and_reserved_values)
{
	static const uint8_t values[] = {0x00, 0x01, 0x02, 0x03, 0x7F, 0xFF};
	uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x03, 0x01, 'a', 'c', 't', 0x00,
	};
	struct ndef_sp sp;

	for (size_t i = 0; i < sizeof(values); i++) {
		payload[sizeof(payload) - 1U] = values[i];
		zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
		zassert_true(sp.has_action);
		zassert_equal(sp.action, values[i]);
	}
}

/* Section 3.3.3: the content is one octet, neither empty nor multi-octet. */
ZTEST(ndef_spec_sp, test_action_payload_length)
{
	static const uint8_t empty[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x03, 0x00, 'a', 'c', 't',
	};
	static const uint8_t long_action[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x03, 0x02, 'a', 'c', 't', 0x00, 0x00,
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(empty, sizeof(empty), &sp), -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(long_action, sizeof(long_action), &sp), -EBADMSG);
}

/* Section 3.3.5 and Table 3: an unsigned 32-bit network-order integer. */
ZTEST(ndef_spec_sp, test_size_network_order_and_boundaries)
{
	uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x04, 's', 0x12, 0x34, 0x56, 0x78,
	};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_true(sp.has_size);
	zassert_equal(sp.size, 0x12345678U);
	memset(&payload[10], 0, 4U);
	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.size, 0U);
	memset(&payload[10], 0xFF, 4U);
	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.size, UINT32_MAX);
}

ZTEST(ndef_spec_sp, test_size_requires_four_octets)
{
	uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x00, 's', 0, 0, 0, 0, 0,
	};
	static const uint8_t lengths[] = {0, 1, 2, 3, 5};
	struct ndef_sp sp;

	for (size_t i = 0; i < sizeof(lengths); i++) {
		payload[8] = lengths[i];
		zassert_equal(ndef_sp_payload_decode(payload, 10U + lengths[i], &sp), -EBADMSG);
	}
}

/* Section 3.3.6: MIME type string occupies the payload, without a terminator. */
ZTEST(ndef_spec_sp, test_type_length_and_mime_parameters)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x18, 't', 'T', 'e',
		'x',  't',  '/',  'H', 'T',  'M', 'L',  ';',  'c',  'h', 'a', 'r',
		's',  'e',  't',  '=', 'u',  't', 'f',  '-',  '8',  ' ',
	};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.type_len, 24U);
	zassert_mem_equal(sp.type, "Text/HTML;charset=utf-8 ", 24U);
}

ZTEST(ndef_spec_sp, test_malformed_type_payload)
{
	static const uint8_t invalid[][11] = {
		{'h', 't', 'm', 'l'},
		{'t', 'e', 'x', 't', '/'},
		{'t', 'e', 'x', 't', '/', 'p', 'l', 'a', 'i', 'n', 0x00},
		{'t', 'e', 'x', 't', '/', 0xC3, 0x28},
	};
	static const uint8_t lengths[] = {4, 5, 11, 7};
	uint8_t payload[21] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x51, 0x01, 0x00, 't',
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(payload, 10U, &sp), -EBADMSG);
	for (size_t i = 0; i < sizeof(lengths); i++) {
		payload[8] = lengths[i];
		memcpy(&payload[10], invalid[i], lengths[i]);
		zassert_equal(ndef_sp_payload_decode(payload, 10U + lengths[i], &sp), -EBADMSG);
	}
}

/* Section 3.3.4: multiple image/video MIME records; other media are extras. */
ZTEST(ndef_spec_sp, test_multiple_image_and_video_icons)
{
	static const uint8_t payload[] = {
		0x92, 0x09, 0x01, 'I',  'M', 'A',  'G', 'E',  '/',  'P',  'N',  'G',
		0xA1, 0x11, 0x01, 0x02, 'U', 0x00, 'x', 0x12, 0x0A, 0x01, 't',  'e',
		'x',  't',  '/',  'v',  'c', 'a',  'r', 'd',  0xA2, 0x52, 0x0A, 0x01,
		'v',  'i',  'd',  'e',  'o', '/',  'm', 'p',  'e',  'g',  0xA3,
	};
	struct ndef_sp sp;
	struct ndef_record icon;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.icon_count, 2U);
	zassert_ok(ndef_sp_icon_get(&sp, 0U, &icon));
	zassert_equal(icon.payload[0], 0xA1U);
	zassert_ok(ndef_sp_icon_get(&sp, 1U, &icon));
	zassert_equal(icon.payload[0], 0xA3U);
	zassert_equal(ndef_sp_icon_get(&sp, 2U, &icon), -ENOENT);
}

/* A MIME prefix without a valid subtype does not describe an icon. */
ZTEST(ndef_spec_sp, test_invalid_mime_prefix_is_not_an_icon)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x', 0x12, 0x06, 0x01, 'i', 'm', 'a', 'g',  'e',
		'/',  0xA1, 0x52, 0x07, 0x01, 'v', 'i',  'd',  'e',  'o', '/', ';', 0xA2,
	};
	struct ndef_sp sp;
	struct ndef_record icon;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.icon_count, 0U);
	zassert_equal(ndef_sp_icon_get(&sp, 0U, &icon), -ENOENT);
}

/* Section 3.2.1 permits ignoring extras; do not recursively decode them. */
ZTEST(ndef_spec_sp, test_unknown_and_nested_extra_records)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x01, 0x01, 'q', 0xFF, 0x11, 0x02, 0x01,
		'Z',  'z',  0xFF, 0x14, 0x0D, 0x01, 'e',  'x',  'a',  'm', 'p',  'l',  'e',  '.',
		'c',  'o',  'm',  ':',  'x',  0xFF, 0x51, 0x02, 0x01, 'S', 'p',  0xFF,
	};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.title_count, 0U);
	zassert_equal(sp.icon_count, 0U);
}

/* NDEF 1.0 section 3.2.6: a reader treats reserved TNF as unknown. */
ZTEST(ndef_spec_sp, test_reserved_tnf_extra_is_opaque_to_reader)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x57, 0x01, 0x01, 'Z', 0xFF,
	};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
}

/* Section 3.2.1: the complete payload, not just its prefix, is a message. */
ZTEST(ndef_spec_sp, test_nested_message_must_consume_payload)
{
	uint8_t payload[sizeof(spec_sp_uri_only) * 2U];
	struct ndef_sp sp;

	memcpy(payload, spec_sp_uri_only, sizeof(spec_sp_uri_only));
	payload[sizeof(spec_sp_uri_only)] = 0x00;
	zassert_equal(ndef_sp_payload_decode(payload, sizeof(spec_sp_uri_only) + 1U, &sp),
		      -EBADMSG);
	memcpy(&payload[sizeof(spec_sp_uri_only)], spec_sp_uri_only, sizeof(spec_sp_uri_only));
	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -EBADMSG);
}

ZTEST(ndef_spec_sp, test_nested_message_flags_and_truncations)
{
	uint8_t payload[sizeof(spec_sp_uri_only)];
	struct ndef_sp sp;

	for (size_t i = 0; i < 18U; i++) {
		zassert_equal(ndef_sp_payload_decode(&spec_sp_a1[5], i, &sp), -EBADMSG,
			      "Appendix A.1 nested prefix length %zu", i);
	}
	for (size_t i = 0; i < 73U; i++) {
		zassert_equal(ndef_sp_payload_decode(&spec_sp_a2[5], i, &sp), -EBADMSG,
			      "Appendix A.2 nested prefix length %zu", i);
	}
	memcpy(payload, spec_sp_uri_only, sizeof(payload));
	payload[0] = 0x51; /* ME but no MB. */
	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -EBADMSG);
	payload[0] = 0x91; /* MB but no ME. */
	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -EBADMSG);
}

/*
 * Implementation policy: duplicate act/s/t are rejected. Unlike URI and
 * title language cardinalities, this is not an explicit SHALL in RTD-SP 1.0.
 */
ZTEST(ndef_spec_sp, test_duplicate_optional_metadata_policy)
{
	static const uint8_t action[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x03, 0x01, 'a',
		'c',  't',  0x00, 0x51, 0x03, 0x01, 'a',  'c',  't',  0x01,
	};
	static const uint8_t size[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x01, 0x04, 's', 0,
		0,    0,    0,    0x51, 0x01, 0x04, 's',  0,    0,    0,   1,
	};
	static const uint8_t type[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x11, 0x01, 0x03, 't',
		'a',  '/',  'b',  0x51, 0x01, 0x03, 't',  'a',  '/',  'c',
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(action, sizeof(action), &sp), -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(size, sizeof(size), &sp), -EBADMSG);
	zassert_equal(ndef_sp_payload_decode(type, sizeof(type), &sp), -EBADMSG);
}

/*
 * Supported NDEF chunking for icons is preserved as a logical record.
 * This does not claim support for chunked U/T/act/s/t typed views.
 */
ZTEST(ndef_spec_sp, test_chunked_icon_payload_access)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U', 0x00, 'x',  0x32, 0x09, 0x01, 'i', 'm', 'a', 'g',
		'e',  '/',  'p',  'n', 'g',  0x89, 0x56, 0x00, 0x03, 'P', 'N', 'G',
	};
	static const uint8_t expected[] = {0x89, 'P', 'N', 'G'};
	struct ndef_sp sp;
	struct ndef_record icon;
	uint8_t data[sizeof(expected)];

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.icon_count, 1U);
	zassert_ok(ndef_sp_icon_get(&sp, 0U, &icon));
	zassert_equal(icon.chunk_count, 2U);
	zassert_equal(icon.payload_len, 4U);
	zassert_ok(ndef_record_payload_read(&icon, 0U, data, sizeof(data)));
	zassert_mem_equal(data, expected, sizeof(data));
}

/* Valid NDEF chunked URI remains a documented implementation limitation. */
ZTEST(ndef_spec_sp, test_chunked_uri_reports_implementation_limit)
{
	static const uint8_t payload[] = {
		0xB1, 0x01, 0x01, 'U', 0x00, 0x56, 0x00, 0x01, 'x',
	};
	struct ndef_sp sp;

	zassert_equal(ndef_sp_payload_decode(payload, sizeof(payload), &sp), -ENOTSUP);
}

/* Additional types may be chunked and ignored, per section 3.2.1. */
ZTEST(ndef_spec_sp, test_unknown_chunked_extra_is_ignored)
{
	static const uint8_t payload[] = {
		0x91, 0x01, 0x02, 'U',  0x00, 'x',  0x31, 0x02,
		0x01, 'Z',  'z',  0xFF, 0x56, 0x00, 0x01, 0xFE,
	};
	struct ndef_sp sp;

	zassert_ok(ndef_sp_payload_decode(payload, sizeof(payload), &sp));
	zassert_equal(sp.icon_count, 0U);
	zassert_equal(sp.title_count, 0U);
}

/* Appendix A.1 also provides an independent oracle for the writer output. */
ZTEST(ndef_spec_sp, test_writer_matches_appendix_a1)
{
	struct ndef_writer w;
	struct ndef_writer sub;
	/* Streaming encoders reserve long headers before compacting them. */
	uint8_t output[64];
	size_t len;

	zassert_ok(ndef_writer_init(&w, output, sizeof(output)));
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_uri_write(&sub, "http://www.nfc-forum.org"));
	zassert_ok(ndef_sp_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(spec_sp_a1));
	zassert_mem_equal(output, spec_sp_a1, sizeof(spec_sp_a1));
}

/* Sections 3.3.3-3.3.6: wire layout checked against handwritten octets. */
ZTEST(ndef_spec_sp, test_writer_metadata_matches_independent_octets)
{
	static const uint8_t expected[] = {
		0xD1, 0x02, 0x2F, 'S', 'p',  0x91, 0x01, 0x02, 'U', 0x00, 'x',  0x11, 0x03,
		0x01, 'a',  'c',  't', 0x02, 0x11, 0x01, 0x04, 's', 0x12, 0x34, 0x56, 0x78,
		0x11, 0x01, 0x09, 't', 't',  'e',  'x',  't',  '/', 'h',  't',  'm',  'l',
		0x52, 0x09, 0x01, 'i', 'm',  'a',  'g',  'e',  '/', 'p',  'n',  'g',  0x89,
	};
	static const uint8_t icon[] = {0x89};
	struct ndef_writer w;
	struct ndef_writer sub;
	uint8_t output[96];
	size_t len;

	zassert_ok(ndef_writer_init(&w, output, sizeof(output)));
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_uri_write(&sub, "x"));
	zassert_ok(ndef_sp_action_write(&sub, NDEF_SP_ACTION_EDIT));
	zassert_ok(ndef_sp_size_write(&sub, 0x12345678U));
	zassert_ok(ndef_sp_type_write(&sub, "text/html"));
	zassert_ok(ndef_sp_icon_write(&sub, "image/png", icon, sizeof(icon)));
	zassert_ok(ndef_sp_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(sizeof(expected), 52U);
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(output, expected, sizeof(expected));
}

ZTEST_SUITE(ndef_spec_sp, NULL, NULL, NULL, NULL, NULL);
