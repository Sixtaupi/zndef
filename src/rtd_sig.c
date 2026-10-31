/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_sig.h>

#include "ndef_priv.h"

#define SIG_URI_FLAG    0x80U
#define SIG_TYPE_MASK   0x7FU
#define CERT_URI_FLAG   0x80U
#define CERT_FMT_SHIFT  4U
#define CERT_FMT_MASK   0x07U
#define CERT_COUNT_MASK 0x0FU

/* Read a 16 bit length prefixed field. */
static int blob_get(const uint8_t *p, size_t len, size_t *off, struct ndef_sig_blob *blob)
{
	uint16_t n;

	if ((len - *off) < 2U) {
		return -EBADMSG;
	}

	n = ndef_get_be16(&p[*off]);
	*off += 2U;

	if ((len - *off) < n) {
		return -EBADMSG;
	}

	blob->data = (n > 0U) ? &p[*off] : NULL;
	blob->len = n;
	*off += n;

	return 0;
}

static bool uri_text_valid(const struct ndef_sig_blob *uri)
{
	if (!ndef_utf8_valid(uri->data, uri->len)) {
		return false;
	}

	for (uint16_t i = 0; i < uri->len; i++) {
		if ((uri->data[i] < 0x20U) || (uri->data[i] == 0x7FU)) {
			return false;
		}
	}

	return true;
}

int ndef_sig_payload_decode(const uint8_t *payload, size_t len, struct ndef_sig *sig)
{
	struct ndef_sig_blob cert;
	size_t off = 0;
	size_t cert_start;
	uint8_t v;
	int ret;

	if ((payload == NULL) || (sig == NULL)) {
		return -EINVAL;
	}

	(void)memset(sig, 0, sizeof(*sig));

	if (len < 1U) {
		return -EBADMSG;
	}

	sig->version = payload[off++];
	if ((sig->version != NDEF_SIG_VERSION_1_0) && ((sig->version >> 4) != 2U)) {
		return -ENOTSUP;
	}

	/* Signature field. */
	if ((len - off) < 1U) {
		return -EBADMSG;
	}
	v = payload[off++];
	sig->signature_is_uri = (v & SIG_URI_FLAG) != 0U;
	sig->type = v & SIG_TYPE_MASK;

	if (sig->version != NDEF_SIG_VERSION_1_0) {
		if ((len - off) < 1U) {
			return -EBADMSG;
		}
		sig->hash = payload[off++];
	}

	ret = blob_get(payload, len, &off, &sig->signature);
	if (ret < 0) {
		return ret;
	}

	if (sig->signature_is_uri && !uri_text_valid(&sig->signature)) {
		return -EBADMSG;
	}

	/* A marker record has neither signature nor certificate. */
	if ((sig->type == (uint8_t)NDEF_SIG_TYPE_NONE) && (off == len)) {
		return 0;
	}

	/* Certificate chain field. */
	if ((len - off) < 1U) {
		return -EBADMSG;
	}
	v = payload[off++];
	sig->has_cert_uri = (v & CERT_URI_FLAG) != 0U;
	sig->cert_format = (v >> CERT_FMT_SHIFT) & CERT_FMT_MASK;
	sig->cert_count = v & CERT_COUNT_MASK;

	cert_start = off;
	sig->cert_store = &payload[off];
	for (uint8_t i = 0; i < sig->cert_count; i++) {
		ret = blob_get(payload, len, &off, &cert);
		if (ret < 0) {
			return ret;
		}
	}
	sig->cert_store_len = off - cert_start;

	if (sig->has_cert_uri) {
		ret = blob_get(payload, len, &off, &sig->cert_uri);
		if (ret < 0) {
			return ret;
		}
		if (!uri_text_valid(&sig->cert_uri)) {
			return -EBADMSG;
		}
	}

	return (off == len) ? 0 : -EBADMSG;
}

int ndef_sig_parse(const struct ndef_record *rec, struct ndef_sig *sig)
{
	int ret = ndef_rtd_check(rec, NDEF_RTD_SIG);

	if (ret < 0) {
		return ret;
	}

	return ndef_sig_payload_decode(rec->payload, rec->payload_len, sig);
}

int ndef_sig_cert_get(const struct ndef_sig *sig, uint8_t index, struct ndef_sig_blob *cert)
{
	size_t off = 0;

	if ((sig == NULL) || (cert == NULL)) {
		return -EINVAL;
	}

	if ((index >= sig->cert_count) || (sig->cert_store == NULL)) {
		return -ENOENT;
	}

	for (uint8_t i = 0; i <= index; i++) {
		int ret = blob_get(sig->cert_store, sig->cert_store_len, &off, cert);

		if (ret < 0) {
			return -ENOENT;
		}
	}

	return 0;
}

static int blob_put(uint8_t *buf, size_t size, size_t *off, const struct ndef_sig_blob *blob)
{
	if ((blob->data == NULL) && (blob->len > 0U)) {
		return -EINVAL;
	}

	if (((size - *off) < 2U) || ((size - *off - 2U) < blob->len)) {
		return -ENOSPC;
	}

	ndef_put_be16(&buf[*off], blob->len);
	*off += 2U;
	if (blob->len > 0U) {
		(void)memcpy(&buf[*off], blob->data, blob->len);
		*off += blob->len;
	}

	return 0;
}

int ndef_sig_payload_encode(const struct ndef_sig *sig, uint8_t *buf, size_t size, size_t *out_len)
{
	size_t off = 0;
	bool marker;
	int ret;

	if ((sig == NULL) || (buf == NULL) || (out_len == NULL)) {
		return -EINVAL;
	}

	if (((sig->version != NDEF_SIG_VERSION_1_0) && (sig->version != NDEF_SIG_VERSION_2_0)) ||
	    (sig->type > SIG_TYPE_MASK) || (sig->cert_format > CERT_FMT_MASK) ||
	    (sig->cert_count > NDEF_SIG_CERT_MAX) ||
	    ((sig->certs == NULL) && (sig->cert_count > 0U))) {
		return -EINVAL;
	}

	if ((sig->signature_is_uri && !uri_text_valid(&sig->signature)) ||
	    (sig->has_cert_uri && !uri_text_valid(&sig->cert_uri))) {
		return -EINVAL;
	}

	marker = (sig->type == (uint8_t)NDEF_SIG_TYPE_NONE) && (sig->signature.len == 0U) &&
		 (sig->cert_count == 0U) && !sig->has_cert_uri;

	if (size < 3U) {
		return -ENOSPC;
	}

	buf[off++] = sig->version;
	buf[off++] = sig->type | (sig->signature_is_uri ? SIG_URI_FLAG : 0U);
	if (sig->version != NDEF_SIG_VERSION_1_0) {
		buf[off++] = sig->hash;
	}

	ret = blob_put(buf, size, &off, &sig->signature);
	if (ret < 0) {
		return ret;
	}

	if (!marker) {
		if (off >= size) {
			return -ENOSPC;
		}
		buf[off++] = (uint8_t)((sig->has_cert_uri ? CERT_URI_FLAG : 0U) |
				       (sig->cert_format << CERT_FMT_SHIFT) | sig->cert_count);

		for (uint8_t i = 0; i < sig->cert_count; i++) {
			ret = blob_put(buf, size, &off, &sig->certs[i]);
			if (ret < 0) {
				return ret;
			}
		}

		if (sig->has_cert_uri) {
			ret = blob_put(buf, size, &off, &sig->cert_uri);
			if (ret < 0) {
				return ret;
			}
		}
	}

	*out_len = off;

	return 0;
}

int ndef_sig_write(struct ndef_writer *w, const struct ndef_sig *sig)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_SIG);
	uint8_t *payload;
	size_t cap;
	size_t len;
	int ret;

	ret = ndef_writer_record_begin(w, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_sig_payload_encode(sig, payload, cap, &len);
	if (ret < 0) {
		ndef_writer_record_abort(w);
		return ret;
	}

	return ndef_writer_record_end(w, len);
}

int ndef_sig_foreach_signed(const uint8_t *msg, size_t len, uint32_t sig_index,
			    ndef_sig_record_cb cb, void *user_data)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	uint32_t first = 0;
	uint32_t i;
	int ret;

	if (cb == NULL) {
		return -EINVAL;
	}

	/* First pass: locate the previous Signature record. */
	ret = ndef_msg_iter_init(&it, msg, len);
	if (ret < 0) {
		return ret;
	}

	for (i = 0; i <= sig_index; i++) {
		ret = ndef_msg_iter_next(&it, &rec);
		if (ret < 0) {
			return (ret == -ENOENT) ? -EINVAL : ret;
		}

		if (ndef_record_is_wkt(&rec, NDEF_RTD_SIG) && (i < sig_index)) {
			first = i + 1U;
		}
	}

	if (!ndef_record_is_wkt(&rec, NDEF_RTD_SIG)) {
		return -EINVAL;
	}

	/* Second pass: hand the covered records over. */
	(void)ndef_msg_iter_init(&it, msg, len);

	for (i = 0; i < sig_index; i++) {
		ret = ndef_msg_iter_next(&it, &rec);
		if (ret < 0) {
			return ret;
		}

		if (i >= first) {
			ret = cb(&rec, user_data);
			if (ret < 0) {
				return ret;
			}
		}
	}

	return 0;
}

int ndef_sig_record_data(const struct ndef_record *rec, ndef_sig_data_cb cb, void *user_data)
{
	struct ndef_record chunk;
	size_t pos = 0;
	int ret;

	if ((rec == NULL) || (cb == NULL)) {
		return -EINVAL;
	}

	if (rec->type_len > 0U) {
		ret = cb(rec->type, rec->type_len, user_data);
		if (ret < 0) {
			return ret;
		}
	}

	if (rec->id_len > 0U) {
		ret = cb(rec->id, rec->id_len, user_data);
		if (ret < 0) {
			return ret;
		}
	}

	if (rec->chunk_count <= 1U) {
		return (rec->payload_len > 0U) ? cb(rec->payload, rec->payload_len, user_data) : 0;
	}

	while (pos < rec->raw_len) {
		ret = ndef_record_decode(&rec->raw[pos], rec->raw_len - pos, &chunk);
		if (ret < 0) {
			return ret;
		}

		if (chunk.payload_len > 0U) {
			ret = cb(chunk.payload, chunk.payload_len, user_data);
			if (ret < 0) {
				return ret;
			}
		}

		pos += chunk.raw_len;
	}

	return 0;
}
