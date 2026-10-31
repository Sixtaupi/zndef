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

static uint8_t buf[512];
static char str[128];

static int first_record(const uint8_t *msg, size_t len, struct ndef_record *rec)
{
	struct ndef_msg_iter it;

	(void)ndef_msg_iter_init(&it, msg, len);

	return ndef_msg_iter_next(&it, rec);
}

/* Text */

ZTEST(ndef_rtd, test_text_utf8)
{
	struct ndef_writer w;
	struct ndef_record rec;
	struct ndef_text text;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_text_write(&w, "fr-FR", "\xc3\xa9t\xc3\xa9"));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(buf[4], 5U);

	zassert_ok(first_record(buf, len, &rec));
	zassert_ok(ndef_text_parse(&rec, &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF8);
	zassert_equal(text.lang_len, 5U);
	zassert_mem_equal(text.lang, "fr-FR", 5U);
	zassert_ok(ndef_text_to_utf8(&text, str, sizeof(str), &len));
	zassert_equal(len, 5U);
	zassert_str_equal(str, "\xc3\xa9t\xc3\xa9");
	zassert_equal(ndef_text_to_utf8(&text, str, 5U, NULL), -ENOSPC);
}

ZTEST(ndef_rtd, test_text_utf16)
{
	/* "A€😀" in UTF-16 BE without BOM, LE with BOM, BE with BOM. */
	static const uint8_t be[] = {0x82, 'e', 'n', 0x00, 'A', 0x20, 0xAC, 0xD8, 0x3D, 0xDE, 0x00};
	static const uint8_t le[] = {0x82, 'e',  'n',  0xFF, 0xFE, 'A', 0x00,
				     0xAC, 0x20, 0x3D, 0xD8, 0x00, 0xDE};
	static const uint8_t bom[] = {0x82, 'e',  'n',  0xFE, 0xFF, 0x00, 'A',
				      0x20, 0xAC, 0xD8, 0x3D, 0xDE, 0x00};
	static const uint8_t lone[] = {0x82, 'e', 'n', 0xDC, 0x00};
	static const uint8_t cut[] = {0x82, 'e', 'n', 0xD8, 0x3D};
	static const char expected[] = "A\xe2\x82\xac\xf0\x9f\x98\x80";
	struct ndef_text text;

	zassert_ok(ndef_text_payload_decode(be, sizeof(be), &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF16);
	zassert_ok(ndef_text_to_utf8(&text, str, sizeof(str), NULL));
	zassert_str_equal(str, expected);

	zassert_ok(ndef_text_payload_decode(le, sizeof(le), &text));
	zassert_ok(ndef_text_to_utf8(&text, str, sizeof(str), NULL));
	zassert_str_equal(str, expected);

	zassert_ok(ndef_text_payload_decode(bom, sizeof(bom), &text));
	zassert_ok(ndef_text_to_utf8(&text, str, sizeof(str), NULL));
	zassert_str_equal(str, expected);

	zassert_equal(ndef_text_payload_decode(lone, sizeof(lone), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(cut, sizeof(cut), &text), -EBADMSG);
}

ZTEST(ndef_rtd, test_text_malformed)
{
	static const uint8_t rfu[] = {0x42, 'e', 'n', 'x'};
	static const uint8_t no_lang[] = {0x00, 'x'};
	static const uint8_t short_lang[] = {0x05, 'e', 'n'};
	static const uint8_t odd16[] = {0x82, 'e', 'n', 0x00};
	static const uint8_t bad8[] = {0x02, 'e', 'n', 0xC3};
	static const uint8_t ctl_lang[] = {0x02, 'e', 0x01};
	struct ndef_text text;

	/* The RFU bit is ignored. */
	zassert_ok(ndef_text_payload_decode(rfu, sizeof(rfu), &text));
	zassert_equal(text.encoding, NDEF_TEXT_UTF8);
	zassert_equal(ndef_text_payload_decode(no_lang, sizeof(no_lang), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(short_lang, sizeof(short_lang), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(odd16, sizeof(odd16), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(bad8, sizeof(bad8), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(ctl_lang, sizeof(ctl_lang), &text), -EBADMSG);
	zassert_equal(ndef_text_payload_decode(rfu, 0U, &text), -EBADMSG);
}

ZTEST(ndef_rtd, test_text_encode)
{
	/* 31 "a-" then "ab": 64 characters, a valid tag when cut to 63. */
	static const char lang64[] =
		"a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-a-ab";
	uint8_t out[8];
	size_t len;

	zassert_ok(ndef_text_payload_encode(out, sizeof(out), NDEF_TEXT_UTF16, "en", 2U, "\x00X",
					    2U, &len));
	zassert_equal(len, 5U);
	zassert_equal(out[0], 0x82);
	zassert_equal(ndef_text_payload_encode(out, sizeof(out), NDEF_TEXT_UTF16, "en", 2U, "X", 1U,
					       &len),
		      -EINVAL);
	zassert_equal(
		ndef_text_payload_encode(out, sizeof(out), NDEF_TEXT_UTF8, "", 0U, "X", 1U, &len),
		-EINVAL);
	zassert_equal(ndef_text_payload_encode(out, sizeof(out), NDEF_TEXT_UTF8, "en", 2U, "abcdef",
					       6U, &len),
		      -ENOSPC);
	zassert_equal(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, lang64, 64U, "",
					       0U, &len),
		      -EINVAL);
	zassert_ok(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, lang64, 63U, "", 0U,
					    &len));
	zassert_equal(buf[0], 63U);

	/* RFC 3066 language tags. */
	zassert_ok(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "zh-Hant-TW", 10U, "",
					    0U, &len));
	zassert_ok(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "de-1996", 7U, "", 0U,
					    &len));
	zassert_equal(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "en_US", 5U, "",
					       0U, &len),
		      -EINVAL);
	zassert_equal(
		ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "en-", 3U, "", 0U, &len),
		-EINVAL);
	zassert_equal(
		ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "1en", 3U, "", 0U, &len),
		-EINVAL);
	zassert_equal(ndef_text_payload_encode(buf, sizeof(buf), NDEF_TEXT_UTF8, "abcdefghi", 9U,
					       "", 0U, &len),
		      -EINVAL);
}

ZTEST(ndef_rtd, test_text_wrong_record)
{
	static const uint8_t msg[] = {0xD1, 0x01, 0x01, 'U', 0x00};
	struct ndef_record rec;
	struct ndef_text text;

	zassert_ok(first_record(msg, sizeof(msg), &rec));
	zassert_equal(ndef_text_parse(&rec, &text), -ENOMSG);
	zassert_equal(ndef_text_parse(NULL, &text), -EINVAL);
}

/* URI */

ZTEST(ndef_rtd, test_uri_prefixes)
{
	static const struct {
		const char *uri;
		uint8_t code;
	} cases[] = {
		{"https://www.example.com", 0x02},
		{"http://example.com", 0x03},
		{"tel:+33123456789", 0x05},
		{"mailto:a@b.c", 0x06},
		{"ftp://anonymous:anonymous@x", 0x07},
		{"ftp://ftp.x", 0x08},
		{"ftp://x", 0x0D},
		{"urn:epc:id:sgtin:1", 0x1E},
		{"urn:epc:x", 0x22},
		{"urn:nfc:wkt:T", 0x23},
		{"urn:isbn:1", 0x13},
		{"geo:1,2", 0x00},
		{"", 0x00},
	};
	struct ndef_writer w;
	struct ndef_record rec;
	struct ndef_uri uri;
	size_t len;

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
		zassert_ok(ndef_uri_write(&w, cases[i].uri));
		zassert_ok(ndef_writer_finish(&w, &len));
		zassert_ok(first_record(buf, len, &rec));
		zassert_ok(ndef_uri_parse(&rec, &uri));
		zassert_equal(uri.code, cases[i].code, "%s", cases[i].uri);
		zassert_ok(ndef_uri_to_string(&uri, str, sizeof(str), &len));
		zassert_equal(len, strlen(cases[i].uri));
		zassert_str_equal(str, cases[i].uri);
	}
}

ZTEST(ndef_rtd, test_uri_decode)
{
	static const uint8_t rfu[] = {0x50, 'a', 'b'};
	static const uint8_t bad[] = {0x01, 0xFF};
	struct ndef_uri uri;

	zassert_ok(ndef_uri_payload_decode(rfu, sizeof(rfu), &uri));
	zassert_str_equal(uri.prefix, "");
	zassert_ok(ndef_uri_to_string(&uri, str, sizeof(str), NULL));
	zassert_str_equal(str, "ab");
	zassert_equal(ndef_uri_to_string(&uri, str, 2U, NULL), -ENOSPC);
	zassert_equal(ndef_uri_payload_decode(bad, sizeof(bad), &uri), -EBADMSG);
	zassert_equal(ndef_uri_payload_decode((const uint8_t *)"\x03"
							       "a\nb",
					      4U, &uri),
		      -EBADMSG);
	zassert_equal(ndef_uri_payload_encode(buf, sizeof(buf), "a\tb", 3U, &uri.field_len),
		      -EINVAL);
	zassert_equal(ndef_uri_payload_decode(bad, 0U, &uri), -EBADMSG);
	zassert_str_equal(ndef_uri_prefix(0x23), "urn:nfc:");
	zassert_str_equal(ndef_uri_prefix(0x24), "");
}

/* Smart Poster */

static int build_sp(struct ndef_writer *w)
{
	static const uint8_t icon[] = {0x89, 'P', 'N', 'G'};
	struct ndef_writer sub;
	int ret;

	ret = ndef_sp_begin(w, &sub);
	ret = ret ? ret : ndef_text_write(&sub, "en-US", "Hello");
	ret = ret ? ret : ndef_uri_write(&sub, "https://zephyrproject.org");
	ret = ret ? ret : ndef_text_write(&sub, "fr", "Bonjour");
	ret = ret ? ret : ndef_sp_action_write(&sub, NDEF_SP_ACTION_SAVE);
	ret = ret ? ret : ndef_sp_size_write(&sub, 123456U);
	ret = ret ? ret : ndef_sp_type_write(&sub, "text/html");
	ret = ret ? ret : ndef_sp_icon_write(&sub, "image/png", icon, sizeof(icon));

	return ret ? ret : ndef_sp_end(w, &sub);
}

ZTEST(ndef_rtd, test_sp_round_trip)
{
	struct ndef_writer w;
	struct ndef_record rec;
	struct ndef_text title;
	struct ndef_sp sp;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(build_sp(&w));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_ok(first_record(buf, len, &rec));
	zassert_ok(ndef_sp_parse(&rec, &sp));
	zassert_equal(sp.uri.code, 0x04);
	zassert_equal(sp.uri.field_len, strlen("zephyrproject.org"));
	zassert_true(sp.has_action);
	zassert_equal(sp.action, NDEF_SP_ACTION_SAVE);
	zassert_true(sp.has_size);
	zassert_equal(sp.size, 123456U);
	zassert_equal(sp.type_len, 9U);
	zassert_mem_equal(sp.type, "text/html", 9U);
	zassert_equal(sp.title_count, 2U);
	zassert_equal(sp.icon_count, 1U);

	zassert_ok(ndef_sp_title_get(&sp, 1U, &title));
	zassert_mem_equal(title.lang, "fr", 2U);
	zassert_equal(ndef_sp_title_get(&sp, 2U, &title), -ENOENT);

	zassert_ok(ndef_sp_title_find(&sp, "FR", &title));
	zassert_mem_equal(title.text, "Bonjour", 7U);
	zassert_ok(ndef_sp_title_find(&sp, "en", &title));
	zassert_mem_equal(title.text, "Hello", 5U);
	zassert_equal(ndef_sp_title_find(&sp, "de", &title), -ENOENT);

	zassert_ok(ndef_sp_icon_get(&sp, 0U, &rec));
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)"image/png", 9U));
	zassert_equal(rec.payload_len, 4U);
	zassert_equal(ndef_sp_icon_get(&sp, 1U, &rec), -ENOENT);
}

ZTEST(ndef_rtd, test_sp_rules)
{
	struct ndef_writer w;
	struct ndef_writer sub;
	struct ndef_sp sp;
	size_t len;

	/* No URI: the record is dropped. */
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_text_write(&w, "en", "first"));
	len = ndef_writer_len(&w);
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_text_write(&sub, "en", "x"));
	zassert_equal(ndef_sp_end(&w, &sub), -EBADMSG);
	zassert_equal(ndef_writer_len(&w), len);
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	/* Two URIs. */
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_uri_write(&sub, "a:b"));
	zassert_ok(ndef_uri_write(&sub, "a:c"));
	zassert_equal(ndef_sp_end(&w, &sub), -EBADMSG);

	/* Two titles in the same language. */
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_uri_write(&sub, "a:b"));
	zassert_ok(ndef_text_write(&sub, "en", "x"));
	zassert_ok(ndef_text_write(&sub, "EN", "y"));
	zassert_equal(ndef_sp_end(&w, &sub), -EBADMSG);

	/* Empty smart poster. */
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_equal(ndef_sp_end(&w, &sub), -EBADMSG);

	zassert_equal(ndef_sp_action_write(&sub, (enum ndef_sp_action)3), -EINVAL);
	zassert_equal(ndef_sp_type_write(&sub, "html"), -EINVAL);
	zassert_equal(ndef_sp_icon_write(&sub, "text/plain", "x", 1U), -EINVAL);

	/* Unknown records are ignored; malformed action is not. */
	{
		static const uint8_t ok[] = {0x91, 0x01, 0x02, 'U', 0x00, 'x', 0x55, 0x00, 0x00};
		static const uint8_t bad_act[] = {0x91, 0x01, 0x02, 'U', 0x00, 'x',  0x51,
						  0x03, 0x02, 'a',  'c', 't',  0x00, 0x00};

		zassert_ok(ndef_sp_payload_decode(ok, sizeof(ok), &sp));
		zassert_false(sp.has_action);
		zassert_equal(ndef_sp_payload_decode(bad_act, sizeof(bad_act), &sp), -EBADMSG);
	}
}

/* Signature */

ZTEST(ndef_rtd, test_sig_round_trip)
{
	static const struct ndef_sig_blob certs[] = {
		{.data = (const uint8_t *)"cert-one", .len = 8U},
		{.data = (const uint8_t *)"c2", .len = 2U},
	};
	struct ndef_sig sig = {
		.version = NDEF_SIG_VERSION_2_0,
		.type = NDEF_SIG_TYPE_ECDSA_P256,
		.hash = NDEF_SIG_HASH_SHA256,
		.signature = {.data = (const uint8_t *)"\x01\x02\x03", .len = 3U},
		.cert_format = NDEF_SIG_CERT_M2M,
		.cert_count = 2U,
		.certs = certs,
		.has_cert_uri = true,
		.cert_uri = {.data = (const uint8_t *)"http://x/c", .len = 10U},
	};
	struct ndef_sig out;
	struct ndef_sig_blob cert;
	size_t len;

	zassert_ok(ndef_sig_payload_encode(&sig, buf, sizeof(buf), &len));
	zassert_equal(len, 1U + 2U + 2U + 3U + 1U + 10U + 4U + 2U + 10U);
	zassert_equal(buf[0], 0x20);
	zassert_equal(buf[1], 0x0B);
	zassert_equal(buf[2], 0x02);
	zassert_equal(buf[8], 0x80 | 0x10 | 0x02);

	zassert_ok(ndef_sig_payload_decode(buf, len, &out));
	zassert_equal(out.type, NDEF_SIG_TYPE_ECDSA_P256);
	zassert_equal(out.hash, NDEF_SIG_HASH_SHA256);
	zassert_false(out.signature_is_uri);
	zassert_equal(out.signature.len, 3U);
	zassert_equal(out.cert_format, NDEF_SIG_CERT_M2M);
	zassert_equal(out.cert_count, 2U);
	zassert_ok(ndef_sig_cert_get(&out, 1U, &cert));
	zassert_equal(cert.len, 2U);
	zassert_mem_equal(cert.data, "c2", 2U);
	zassert_equal(ndef_sig_cert_get(&out, 2U, &cert), -ENOENT);
	zassert_true(out.has_cert_uri);
	zassert_mem_equal(out.cert_uri.data, "http://x/c", 10U);

	/* Truncations and trailing data. */
	for (size_t n = 0; n < len; n++) {
		zassert_equal(ndef_sig_payload_decode(buf, n, &out), -EBADMSG, "length %zu", n);
	}
	zassert_equal(ndef_sig_payload_decode(buf, len + 1U, &out), -EBADMSG);

	/* Unknown major version. */
	buf[0] = 0x30;
	zassert_equal(ndef_sig_payload_decode(buf, len, &out), -ENOTSUP);
}

ZTEST(ndef_rtd, test_sig_v1_and_uri)
{
	/* Version 1.0, signature URI, no certificate. */
	static const uint8_t v1[] = {0x01, 0x82, 0x00, 0x03, 'a', ':', 'b', 0x00};
	static const uint8_t ctl[] = {0x01, 0x82, 0x00, 0x03, 'a', ':', 0x0A, 0x00};
	static const uint8_t marker[] = {0x01, 0x00, 0x00, 0x00};
	struct ndef_sig sig;
	size_t len;

	zassert_ok(ndef_sig_payload_decode(v1, sizeof(v1), &sig));
	zassert_equal(sig.version, NDEF_SIG_VERSION_1_0);
	zassert_true(sig.signature_is_uri);
	zassert_equal(sig.type, NDEF_SIG_TYPE_RSASSA_PKCS1_1024);
	zassert_equal(sig.hash, NDEF_SIG_HASH_NONE);
	zassert_equal(sig.cert_count, 0U);
	zassert_equal(ndef_sig_payload_decode(ctl, sizeof(ctl), &sig), -EBADMSG);

	zassert_ok(ndef_sig_payload_decode(marker, sizeof(marker), &sig));
	zassert_equal(sig.type, NDEF_SIG_TYPE_NONE);

	memset(&sig, 0, sizeof(sig));
	sig.version = NDEF_SIG_VERSION_1_0;
	zassert_ok(ndef_sig_payload_encode(&sig, buf, sizeof(buf), &len));
	zassert_equal(len, sizeof(marker));
	zassert_mem_equal(buf, marker, sizeof(marker));

	sig.version = 0x02;
	zassert_equal(ndef_sig_payload_encode(&sig, buf, sizeof(buf), &len), -EINVAL);
}

struct sig_ctx {
	uint8_t data[64];
	size_t len;
	uint32_t records;
};

static int collect_data(const uint8_t *data, size_t len, void *user_data)
{
	struct sig_ctx *ctx = user_data;

	if (len > sizeof(ctx->data) - ctx->len) {
		return -ENOSPC;
	}
	memcpy(&ctx->data[ctx->len], data, len);
	ctx->len += len;

	return 0;
}

static int collect_record(const struct ndef_record *rec, void *user_data)
{
	struct sig_ctx *ctx = user_data;

	ctx->records++;

	return ndef_sig_record_data(rec, collect_data, ctx);
}

ZTEST(ndef_rtd, test_sig_covered_records)
{
	static const struct ndef_record_desc media = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "a/b");
	struct ndef_sig sig = {
		.version = NDEF_SIG_VERSION_2_0,
		.type = NDEF_SIG_TYPE_ECDSA_P256,
		.hash = NDEF_SIG_HASH_SHA256,
		.signature = {.data = (const uint8_t *)"S", .len = 1U},
	};
	struct sig_ctx ctx;
	struct ndef_writer w;
	size_t len;

	/* [0] U, [1] chunked a/b, [2] Sig, [3] T, [4] Sig */
	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_uri_write(&w, "a:b"));
	zassert_ok(ndef_writer_chunk_begin(&w, &media, "12", 2U));
	zassert_ok(ndef_writer_chunk(&w, "34", 2U, true));
	zassert_ok(ndef_sig_write(&w, &sig));
	zassert_ok(ndef_text_write(&w, "en", "x"));
	zassert_ok(ndef_sig_write(&w, &sig));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	memset(&ctx, 0, sizeof(ctx));
	zassert_ok(ndef_sig_foreach_signed(buf, len, 2U, collect_record, &ctx));
	zassert_equal(ctx.records, 2U);
	zassert_equal(ctx.len, 5U + 7U);
	zassert_mem_equal(ctx.data,
			  "U\x00"
			  "a:ba/b1234",
			  12U);

	memset(&ctx, 0, sizeof(ctx));
	zassert_ok(ndef_sig_foreach_signed(buf, len, 4U, collect_record, &ctx));
	zassert_equal(ctx.records, 1U);
	zassert_mem_equal(ctx.data,
			  "T\x02"
			  "enx",
			  5U);

	zassert_equal(ndef_sig_foreach_signed(buf, len, 3U, collect_record, &ctx), -EINVAL);
	zassert_equal(ndef_sig_foreach_signed(buf, len, 5U, collect_record, &ctx), -EINVAL);
}

/* Connection Handover */

ZTEST(ndef_rtd, test_handover_select)
{
	static const char oob_type[] = "application/vnd.bluetooth.le.oob";
	static const struct ndef_ho_ref carrier = {.id = "0", .len = 1U};
	static const struct ndef_ho_ref aux[] = {{.id = "aux1", .len = 4U}, {.id = "b", .len = 1U}};
	struct ndef_record_desc oob = {
		.tnf = NDEF_TNF_MEDIA,
		.type = oob_type,
		.type_len = sizeof(oob_type) - 1U,
		.id = "0",
		.id_len = 1U,
	};
	struct ndef_writer w;
	struct ndef_writer sub;
	struct ndef_record rec;
	struct ndef_ho_ac ac;
	struct ndef_ho_ref ref;
	struct ndef_ho ho;
	enum ndef_ho_kind kind;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_SELECT, NDEF_HO_VERSION_DEFAULT, &sub));
	zassert_ok(ndef_ho_ac_write(&sub, NDEF_HO_CPS_ACTIVE, &carrier, aux, 2U));
	zassert_ok(ndef_ho_err_write(&sub, NDEF_HO_ERR_PERM_MEMORY, 1024U));
	zassert_ok(ndef_ho_end(&w, &sub));
	zassert_ok(ndef_writer_record_add(&w, &oob, "\x08\x1b", 2U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_ok(first_record(buf, len, &rec));
	zassert_ok(ndef_ho_parse(&rec, &kind, &ho));
	zassert_equal(kind, NDEF_HO_SELECT);
	zassert_equal(ho.version, 0x15);
	zassert_equal(ho.ac_count, 1U);
	zassert_true(ho.has_err);
	zassert_equal(ho.err_reason, NDEF_HO_ERR_PERM_MEMORY);
	zassert_equal(ho.err_data, 1024U);

	zassert_ok(ndef_ho_ac_get(&ho, 0U, &ac));
	zassert_equal(ac.cps, NDEF_HO_CPS_ACTIVE);
	zassert_equal(ac.aux_count, 2U);
	zassert_ok(ndef_ho_ac_aux_get(&ac, 0U, &ref));
	zassert_equal(ref.len, 4U);
	zassert_mem_equal(ref.id, "aux1", 4U);
	zassert_ok(ndef_ho_ac_aux_get(&ac, 1U, &ref));
	zassert_mem_equal(ref.id, "b", 1U);
	zassert_equal(ndef_ho_ac_aux_get(&ac, 2U, &ref), -ENOENT);
	zassert_equal(ndef_ho_ac_get(&ho, 1U, &ac), -ENOENT);

	zassert_ok(ndef_msg_find_id(buf, len, ac.carrier_ref, ac.carrier_ref_len, &rec));
	zassert_true(ndef_record_type_eq(&rec, NDEF_TNF_MEDIA, (const uint8_t *)oob_type,
					 sizeof(oob_type) - 1U));
}

ZTEST(ndef_rtd, test_handover_request_rules)
{
	static const struct ndef_ho_ref carrier = {.id = "0", .len = 1U};
	struct ndef_writer w;
	struct ndef_writer sub;
	struct ndef_ho ho;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));

	/* 1.5 request without collision resolution. */
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_REQUEST, NDEF_HO_VERSION(1, 5), &sub));
	zassert_ok(ndef_ho_ac_write(&sub, NDEF_HO_CPS_ACTIVE, &carrier, NULL, 0U));
	zassert_equal(ndef_ho_end(&w, &sub), -EBADMSG);
	zassert_equal(ndef_writer_len(&w), 0U);

	/* 1.1 request without collision resolution. */
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_REQUEST, NDEF_HO_VERSION(1, 1), &sub));
	zassert_ok(ndef_ho_ac_write(&sub, NDEF_HO_CPS_ACTIVE, &carrier, NULL, 0U));
	zassert_ok(ndef_ho_end(&w, &sub));

	/* 1.5 request without carrier. */
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_REQUEST, NDEF_HO_VERSION(1, 5), &sub));
	zassert_ok(ndef_ho_cr_write(&sub, 0x1234U));
	zassert_equal(ndef_ho_end(&w, &sub), -EBADMSG);

	/* Valid 1.5 request. */
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_REQUEST, NDEF_HO_VERSION(1, 5), &sub));
	zassert_ok(ndef_ho_cr_write(&sub, 0x1234U));
	zassert_ok(ndef_ho_ac_write(&sub, NDEF_HO_CPS_UNKNOWN, &carrier, NULL, 0U));
	zassert_ok(ndef_ho_end(&w, &sub));

	/* Empty select. */
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_SELECT, NDEF_HO_VERSION(1, 2), &sub));
	zassert_ok(ndef_ho_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_equal(ndef_ho_payload_decode(NDEF_HO_REQUEST, (const uint8_t *)"\x12", 1U, &ho),
		      -EBADMSG);
	zassert_ok(ndef_ho_payload_decode(NDEF_HO_SELECT, (const uint8_t *)"\x12", 1U, &ho));
	zassert_equal(ho.msg_len, 0U);
	zassert_equal(ndef_ho_payload_decode(NDEF_HO_SELECT, (const uint8_t *)"\x22", 1U, &ho),
		      -ENOTSUP);
	zassert_equal(ndef_ho_begin(&w, NDEF_HO_SELECT, 0x20, &sub), -EINVAL);
	zassert_equal(ndef_ho_err_write(&sub, NDEF_HO_ERR_CARRIER, 256U), -EINVAL);
}

ZTEST(ndef_rtd, test_handover_records)
{
	static const uint8_t ac_bad[] = {0x01, 0x00, 0x00};
	static const uint8_t ac_trunc[] = {0x01, 0x01, '0', 0x01, 0x02, 'a'};
	struct ndef_ho_carrier hc;
	struct ndef_ho_ac ac;
	struct ndef_writer w;
	struct ndef_record rec;
	size_t len;

	zassert_equal(ndef_ho_ac_payload_decode(ac_bad, sizeof(ac_bad), &ac), -EBADMSG);
	zassert_equal(ndef_ho_ac_payload_decode(ac_trunc, sizeof(ac_trunc), &ac), -EBADMSG);

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_ho_carrier_write(&w, "c", 1U, NDEF_HO_CTF_EXTERNAL, "nfc.org:x", 9U,
					 "\x01\x02", 2U));
	zassert_equal(ndef_ho_carrier_write(&w, "c", 1U, NDEF_HO_CTF_MEDIA, "bad", 3U, NULL, 0U),
		      -EINVAL);
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_ok(first_record(buf, len, &rec));
	zassert_true(ndef_record_is_wkt(&rec, NDEF_RTD_HO_CARRIER));
	zassert_ok(ndef_ho_carrier_payload_decode(rec.payload, rec.payload_len, &hc));
	zassert_equal(hc.ctf, NDEF_HO_CTF_EXTERNAL);
	zassert_equal(hc.type_len, 9U);
	zassert_equal(hc.data_len, 2U);
	zassert_equal(hc.data[1], 0x02);
	zassert_str_equal(ndef_ho_type(NDEF_HO_MEDIATION), "Hm");
	zassert_is_null(ndef_ho_type((enum ndef_ho_kind)9));
}

/* Device Information */

ZTEST(ndef_rtd, test_di_round_trip)
{
	static const uint8_t uuid[NDEF_DI_UUID_LEN] = {1, 2,  3,  4,  5,  6,  7,  8,
						       9, 10, 11, 12, 13, 14, 15, 16};
	const struct ndef_di_elem elems[] = {
		{.type = NDEF_DI_MANUFACTURER, .len = 4U, .value = (const uint8_t *)"Acme"},
		{.type = NDEF_DI_MODEL, .len = 2U, .value = (const uint8_t *)"M1"},
		{.type = NDEF_DI_UUID, .len = NDEF_DI_UUID_LEN, .value = uuid},
		{.type = 0x42, .len = 1U, .value = (const uint8_t *)"\xff"},
		{.type = NDEF_DI_FIRMWARE, .len = 3U, .value = (const uint8_t *)"1.0"},
	};
	struct ndef_di_elem e;
	struct ndef_writer w;
	struct ndef_record rec;
	struct ndef_di di;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_di_write(&w, elems, 5U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));

	zassert_ok(first_record(buf, len, &rec));
	zassert_ok(ndef_di_parse(&rec, &di));
	zassert_equal(di.count, 5U);
	zassert_mem_equal(di.manufacturer.value, "Acme", 4U);
	zassert_mem_equal(di.model.value, "M1", 2U);
	zassert_mem_equal(di.uuid.value, uuid, sizeof(uuid));
	zassert_mem_equal(di.firmware.value, "1.0", 3U);
	zassert_is_null(di.unique_name.value);
	zassert_ok(ndef_di_elem_get(&di, 3U, &e));
	zassert_equal(e.type, 0x42);
	zassert_equal(ndef_di_elem_get(&di, 5U, &e), -ENOENT);

	/* Missing model, duplicated element, bad UUID length. */
	zassert_equal(ndef_di_payload_encode(elems, 1U, buf, sizeof(buf), &len), -EINVAL);
	{
		const struct ndef_di_elem dup[] = {elems[0], elems[1], elems[0]};
		const struct ndef_di_elem bad_uuid[] = {
			elems[0], elems[1], {.type = NDEF_DI_UUID, .len = 2U, .value = uuid}};
		static const uint8_t no_model[] = {0x00, 0x01, 'A'};
		static const uint8_t dup_raw[] = {0x00, 0x01, 'A', 0x01, 0x00, 0x00, 0x00};
		static const uint8_t trunc[] = {0x00, 0x01, 'A', 0x01, 0x02, 'M'};

		zassert_equal(ndef_di_payload_encode(dup, 3U, buf, sizeof(buf), &len), -EINVAL);
		zassert_equal(ndef_di_payload_encode(bad_uuid, 3U, buf, sizeof(buf), &len),
			      -EINVAL);
		zassert_equal(ndef_di_payload_decode(no_model, sizeof(no_model), &di), -EBADMSG);
		zassert_equal(ndef_di_payload_decode(dup_raw, sizeof(dup_raw), &di), -EBADMSG);
		zassert_equal(ndef_di_payload_decode(trunc, sizeof(trunc), &di), -EBADMSG);
	}
}

/* Validation of RTD payloads */

ZTEST(ndef_rtd, test_validate_rtd)
{
	/* Text record whose language code overruns the payload. */
	static const uint8_t bad_text[] = {0xD1, 0x01, 0x03, 'T', 0x05, 'e', 'n'};
	/* Unknown well-known type: accepted. */
	static const uint8_t unknown[] = {0xD1, 0x02, 0x01, 'Z', 'z', 0x00};

	zassert_ok(ndef_msg_validate(bad_text, sizeof(bad_text), NDEF_VALIDATE_TYPES, NULL, NULL));

	if (!IS_ENABLED(CONFIG_ZNDEF_VALIDATE_RTD)) {
		zassert_equal(ndef_msg_validate(bad_text, sizeof(bad_text), NDEF_VALIDATE_RTD, NULL,
						NULL),
			      -ENOTSUP);
		ztest_test_skip();
	}

	zassert_equal(ndef_msg_validate(bad_text, sizeof(bad_text), NDEF_VALIDATE_RTD, NULL, NULL),
		      -EBADMSG);
	zassert_ok(ndef_msg_validate(unknown, sizeof(unknown), NDEF_VALIDATE_ALL, NULL, NULL));

	/* A lone Smart Poster action record is an error. */
	{
		static const uint8_t act[] = {0xD1, 0x03, 0x01, 'a', 'c', 't', 0x00};

		zassert_ok(ndef_msg_validate(act, sizeof(act), NDEF_VALIDATE_TYPES, NULL, NULL));
		zassert_equal(ndef_msg_validate(act, sizeof(act), NDEF_VALIDATE_RTD, NULL, NULL),
			      -EBADMSG);
	}
}

ZTEST(ndef_rtd, test_validate_nested_chunks_report_unsupported)
{
	static const struct ndef_record_desc uri =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_URI);
	static const struct ndef_record_desc ac =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_HO_AC);
	static const uint8_t carrier[] = {0x01, 0x01, 'c', 0x00};
	struct ndef_writer w;
	struct ndef_writer sub;
	uint8_t scratch[64];
	uint8_t buf[128];
	size_t len;

	if (!IS_ENABLED(CONFIG_ZNDEF_VALIDATE_RTD)) {
		ztest_test_skip();
	}

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_writer_chunk_begin(&sub, &uri,
					   "\x00"
					   "a",
					   2U));
	zassert_ok(ndef_writer_chunk(&sub, "b", 1U, true));
	/* Close structurally, leaving payload validation to the validator. */
	zassert_ok(ndef_writer_nested_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL), -ENOTSUP);
	zassert_equal(ndef_msg_validate_buf(buf, len, NDEF_VALIDATE_ALL, scratch, sizeof(scratch),
					    NULL, NULL),
		      -ENOTSUP);

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_SELECT, NDEF_HO_VERSION_DEFAULT, &sub));
	zassert_ok(ndef_writer_chunk_begin(&sub, &ac, carrier, 2U));
	zassert_ok(ndef_writer_chunk(&sub, &carrier[2], 2U, true));
	zassert_ok(ndef_writer_nested_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL), -ENOTSUP);
}

ZTEST(ndef_rtd, test_unknown_nested_chunks_remain_opaque)
{
	static const struct ndef_record_desc unknown = NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, "Zz");
	struct ndef_writer w;
	struct ndef_writer sub;
	uint8_t buf[128];
	size_t len;
	uint32_t flags = NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT;

	if (IS_ENABLED(CONFIG_ZNDEF_VALIDATE_RTD)) {
		flags |= NDEF_VALIDATE_RTD;
	}

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_sp_begin(&w, &sub));
	zassert_ok(ndef_writer_chunk_begin(&sub, &unknown, "a", 1U));
	zassert_ok(ndef_writer_chunk(&sub, "b", 1U, true));
	zassert_ok(ndef_uri_write(&sub, "https://example.com"));
	zassert_ok(ndef_sp_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, flags, NULL, NULL));

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_ho_begin(&w, NDEF_HO_SELECT, NDEF_HO_VERSION_DEFAULT, &sub));
	zassert_ok(ndef_writer_chunk_begin(&sub, &unknown, "a", 1U));
	zassert_ok(ndef_writer_chunk(&sub, "b", 1U, true));
	zassert_ok(ndef_ho_end(&w, &sub));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, flags, NULL, NULL));
}

ZTEST_SUITE(ndef_rtd, NULL, NULL, NULL, NULL, NULL);
