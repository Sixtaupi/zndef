/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_uri.h>

#include "ndef_priv.h"

/* URI identifier codes, RTD-URI 1.0 table 3. */
static const char *const uri_prefixes[NDEF_URI_CODE_MAX + 1U] = {
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

/*
 * UTF-8 IRI without characters 0 to 31, whose presence discards the record
 * (RTD-URI 1.0 section 3.2.3).
 */
static bool field_valid(const uint8_t *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (s[i] < 0x20U) {
			return false;
		}
	}

	return ndef_utf8_valid(s, len);
}

const char *ndef_uri_prefix(uint8_t code)
{
	/* RFU codes are treated as 0x00 (RTD-URI 1.0 section 3.2.2). */
	return (code <= NDEF_URI_CODE_MAX) ? uri_prefixes[code] : uri_prefixes[0];
}

int ndef_uri_payload_decode(const uint8_t *payload, size_t len, struct ndef_uri *uri)
{
	if ((payload == NULL) || (uri == NULL)) {
		return -EINVAL;
	}

	if ((len < 1U) || !field_valid(&payload[1], len - 1U)) {
		return -EBADMSG;
	}

	uri->code = payload[0];
	uri->prefix = ndef_uri_prefix(payload[0]);
	uri->field = (const char *)&payload[1];
	uri->field_len = len - 1U;

	return 0;
}

int ndef_uri_parse(const struct ndef_record *rec, struct ndef_uri *uri)
{
	int ret = ndef_rtd_check(rec, NDEF_RTD_URI);

	if (ret < 0) {
		return ret;
	}

	return ndef_uri_payload_decode(rec->payload, rec->payload_len, uri);
}

int ndef_uri_to_string(const struct ndef_uri *uri, char *dst, size_t size, size_t *out_len)
{
	size_t plen;

	if ((uri == NULL) || (uri->prefix == NULL) || (dst == NULL) ||
	    ((uri->field == NULL) && (uri->field_len > 0U))) {
		return -EINVAL;
	}

	plen = strlen(uri->prefix);
	if ((size <= plen) || ((size - plen) <= uri->field_len)) {
		return -ENOSPC;
	}

	(void)memcpy(dst, uri->prefix, plen);
	if (uri->field_len > 0U) {
		(void)memcpy(&dst[plen], uri->field, uri->field_len);
	}
	dst[plen + uri->field_len] = '\0';

	if (out_len != NULL) {
		*out_len = plen + uri->field_len;
	}

	return 0;
}

int ndef_uri_payload_encode(uint8_t *buf, size_t size, const char *str, size_t str_len,
			    size_t *out_len)
{
	uint8_t code = 0U;
	size_t best = 0U;

	if ((buf == NULL) || (out_len == NULL) || ((str == NULL) && (str_len > 0U))) {
		return -EINVAL;
	}

	if (!field_valid((const uint8_t *)str, str_len)) {
		return -EINVAL;
	}

	for (uint8_t i = 1U; i <= NDEF_URI_CODE_MAX; i++) {
		size_t plen = strlen(uri_prefixes[i]);

		if ((plen > best) && (plen <= str_len) &&
		    (strncmp(str, uri_prefixes[i], plen) == 0)) {
			best = plen;
			code = i;
		}
	}

	if ((size < 1U) || ((size - 1U) < (str_len - best))) {
		return -ENOSPC;
	}

	buf[0] = code;
	if (str_len > best) {
		(void)memcpy(&buf[1], &str[best], str_len - best);
	}

	*out_len = 1U + str_len - best;

	return 0;
}

int ndef_uri_write(struct ndef_writer *w, const char *str)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_URI);
	uint8_t *payload;
	size_t cap;
	size_t len;
	int ret;

	if (str == NULL) {
		return -EINVAL;
	}

	ret = ndef_writer_record_begin(w, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_uri_payload_encode(payload, cap, str, strlen(str), &len);
	if (ret < 0) {
		ndef_writer_record_abort(w);
		return ret;
	}

	return ndef_writer_record_end(w, len);
}
