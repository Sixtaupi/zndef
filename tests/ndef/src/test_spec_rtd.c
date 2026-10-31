/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/ndef.h>

/* Literal alphabets below come from NFCForum-TS-RTD_1.0 chapter 3 ABNF. */
static const char first_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static const char wkt_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789()+,-:=@;$_!*'.";
static const char dns_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-";

static bool alphabet_contains(const char *alphabet, unsigned int octet)
{
	return octet != 0U && octet < 128U && strchr(alphabet, (int)octet) != NULL;
}

ZTEST(rtd_spec, test_well_known_ascii_alphabet_exhaustive)
{
	/* Sections 2.1.1, 2.1.2, 3 and appendix A: all 256 octet values. */
	for (unsigned int c = 0U; c < 256U; c++) {
		uint8_t first[] = {(uint8_t)c};
		uint8_t suffix[] = {'A', (uint8_t)c};
		int first_result = alphabet_contains(first_alphabet, c) ? 0 : -EINVAL;
		int suffix_result = alphabet_contains(wkt_alphabet, c) ? 0 : -EINVAL;

		zassert_equal(ndef_type_validate(NDEF_TNF_WELL_KNOWN, first, sizeof(first)),
			      first_result, "first octet %02x", c);
		zassert_equal(ndef_type_validate(NDEF_TNF_WELL_KNOWN, suffix, sizeof(suffix)),
			      suffix_result, "suffix octet %02x", c);
	}
}

ZTEST(rtd_spec, test_external_ascii_alphabet_exhaustive)
{
	/*
	 * Chapter 3 permits dots and hyphens anywhere in its DNS-char grammar;
	 * this test intentionally does not invent a stricter DNS-label grammar.
	 */
	for (unsigned int c = 0U; c < 256U; c++) {
		uint8_t domain[] = {(uint8_t)c, ':', 'x'};
		uint8_t name[] = {'d', ':', (uint8_t)c};
		int domain_result = alphabet_contains(dns_alphabet, c) ? 0 : -EINVAL;
		int name_result = alphabet_contains(wkt_alphabet, c) ? 0 : -EINVAL;

		zassert_equal(ndef_type_validate(NDEF_TNF_EXTERNAL, domain, sizeof(domain)),
			      domain_result, "domain octet %02x", c);
		zassert_equal(ndef_type_validate(NDEF_TNF_EXTERNAL, name, sizeof(name)),
			      name_result, "name octet %02x", c);
	}
}

ZTEST(rtd_spec, test_external_required_parts)
{
	static const char *const invalid[] = {"", ":", ":x", "d:", "domain", "a_b:x"};
	static const char *const valid[] = {
		"example.com:f", "android.com:pkg", "d:x", "d::", "d:()+,-:=@;$_!*'.",
	};

	for (size_t n = 0; n < ARRAY_SIZE(invalid); n++) {
		zassert_equal(ndef_type_validate(NDEF_TNF_EXTERNAL, (const uint8_t *)invalid[n],
						 strlen(invalid[n])),
			      -EINVAL, "%s", invalid[n]);
	}
	for (size_t n = 0; n < ARRAY_SIZE(valid); n++) {
		zassert_ok(ndef_type_validate(NDEF_TNF_EXTERNAL, (const uint8_t *)valid[n],
					      strlen(valid[n])),
			   "%s", valid[n]);
	}
}

ZTEST(rtd_spec, test_well_known_percent_encoding_forbidden)
{
	/* Section 3.2 forbids percent encoding for both global and local types. */
	static const char *const names[] = {"A%41", "a%41", "0%41", "A%C3%A9", "a%2f"};

	for (size_t n = 0; n < ARRAY_SIZE(names); n++) {
		zassert_equal(ndef_type_validate(NDEF_TNF_WELL_KNOWN, (const uint8_t *)names[n],
						 strlen(names[n])),
			      -EINVAL, "%s", names[n]);
	}
}

ZTEST(rtd_spec, test_external_percent_encoding_utf8)
{
	/*
	 * Section 3.2 requires UTF-8 before percent conversion. Explicit scalar
	 * boundaries detect isolated continuations, truncation, overlong forms,
	 * surrogate encodings, and values beyond the Unicode scalar range.
	 */
	static const char *const valid[] = {
		"d:%41",       "d:a%C3%A9",      "d:%c3%a9",       "d:%C2%80",
		"d:%DF%BF",    "d:%E0%A0%80",    "d:%ED%9F%BF",    "d:%EE%80%80",
		"d:%EF%BF%BF", "d:%F0%90%80%80", "d:%F4%8F%BF%BF",
	};
	static const char *const invalid[] = {
		"d:%",
		"d:%4",
		"d:%4G",
		"d:%GG",
		"d:%80",
		"d:%BF",
		"d:%C0%80",
		"d:%C1%BF",
		"d:%C2",
		"d:%C2A",
		"d:%C2%20",
		"d:%E0%9F%BF",
		"d:%ED%A0%80",
		"d:%EF%BF",
		"d:%F0%8F%BF%BF",
		"d:%F4%90%80%80",
		"d:%F5%80%80%80",
		"d:%FE",
		"d:%FF",
	};

	for (size_t n = 0; n < ARRAY_SIZE(valid); n++) {
		zassert_ok(ndef_type_validate(NDEF_TNF_EXTERNAL, (const uint8_t *)valid[n],
					      strlen(valid[n])),
			   "%s", valid[n]);
	}
	for (size_t n = 0; n < ARRAY_SIZE(invalid); n++) {
		zassert_equal(ndef_type_validate(NDEF_TNF_EXTERNAL, (const uint8_t *)invalid[n],
						 strlen(invalid[n])),
			      -EINVAL, "%s", invalid[n]);
	}
}

ZTEST(rtd_spec, test_global_local_classification)
{
	static const char *const global[] = {"U", "Cfq", "Trip-to-Texas"};
	static const char *const local[] = {"0", "foo", "u", "9x"};

	for (size_t n = 0; n < ARRAY_SIZE(global); n++) {
		zassert_ok(ndef_type_validate(NDEF_TNF_WELL_KNOWN, (const uint8_t *)global[n],
					      strlen(global[n])));
		zassert_false(ndef_wkt_is_local((const uint8_t *)global[n], strlen(global[n])));
	}
	for (size_t n = 0; n < ARRAY_SIZE(local); n++) {
		zassert_ok(ndef_type_validate(NDEF_TNF_WELL_KNOWN, (const uint8_t *)local[n],
					      strlen(local[n])));
		zassert_true(ndef_wkt_is_local((const uint8_t *)local[n], strlen(local[n])));
	}
	/* Whether application context exists is deliberately not inferred here. */
}

ZTEST(rtd_spec, test_binary_encoding_examples)
{
	/* RTD sections 2.1, 2.2, 3.1, appendix B: no canonical URN prefix. */
	static const struct ndef_record_desc sms = {
		.tnf = NDEF_TNF_WELL_KNOWN,
		.type = "Sms",
		.type_len = 3U,
	};
	static const struct ndef_record_desc external = {
		.tnf = NDEF_TNF_EXTERNAL,
		.type = "example.com:f",
		.type_len = 13U,
	};
	static const uint8_t expected_sms[] = {0xD1, 0x03, 0x00, 0x53, 0x6D, 0x73};
	static const uint8_t expected_external[] = {
		0xD4, 0x0D, 0x00, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm', ':', 'f',
	};
	uint8_t buf[32];
	struct ndef_writer w;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &sms, NULL, 0U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected_sms));
	zassert_mem_equal(buf, expected_sms, sizeof(expected_sms));
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &external, NULL, 0U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected_external));
	zassert_mem_equal(buf, expected_external, sizeof(expected_external));
}

ZTEST(rtd_spec, test_well_known_equivalence)
{
	/* Section 3.3 and appendix B: equal iff binary representations match. */
	static const uint8_t sms[] = {0xD1, 0x03, 0x00, 0x53, 0x6D, 0x73};
	static const uint8_t lower_sms[] = {0xD1, 0x03, 0x00, 0x73, 0x6D, 0x73};
	static const char *const distinct[] = {"Foobar", "fooBar", "fOoBaR", "foobar"};
	struct ndef_record rec;

	zassert_ok(ndef_record_decode(sms, sizeof(sms), &rec));
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_WELL_KNOWN, (const uint8_t *)"Sms", 3U));
	zassert_false(ndef_record_type_eq(&rec, NDEF_TNF_WELL_KNOWN, (const uint8_t *)"sms", 3U));
	zassert_ok(ndef_record_decode(lower_sms, sizeof(lower_sms), &rec));
	zassert_false(ndef_record_type_eq(&rec, NDEF_TNF_WELL_KNOWN, (const uint8_t *)"Sms", 3U));

	for (size_t n = 0; n < ARRAY_SIZE(distinct); n++) {
		rec.tnf = NDEF_TNF_WELL_KNOWN;
		rec.type = (const uint8_t *)distinct[n];
		rec.type_len = 6U;
		for (size_t m = 0; m < ARRAY_SIZE(distinct); m++) {
			zassert_equal(ndef_record_type_eq(&rec, NDEF_TNF_WELL_KNOWN,
							  (const uint8_t *)distinct[m], 6U),
				      n == m);
		}
	}
}

ZTEST(rtd_spec, test_external_equivalence)
{
	/* These four case variants are the explicit equivalent examples of 3.3. */
	static const char *const equivalent[] = {
		"example.com:foobar",
		"Example.com:foobar",
		"Example.COM:Foobar",
		"eXaMpLe.CoM:fOoBaR",
	};
	struct ndef_record rec = {.tnf = NDEF_TNF_EXTERNAL, .type_len = 18U};

	for (size_t n = 0; n < ARRAY_SIZE(equivalent); n++) {
		rec.type = (const uint8_t *)equivalent[n];
		for (size_t m = 0; m < ARRAY_SIZE(equivalent); m++) {
			zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL,
							 (const uint8_t *)equivalent[m], 18U));
		}
		zassert_false(ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL,
						  (const uint8_t *)"example.com:foobaz", 18U));
	}
	/* Character-by-character comparison does not percent-decode names. */
	rec.type = (const uint8_t *)"d:%41";
	rec.type_len = 5U;
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL, (const uint8_t *)"D:%41", 5U));
	zassert_false(ndef_record_type_eq(&rec, NDEF_TNF_EXTERNAL, (const uint8_t *)"d:A", 3U));
}

ZTEST(rtd_spec, test_unknown_types_do_not_block_known_record_lookup)
{
	/* Section 4.2: unknown types must not prevent use of later known records. */
	static const uint8_t msg[] = {
		0x91, 0x01, 0x00, 'Z', 0x14, 0x03, 0x00, 'd', ':', 'x', 0x51, 0x01, 0x01, 'U', 0x00,
	};
	struct ndef_record rec;

	zassert_ok(ndef_msg_validate(msg, sizeof(msg), NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT,
				     NULL, NULL));
	zassert_ok(ndef_msg_find_type(msg, sizeof(msg), NDEF_TNF_WELL_KNOWN, (const uint8_t *)"U",
				      1U, &rec));
	zassert_equal(rec.raw, msg + 10U);
	/* Applications still decide which types they recognize and dispatch. */
}

ZTEST(rtd_spec, test_illegal_type_diagnostic_and_application_filter)
{
	/* Section 4.1 is an application ignore rule, not a required errno. */
	static const uint8_t msg[] = {
		0x91, 0x01, 0x00, '/', 0x51, 0x01, 0x01, 'U', 0x00,
	};
	static const struct ndef_record_desc invalid = {
		.tnf = NDEF_TNF_WELL_KNOWN,
		.type = "/",
		.type_len = 1U,
	};
	struct ndef_msg_iter it;
	struct ndef_record rec;
	struct ndef_writer w;
	uint8_t buf[16];
	unsigned int ignored = 0U;
	unsigned int recognized = 0U;
	int ret;

	zassert_ok(ndef_msg_iter_init(&it, msg, sizeof(msg)));
	while ((ret = ndef_msg_iter_next(&it, &rec)) == 0) {
		if (ndef_type_validate(rec.tnf, rec.type, rec.type_len) < 0) {
			ignored++;
			continue;
		}
		if (ndef_record_is_wkt(&rec, "U")) {
			recognized++;
		}
	}
	zassert_equal(ret, -ENOENT);
	zassert_equal(ignored, 1U);
	zassert_equal(recognized, 1U);
	zassert_equal(ndef_msg_validate(msg, sizeof(msg), NDEF_VALIDATE_TYPES, NULL, NULL),
		      -EBADMSG);
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_equal(ndef_writer_record_add(&w, &invalid, NULL, 0U), -EINVAL);
}

ZTEST_SUITE(rtd_spec, NULL, NULL, NULL, NULL, NULL);
