/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_text.h>

#include "ndef_priv.h"

#define STATUS_UTF16    0x80U
#define STATUS_LANG_LEN 0x3FU

/* Shared framing check before applying the RFC 3066 language-tag syntax. */
static bool lang_valid(const uint8_t *lang, size_t len)
{
	if ((len == 0U) || (len > NDEF_TEXT_LANG_MAX)) {
		return false;
	}

	for (size_t i = 0; i < len; i++) {
		if ((lang[i] <= 0x20U) || (lang[i] >= 0x7FU)) {
			return false;
		}
	}

	return true;
}

/* RFC 3066: 1*8ALPHA *( "-" 1*8( ALPHA / DIGIT ) ). */
static bool lang_tag_valid(const uint8_t *lang, size_t len)
{
	size_t sub_len = 0;
	bool first = true;

	if (!lang_valid(lang, len)) {
		return false;
	}

	for (size_t i = 0; i <= len; i++) {
		if ((i == len) || (lang[i] == (uint8_t)'-')) {
			if ((sub_len == 0U) || (sub_len > 8U)) {
				return false;
			}
			sub_len = 0U;
			first = false;
			continue;
		}

		if (!ndef_is_alpha(lang[i]) && (first || !ndef_is_digit(lang[i]))) {
			return false;
		}
		sub_len++;
	}

	return true;
}

/* RTD-Text 1.0 section 3.4: no BOM means big endian. */
static size_t utf16_start(const uint8_t *s, size_t len, bool *le)
{
	*le = false;
	if (len >= 2U) {
		if ((s[0] == 0xFFU) && (s[1] == 0xFEU)) {
			*le = true;
			return 2U;
		}
		if ((s[0] == 0xFEU) && (s[1] == 0xFFU)) {
			return 2U;
		}
	}

	return 0U;
}

static bool utf16_next(const uint8_t *s, size_t len, size_t *off, bool le, uint32_t *cp)
{
	size_t i = *off;

	if ((len - i) < 2U) {
		return false;
	}

	*cp = le ? (((uint32_t)s[i + 1U] << 8) | s[i]) : ndef_get_be16(&s[i]);
	i += 2U;
	if ((*cp >= 0xD800U) && (*cp <= 0xDBFFU)) {
		uint32_t lo;

		if ((len - i) < 2U) {
			return false;
		}
		lo = le ? (((uint32_t)s[i + 1U] << 8) | s[i]) : ndef_get_be16(&s[i]);
		if ((lo < 0xDC00U) || (lo > 0xDFFFU)) {
			return false;
		}
		i += 2U;
		*cp = 0x10000U + ((*cp - 0xD800U) << 10) + (lo - 0xDC00U);
	} else if ((*cp >= 0xDC00U) && (*cp <= 0xDFFFU)) {
		return false;
	} else {
		/* Non-surrogate BMP scalar. */
	}

	*off = i;
	return true;
}

/* RTD-Text 1.0 section 3.2.1 requires CRLF for line breaks. */
static bool line_break_valid(uint32_t cp, bool *after_cr)
{
	if ((*after_cr && (cp != 0x0AU)) || ((cp == 0x0AU) && !*after_cr)) {
		return false;
	}

	*after_cr = cp == 0x0DU;
	return true;
}

static bool text_valid(const uint8_t *s, size_t len, enum ndef_text_encoding encoding)
{
	bool after_cr = false;

	if (encoding == NDEF_TEXT_UTF8) {
		if (!ndef_utf8_valid(s, len)) {
			return false;
		}
		for (size_t i = 0U; i < len; i++) {
			if (!line_break_valid(s[i], &after_cr)) {
				return false;
			}
		}
	} else if (encoding == NDEF_TEXT_UTF16) {
		bool le;
		size_t i;

		if ((len & 1U) != 0U) {
			return false;
		}
		i = utf16_start(s, len, &le);
		while (i < len) {
			uint32_t cp;

			if (!utf16_next(s, len, &i, le, &cp) || !line_break_valid(cp, &after_cr)) {
				return false;
			}
		}
	} else {
		return false;
	}

	return !after_cr;
}

int ndef_text_payload_decode(const uint8_t *payload, size_t len, struct ndef_text *text)
{
	uint8_t status;
	uint8_t lang_len;
	const uint8_t *str;
	size_t str_len;

	if ((payload == NULL) || (text == NULL)) {
		return -EINVAL;
	}

	if (len < 1U) {
		return -EBADMSG;
	}

	status = payload[0];
	lang_len = status & STATUS_LANG_LEN;

	/* The RFU bit is ignored (RTD-Text 1.0 section 3.2.1). */
	if (((len - 1U) < lang_len) || !lang_tag_valid(&payload[1], lang_len)) {
		return -EBADMSG;
	}

	str = &payload[1U + lang_len];
	str_len = len - 1U - lang_len;

	text->encoding = ((status & STATUS_UTF16) != 0U) ? NDEF_TEXT_UTF16 : NDEF_TEXT_UTF8;
	if (!text_valid(str, str_len, text->encoding)) {
		return -EBADMSG;
	}

	text->lang = (const char *)&payload[1];
	text->lang_len = lang_len;
	text->text = str;
	text->text_len = str_len;

	return 0;
}

int ndef_text_parse(const struct ndef_record *rec, struct ndef_text *text)
{
	int ret = ndef_rtd_check(rec, NDEF_RTD_TEXT);

	if (ret < 0) {
		return ret;
	}

	return ndef_text_payload_decode(rec->payload, rec->payload_len, text);
}

static size_t utf8_put(uint8_t *dst, uint32_t cp)
{
	if (cp < 0x80U) {
		dst[0] = (uint8_t)cp;
		return 1U;
	} else if (cp < 0x800U) {
		dst[0] = (uint8_t)(0xC0U | (cp >> 6));
		dst[1] = (uint8_t)(0x80U | (cp & 0x3FU));
		return 2U;
	} else if (cp < 0x10000U) {
		dst[0] = (uint8_t)(0xE0U | (cp >> 12));
		dst[1] = (uint8_t)(0x80U | ((cp >> 6) & 0x3FU));
		dst[2] = (uint8_t)(0x80U | (cp & 0x3FU));
		return 3U;
	} else {
		dst[0] = (uint8_t)(0xF0U | (cp >> 18));
		dst[1] = (uint8_t)(0x80U | ((cp >> 12) & 0x3FU));
		dst[2] = (uint8_t)(0x80U | ((cp >> 6) & 0x3FU));
		dst[3] = (uint8_t)(0x80U | (cp & 0x3FU));
		return 4U;
	}
}

static int utf16_to_utf8(const uint8_t *s, size_t len, uint8_t *dst, size_t size, size_t *out)
{
	bool le;
	size_t i = utf16_start(s, len, &le);
	size_t o = 0;

	while (i < len) {
		uint8_t tmp[4];
		uint32_t cp;
		size_t n;

		if (!utf16_next(s, len, &i, le, &cp)) {
			return -EBADMSG;
		}

		n = utf8_put(tmp, cp);
		if ((size - o) <= n) {
			return -ENOSPC;
		}
		(void)memcpy(&dst[o], tmp, n);
		o += n;
	}

	*out = o;

	return 0;
}

int ndef_text_to_utf8(const struct ndef_text *text, char *dst, size_t size, size_t *out_len)
{
	size_t len;

	if ((text == NULL) || (dst == NULL) || (size == 0U) ||
	    ((text->text == NULL) && (text->text_len > 0U)) ||
	    ((text->encoding != NDEF_TEXT_UTF8) && (text->encoding != NDEF_TEXT_UTF16))) {
		return -EINVAL;
	}

	if (!text_valid(text->text, text->text_len, text->encoding)) {
		return -EBADMSG;
	}

	if (text->encoding == NDEF_TEXT_UTF16) {
		int ret = utf16_to_utf8(text->text, text->text_len, (uint8_t *)dst, size, &len);

		if (ret < 0) {
			return ret;
		}
	} else {
		len = text->text_len;
		if (len >= size) {
			return -ENOSPC;
		}
		if (len > 0U) {
			(void)memcpy(dst, text->text, len);
		}
	}

	dst[len] = '\0';

	if (out_len != NULL) {
		*out_len = len;
	}

	return 0;
}

int ndef_text_payload_encode(uint8_t *buf, size_t size, enum ndef_text_encoding encoding,
			     const char *lang, size_t lang_len, const void *str, size_t str_len,
			     size_t *out_len)
{
	if ((buf == NULL) || (lang == NULL) || (out_len == NULL) ||
	    ((str == NULL) && (str_len > 0U))) {
		return -EINVAL;
	}

	if (!lang_tag_valid((const uint8_t *)lang, lang_len)) {
		return -EINVAL;
	}

	if (!text_valid(str, str_len, encoding)) {
		return -EINVAL;
	}

	if ((size < (1U + lang_len)) || ((size - 1U - lang_len) < str_len)) {
		return -ENOSPC;
	}

	buf[0] = (uint8_t)lang_len | ((encoding == NDEF_TEXT_UTF16) ? STATUS_UTF16 : 0U);
	(void)memcpy(&buf[1], lang, lang_len);
	if (str_len > 0U) {
		(void)memcpy(&buf[1U + lang_len], str, str_len);
	}

	*out_len = 1U + lang_len + str_len;

	return 0;
}

int ndef_text_write(struct ndef_writer *w, const char *lang, const char *str)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_TEXT);
	uint8_t *payload;
	size_t cap;
	size_t len;
	int ret;

	if ((lang == NULL) || (str == NULL)) {
		return -EINVAL;
	}

	ret = ndef_writer_record_begin(w, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_text_payload_encode(payload, cap, NDEF_TEXT_UTF8, lang,
				       ndef_strnlen(lang, NDEF_TEXT_LANG_MAX), str, strlen(str),
				       &len);
	if (ret < 0) {
		ndef_writer_record_abort(w);
		return ret;
	}

	return ndef_writer_record_end(w, len);
}
