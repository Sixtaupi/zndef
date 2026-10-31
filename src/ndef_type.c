/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/ndef.h>

#include "ndef_priv.h"

/* RTD 1.0 <other> characters. */
static bool wkt_other(uint8_t c)
{
	return (c != 0U) && (strchr("()+,-:=@;$_!*'.", (int)c) != NULL);
}

static uint8_t hex_value(uint8_t c)
{
	return ndef_is_digit(c) ? (uint8_t)(c - (uint8_t)'0')
				: (uint8_t)(ndef_ascii_lower(c) - (uint8_t)'a' + 10U);
}

/* Read one percent-encoded octet, advancing only after a complete %HH. */
static bool pct_octet(const uint8_t *s, size_t len, size_t *i, uint8_t *octet)
{
	if (((len - *i) < 3U) || (s[*i] != (uint8_t)'%') || !ndef_is_hex(s[*i + 1U]) ||
	    !ndef_is_hex(s[*i + 2U])) {
		return false;
	}

	*octet = (uint8_t)((hex_value(s[*i + 1U]) << 4) | hex_value(s[*i + 2U]));
	*i += 3U;

	return true;
}

/* RTD 1.0 section 3.2: percent-encoded external names encode UTF-8 first. */
static bool pct_utf8(const uint8_t *s, size_t len, size_t *i)
{
	uint32_t cp;
	uint32_t min;
	uint8_t c;
	size_t trailing;

	if (!pct_octet(s, len, i, &c)) {
		return false;
	}

	if (c < 0x80U) {
		return true;
	} else if ((c >= 0xC2U) && (c <= 0xDFU)) {
		cp = (uint32_t)c & 0x1FU;
		min = 0x80U;
		trailing = 1U;
	} else if ((c >= 0xE0U) && (c <= 0xEFU)) {
		cp = (uint32_t)c & 0x0FU;
		min = 0x800U;
		trailing = 2U;
	} else if ((c >= 0xF0U) && (c <= 0xF4U)) {
		cp = (uint32_t)c & 0x07U;
		min = 0x10000U;
		trailing = 3U;
	} else {
		return false;
	}

	for (size_t n = 0; n < trailing; n++) {
		if (!pct_octet(s, len, i, &c) || ((c & 0xC0U) != 0x80U)) {
			return false;
		}
		cp = (cp << 6) | (c & 0x3FU);
	}

	return (cp >= min) && (cp <= 0x10FFFFU) && ((cp < 0xD800U) || (cp > 0xDFFFU));
}

/*
 * 1*WKT-char (RTD 1.0 chapter 3). Percent encoding is forbidden in
 * well-known types and discouraged, but allowed, in external types.
 */
static bool wkt_chars_valid(const uint8_t *s, size_t len, bool pct)
{
	size_t i = 0;

	if (len == 0U) {
		return false;
	}

	while (i < len) {
		uint8_t c = s[i];

		if (pct && (c == (uint8_t)'%')) {
			if (!pct_utf8(s, len, &i)) {
				return false;
			}
			continue;
		}

		if (!ndef_is_alpha(c) && !ndef_is_digit(c) && !wkt_other(c)) {
			return false;
		}

		i++;
	}

	return true;
}

/* WKT-type = global / local: starts with <upper>, or with <lower> / <number>. */
static bool wkt_valid(const uint8_t *s, size_t len)
{
	return (len > 0U) && (ndef_is_alpha(s[0]) || ndef_is_digit(s[0])) &&
	       wkt_chars_valid(s, len, false);
}

/* External type: dns-part ":" name-part (RTD 1.0 chapter 3). */
static bool external_valid(const uint8_t *s, size_t len)
{
	size_t sep = 0;

	while ((sep < len) && (s[sep] != (uint8_t)':')) {
		uint8_t c = s[sep];

		if (!ndef_is_alpha(c) && !ndef_is_digit(c) && (c != (uint8_t)'.') &&
		    (c != (uint8_t)'-')) {
			return false;
		}
		sep++;
	}

	if ((sep == 0U) || (sep >= (len - 1U))) {
		return false;
	}

	return wkt_chars_valid(&s[sep + 1U], len - sep - 1U, true);
}

/* RFC 2045 token character. */
static bool token_char(uint8_t c)
{
	if ((c <= 0x20U) || (c >= 0x7FU)) {
		return false;
	}

	return strchr("()<>@,;:\\\"/[]?=", (int)c) == NULL;
}

static size_t token_len(const uint8_t *s, size_t len)
{
	size_t n = 0;

	while ((n < len) && token_char(s[n])) {
		n++;
	}

	return n;
}

static size_t skip_ws(const uint8_t *s, size_t len, size_t i)
{
	size_t scratch_i = i;

	while ((scratch_i < len) &&
	       ((s[scratch_i] == (uint8_t)' ') || (s[scratch_i] == (uint8_t)'\t'))) {
		scratch_i++;
	}

	return scratch_i;
}

/* RFC 2046 media type: type "/" subtype *(";" attribute "=" value). */
static bool media_valid(const uint8_t *s, size_t len)
{
	size_t i;
	size_t n;

	n = token_len(s, len);
	if ((n == 0U) || (n >= len) || (s[n] != (uint8_t)'/')) {
		return false;
	}

	i = n + 1U;
	n = token_len(&s[i], len - i);
	if (n == 0U) {
		return false;
	}
	i += n;

	while (true) {
		i = skip_ws(s, len, i);
		if (i == len) {
			return true;
		}

		if (s[i] != (uint8_t)';') {
			return false;
		}

		i = skip_ws(s, len, i + 1U);
		n = token_len(&s[i], len - i);
		if (n == 0U) {
			return false;
		}
		i += n;

		if ((i >= len) || (s[i] != (uint8_t)'=')) {
			return false;
		}
		i++;

		if ((i < len) && (s[i] == (uint8_t)'"')) {
			/* quoted-string: any CHAR but '"', '\' and CR, or a quoted-pair. */
			i++;
			while ((i < len) && (s[i] != (uint8_t)'"')) {
				if (s[i] == (uint8_t)'\\') {
					i++;
					if ((i >= len) || (s[i] >= 0x80U)) {
						return false;
					}
				} else if ((s[i] == (uint8_t)'\r') || (s[i] >= 0x80U)) {
					return false;
				} else {
					/* Unescaped ASCII character in a quoted string. */
				}
				i++;
			}
			if (i >= len) {
				return false;
			}
			i++;
		} else {
			n = token_len(&s[i], len - i);
			if (n == 0U) {
				return false;
			}
			i += n;
		}
	}
}

/*
 * RFC 3986 URI character: unreserved, reserved or percent encoded. Octets
 * above 0x7F are accepted: NDEF leaves the encoding of non US-ASCII
 * characters to the application (NDEF 1.0 section 2.4.2).
 */
static bool uri_chars_valid(const uint8_t *s, size_t len, bool fragment)
{
	size_t i = 0;

	while (i < len) {
		uint8_t c = s[i];

		i++;
		if (c == (uint8_t)'%') {
			if (((len - i) < 2U) || !ndef_is_hex(s[i]) || !ndef_is_hex(s[i + 1U])) {
				return false;
			}
			i += 2U;
			continue;
		}

		if (ndef_is_alpha(c) || ndef_is_digit(c) || (c >= 0x80U)) {
			continue;
		}

		if ((c == (uint8_t)'#') && !fragment) {
			return false;
		}

		if ((c == 0U) || (strchr("-._~:/?#[]@!$&'()*+,;=", (int)c) == NULL)) {
			return false;
		}
	}

	return true;
}

/* absolute-URI = scheme ":" hier-part [ "?" query ], without fragment. */
static bool uri_valid(const uint8_t *s, size_t len)
{
	size_t i;

	/* scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
	if ((len == 0U) || !ndef_is_alpha(s[0])) {
		return false;
	}

	for (i = 1U; (i < len) && (s[i] != (uint8_t)':'); i++) {
		if (!ndef_is_alpha(s[i]) && !ndef_is_digit(s[i]) && (s[i] != (uint8_t)'+') &&
		    (s[i] != (uint8_t)'-') && (s[i] != (uint8_t)'.')) {
			return false;
		}
	}

	if (i == len) {
		return false;
	}

	return uri_chars_valid(&s[i + 1U], len - i - 1U, false);
}

int ndef_type_validate(enum ndef_tnf tnf, const uint8_t *type, size_t type_len)
{
	bool ok;

	if ((type == NULL) && (type_len > 0U)) {
		return -EINVAL;
	}

	switch (tnf) {
	case NDEF_TNF_EMPTY:
	case NDEF_TNF_UNKNOWN:
	case NDEF_TNF_UNCHANGED:
		ok = (type_len == 0U);
		break;
	case NDEF_TNF_WELL_KNOWN:
		ok = wkt_valid(type, type_len);
		break;
	case NDEF_TNF_MEDIA:
		ok = media_valid(type, type_len);
		break;
	case NDEF_TNF_ABSOLUTE_URI:
		ok = uri_valid(type, type_len);
		break;
	case NDEF_TNF_EXTERNAL:
		ok = (type_len > 0U) && external_valid(type, type_len);
		break;
	default:
		ok = false;
		break;
	}

	return ok ? 0 : -EINVAL;
}

bool ndef_wkt_is_local(const uint8_t *type, size_t type_len)
{
	if ((type == NULL) || (type_len == 0U)) {
		return false;
	}

	return ((type[0] >= (uint8_t)'a') && (type[0] <= (uint8_t)'z')) || ndef_is_digit(type[0]);
}

int ndef_id_validate(const uint8_t *id, size_t id_len)
{
	if ((id == NULL) && (id_len > 0U)) {
		return -EINVAL;
	}

	/* URI reference, absolute or relative (NDEF 1.0 section 3.2.11). */
	return uri_chars_valid(id, id_len, true) ? 0 : -EINVAL;
}
