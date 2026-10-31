/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Independent vectors from NFCForum-TS-RTD_URI_1.0 (2006-07-24). */
#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>
#include <ndef/rtd_uri.h>

/* Sections 3.2.1/3.2.2, Table 3: independent transcription of all 36 codes. */
static const char *const spec_prefixes[] = {
	"",
	"http://www.",
	"https://www.",
	"http://",
	"https://",
	"tel:",
	"mailto:",
	"ftp://anonymous:anonymous@",
	"ftp://ftp.",
	"ftps://",
	"sftp://",
	"smb://",
	"nfs://",
	"ftp://",
	"dav://",
	"news:",
	"telnet://",
	"imap:",
	"rtsp://",
	"urn:",
	"pop:",
	"sip:",
	"sips:",
	"tftp:",
	"btspp://",
	"btl2cap://",
	"btgoep://",
	"tcpobex://",
	"irdaobex://",
	"file://",
	"urn:epc:id:",
	"urn:epc:tag:",
	"urn:epc:pat:",
	"urn:epc:raw:",
	"urn:epc:",
	"urn:nfc:",
};

ZTEST(ndef_spec_uri, test_appendix_a_canonical_octets)
{
	/* Annex A.1, A.2 and A.3: hex octets, not encodings produced by this library. */
	static const uint8_t url[] = {
		0xD1, 0x01, 0x08, 0x55, 0x01, 0x6E, 0x66, 0x63, 0x2E, 0x63, 0x6F, 0x6D,
	};
	static const uint8_t tel[] = {
		0xD1, 0x01, 0x0D, 0x55, 0x05, 0x2B, 0x33, 0x35, 0x38,
		0x39, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	};
	static const uint8_t proprietary[] = {
		0xD1, 0x01, 0x1F, 0x55, 0x00, 0x6D, 0x6D, 0x73, 0x3A, 0x2F, 0x2F, 0x65,
		0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x64,
		0x6F, 0x77, 0x6E, 0x6C, 0x6F, 0x61, 0x64, 0x2E, 0x77, 0x6D, 0x76,
	};
	const struct {
		const uint8_t *record;
		size_t len;
		const char *iri;
	} cases[] = {
		{url, sizeof(url), "http://www.nfc.com"},
		{tel, sizeof(tel), "tel:+35891234567"},
		{proprietary, sizeof(proprietary), "mms://example.com/download.wmv"},
	};
	uint8_t encoded[64];
	char converted[64];
	struct ndef_record rec;
	struct ndef_uri uri;
	struct ndef_writer writer;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		zassert_ok(ndef_record_decode(cases[i].record, cases[i].len, &rec));
		zassert_equal(rec.tnf, NDEF_TNF_WELL_KNOWN);
		zassert_equal(rec.type_len, 1U);
		zassert_equal(rec.type[0], 0x55);
		zassert_ok(ndef_uri_parse(&rec, &uri));
		zassert_ok(ndef_uri_to_string(&uri, converted, sizeof(converted), &len));
		zassert_equal(len, strlen(cases[i].iri));
		zassert_str_equal(converted, cases[i].iri);
		zassert_ok(ndef_uri_payload_encode(encoded, sizeof(encoded), cases[i].iri,
						   strlen(cases[i].iri), &len));
		zassert_equal(len, cases[i].len - 4U);
		zassert_mem_equal(encoded, &cases[i].record[4], len);
		zassert_ok(ndef_writer_init(&writer, encoded, sizeof(encoded)));
		zassert_ok(ndef_uri_write(&writer, cases[i].iri));
		zassert_ok(ndef_writer_finish(&writer, &len));
		zassert_equal(len, cases[i].len);
		zassert_mem_equal(encoded, cases[i].record, len);
	}
}

ZTEST(ndef_spec_uri, test_all_abbreviation_table_entries)
{
	uint8_t payload[] = {0x00, 'x'};
	uint8_t encoded[64];
	char full[64];
	char converted[64];
	struct ndef_uri uri;
	size_t len;

	zassert_equal(ARRAY_SIZE(spec_prefixes), 36U);
	for (size_t code = 0U; code < ARRAY_SIZE(spec_prefixes); code++) {
		size_t plen = strlen(spec_prefixes[code]);

		zassert_str_equal(ndef_uri_prefix((uint8_t)code), spec_prefixes[code]);
		payload[0] = (uint8_t)code;
		zassert_ok(ndef_uri_payload_decode(payload, sizeof(payload), &uri));
		zassert_equal(uri.code, code);
		zassert_str_equal(uri.prefix, spec_prefixes[code]);
		zassert_equal(uri.field_len, 1U);
		zassert_equal(uri.field[0], 'x');
		memcpy(full, spec_prefixes[code], plen);
		full[plen] = 'x';
		full[plen + 1U] = '\0';
		zassert_ok(ndef_uri_to_string(&uri, converted, sizeof(converted), &len));
		zassert_equal(len, plen + 1U);
		zassert_str_equal(converted, full);
		zassert_ok(
			ndef_uri_payload_encode(encoded, sizeof(encoded), full, plen + 1U, &len));
		zassert_equal(len, 2U);
		zassert_equal(encoded[0], code);
		zassert_equal(encoded[1], 'x');
	}
}

ZTEST(ndef_spec_uri, test_all_rfu_codes_are_no_prefix)
{
	/* Section 3.2.2: every RFU is decoded like zero; MUST NOT be emitted. */
	uint8_t payload[] = {0x24, 't', 'e', 'l', ':', '1'};
	uint8_t encoded[16];
	char converted[16];
	struct ndef_uri uri;
	size_t len;

	for (unsigned int code = 0x24U; code <= 0xFFU; code++) {
		payload[0] = (uint8_t)code;
		zassert_str_equal(ndef_uri_prefix((uint8_t)code), "");
		zassert_ok(ndef_uri_payload_decode(payload, sizeof(payload), &uri));
		zassert_str_equal(uri.prefix, "");
		zassert_ok(ndef_uri_to_string(&uri, converted, sizeof(converted), &len));
		zassert_equal(len, 5U);
		zassert_str_equal(converted, "tel:1");
		zassert_ok(ndef_uri_payload_encode(encoded, sizeof(encoded), converted, len, &len));
		zassert_equal(encoded[0], 0x05);
		zassert_equal(encoded[1], '1');
		zassert_equal(len, 2U);
	}
}

ZTEST(ndef_spec_uri, test_no_substitution_is_valid_with_known_scheme)
{
	/* Section 3.2.2 permits code zero even when an abbreviation exists. */
	static const uint8_t payload[] = {
		0x00, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w',
		'w',  '.', 'n', 'f', 'c', '.', 'c', 'o', 'm',
	};
	struct ndef_uri uri;
	char converted[32];
	size_t len;

	zassert_ok(ndef_uri_payload_decode(payload, sizeof(payload), &uri));
	zassert_equal(uri.code, 0U);
	zassert_str_equal(uri.prefix, "");
	zassert_equal(uri.field_len, sizeof(payload) - 1U);
	zassert_ok(ndef_uri_to_string(&uri, converted, sizeof(converted), &len));
	zassert_equal(len, 18U);
	zassert_str_equal(converted, "http://www.nfc.com");
}

ZTEST(ndef_spec_uri, test_unicode_iri_is_preserved_as_utf8)
{
	/* Section 3.2.3's hääyö example. IDNA conversion belongs to the application. */
	static const uint8_t payload[] = {
		0x01, 'h', 0xC3, 0xA4, 0xC3, 0xA4, 'y', 0xC3, 0xB6, '.', 'c', 'o', 'm', '/',
	};
	static const char iri[] = "http://www.h\xC3\xA4\xC3\xA4y\xC3\xB6.com/";
	uint8_t encoded[32];
	char converted[32];
	struct ndef_uri uri;
	size_t len;

	zassert_ok(ndef_uri_payload_decode(payload, sizeof(payload), &uri));
	zassert_equal(uri.field_len, sizeof(payload) - 1U);
	zassert_ok(ndef_uri_to_string(&uri, converted, sizeof(converted), &len));
	zassert_equal(len, sizeof(iri) - 1U);
	zassert_mem_equal(converted, iri, sizeof(iri));
	zassert_ok(ndef_uri_payload_encode(encoded, sizeof(encoded), iri, sizeof(iri) - 1U, &len));
	zassert_equal(len, sizeof(payload));
	zassert_mem_equal(encoded, payload, sizeof(payload));
}

ZTEST(ndef_spec_uri, test_every_c0_control_discards_record)
{
	/* Section 3.2.3: inclusive range 0..31, including an embedded NUL. */
	uint8_t payload[] = {0x00, 'x', 0x00, 'y'};
	uint8_t encoded[8];
	struct ndef_uri uri;
	size_t len;

	for (unsigned int cp = 0U; cp <= 31U; cp++) {
		payload[2] = (uint8_t)cp;
		zassert_equal(ndef_uri_payload_decode(payload, sizeof(payload), &uri), -EBADMSG,
			      "control U+%04X", cp);
		zassert_equal(ndef_uri_payload_encode(encoded, sizeof(encoded),
						      (const char *)&payload[1], 3U, &len),
			      -EINVAL, "control U+%04X", cp);
	}
}

ZTEST(ndef_spec_uri, test_invalid_utf8_discards_record)
{
	/* Section 3.2.3: invalid UTF-8 MUST cause the entire record to be discarded. */
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
	uint8_t payload[5];
	uint8_t encoded[8];
	struct ndef_uri uri;
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		payload[0] = 0x00;
		memcpy(&payload[1], cases[i].bytes, cases[i].len);
		zassert_equal(ndef_uri_payload_decode(payload, 1U + cases[i].len, &uri), -EBADMSG,
			      "UTF-8 case %zu", i);
		zassert_equal(ndef_uri_payload_encode(encoded, sizeof(encoded),
						      (const char *)cases[i].bytes, cases[i].len,
						      &len),
			      -EINVAL, "UTF-8 case %zu", i);
	}
}

ZTEST(ndef_spec_uri, test_longest_prefix_selection_and_case_preservation)
{
	/* Longest prefix is this library's policy; the specification also permits zero. */
	static const struct {
		const char *iri;
		uint8_t code;
		const char *field;
	} cases[] = {
		{"http://www.x", 0x01, "x"},
		{"https://www.x", 0x02, "x"},
		{"http://x", 0x03, "x"},
		{"https://x", 0x04, "x"},
		{"ftp://anonymous:anonymous@x", 0x07, "x"},
		{"ftp://ftp.x", 0x08, "x"},
		{"ftp://x", 0x0D, "x"},
		{"urn:epc:id:x", 0x1E, "x"},
		{"urn:epc:tag:x", 0x1F, "x"},
		{"urn:epc:pat:x", 0x20, "x"},
		{"urn:epc:raw:x", 0x21, "x"},
		{"urn:epc:x", 0x22, "x"},
		{"urn:nfc:x", 0x23, "x"},
		{"urn:isbn:1", 0x13, "isbn:1"},
		{"HTTP://www.x", 0x00, "HTTP://www.x"},
		{"custom:x", 0x00, "custom:x"},
	};
	uint8_t encoded[64];
	size_t len;

	for (size_t i = 0U; i < ARRAY_SIZE(cases); i++) {
		zassert_ok(ndef_uri_payload_encode(encoded, sizeof(encoded), cases[i].iri,
						   strlen(cases[i].iri), &len));
		zassert_equal(encoded[0], cases[i].code);
		zassert_equal(len, 1U + strlen(cases[i].field));
		zassert_mem_equal(&encoded[1], cases[i].field, strlen(cases[i].field));
	}
}

ZTEST(ndef_spec_uri, test_framing_empty_fields_and_capacity)
{
	static const uint8_t empty[] = {0x00};
	static const uint8_t prefix_only[] = {0x04};
	static const uint8_t tel[] = {0x05, '+', '1'};
	struct ndef_uri uri;
	uint8_t encoded[3];
	char converted[16];
	size_t len = 123U;

	/* Empty fields are structurally representable; scheme semantics are external. */
	zassert_equal(ndef_uri_payload_decode(empty, 0U, &uri), -EBADMSG);
	zassert_ok(ndef_uri_payload_decode(empty, sizeof(empty), &uri));
	zassert_equal(uri.field_len, 0U);
	zassert_ok(ndef_uri_to_string(&uri, converted, 1U, &len));
	zassert_equal(len, 0U);
	zassert_equal(converted[0], '\0');
	zassert_ok(ndef_uri_payload_encode(encoded, 1U, NULL, 0U, &len));
	zassert_equal(len, 1U);
	zassert_equal(encoded[0], 0U);
	zassert_ok(ndef_uri_payload_decode(prefix_only, sizeof(prefix_only), &uri));
	zassert_ok(ndef_uri_to_string(&uri, converted, 9U, &len));
	zassert_equal(len, 8U);
	zassert_mem_equal(converted, "https://", 8U);

	zassert_ok(ndef_uri_payload_decode(tel, sizeof(tel), &uri));
	len = 123U;
	zassert_equal(ndef_uri_to_string(&uri, converted, 6U, &len), -ENOSPC);
	zassert_equal(len, 123U);
	zassert_ok(ndef_uri_to_string(&uri, converted, 7U, &len));
	zassert_equal(len, 6U);
	zassert_str_equal(converted, "tel:+1");
	zassert_equal(ndef_uri_to_string(&uri, converted, 0U, NULL), -ENOSPC);
	zassert_equal(ndef_uri_payload_encode(encoded, 0U, "tel:+1", 6U, &len), -ENOSPC);
	zassert_equal(ndef_uri_payload_encode(encoded, 2U, "tel:+1", 6U, &len), -ENOSPC);
	zassert_ok(ndef_uri_payload_encode(encoded, 3U, "tel:+1", 6U, &len));
	zassert_equal(len, sizeof(tel));
	zassert_mem_equal(encoded, tel, sizeof(tel));
}

ZTEST(ndef_spec_uri, test_record_type_chunking_and_invalid_arguments)
{
	static const uint8_t message[] = {0xD1, 0x01, 0x02, 'U', 0x00, 'x'};
	struct ndef_record rec;
	struct ndef_uri uri;
	struct ndef_writer writer;
	uint8_t encoded[16];
	char converted[16];
	size_t len = 123U;

	zassert_ok(ndef_record_decode(message, sizeof(message), &rec));
	rec.chunk_count = 2U;
	zassert_equal(ndef_uri_parse(&rec, &uri), -ENOTSUP);
	rec.chunk_count = 1U;
	rec.tnf = NDEF_TNF_MEDIA;
	zassert_equal(ndef_uri_parse(&rec, &uri), -ENOMSG);
	rec.tnf = NDEF_TNF_WELL_KNOWN;
	rec.type = (const uint8_t *)"u";
	zassert_equal(ndef_uri_parse(&rec, &uri), -ENOMSG);
	zassert_equal(ndef_uri_parse(NULL, &uri), -EINVAL);
	zassert_equal(ndef_uri_payload_decode(NULL, 0U, &uri), -EINVAL);
	zassert_equal(ndef_uri_payload_decode(&message[4], 2U, NULL), -EINVAL);
	zassert_equal(ndef_uri_payload_encode(NULL, 16U, "x", 1U, &len), -EINVAL);
	zassert_equal(ndef_uri_payload_encode(encoded, sizeof(encoded), NULL, 1U, &len), -EINVAL);
	zassert_equal(ndef_uri_payload_encode(encoded, sizeof(encoded), "x", 1U, NULL), -EINVAL);
	zassert_equal(len, 123U);
	zassert_ok(ndef_uri_payload_decode(&message[4], 2U, &uri));
	zassert_equal(ndef_uri_to_string(NULL, converted, sizeof(converted), NULL), -EINVAL);
	zassert_equal(ndef_uri_to_string(&uri, NULL, sizeof(converted), NULL), -EINVAL);
	uri.prefix = NULL;
	zassert_equal(ndef_uri_to_string(&uri, converted, sizeof(converted), NULL), -EINVAL);

	zassert_ok(ndef_writer_init(&writer, encoded, sizeof(encoded)));
	zassert_equal(ndef_uri_write(&writer, NULL), -EINVAL);
	zassert_equal(ndef_writer_len(&writer), 0U);
}

ZTEST_SUITE(ndef_spec_uri, NULL, NULL, NULL, NULL, NULL);
