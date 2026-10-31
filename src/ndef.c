/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/ndef.h>

#include "ndef_priv.h"

bool ndef_casecmp_eq(const uint8_t *a, const uint8_t *b, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (ndef_ascii_lower(a[i]) != ndef_ascii_lower(b[i])) {
			return false;
		}
	}

	return true;
}

size_t ndef_strnlen(const char *s, size_t max)
{
	size_t n = 0;

	while ((n <= max) && (s[n] != '\0')) {
		n++;
	}

	return n;
}

int ndef_rtd_check(const struct ndef_record *rec, const char *type)
{
	if (rec == NULL) {
		return -EINVAL;
	}

	if (!ndef_record_is_wkt(rec, type)) {
		return -ENOMSG;
	}

	if (rec->chunk_count > 1U) {
		return -ENOTSUP;
	}

	return 0;
}

bool ndef_utf8_valid(const uint8_t *s, size_t len)
{
	size_t i = 0;

	if (s == NULL) {
		return len == 0U;
	}

	while (i < len) {
		uint8_t c = s[i];
		size_t n;
		uint32_t cp;
		uint32_t min;

		if (c < 0x80U) {
			i++;
			continue;
		} else if ((c & 0xE0U) == 0xC0U) {
			n = 1U;
			cp = (uint32_t)c & 0x1FU;
			min = 0x80U;
		} else if ((c & 0xF0U) == 0xE0U) {
			n = 2U;
			cp = (uint32_t)c & 0x0FU;
			min = 0x800U;
		} else if ((c & 0xF8U) == 0xF0U) {
			n = 3U;
			cp = (uint32_t)c & 0x07U;
			min = 0x10000U;
		} else {
			return false;
		}

		if ((len - i) <= n) {
			return false;
		}

		for (size_t k = 1; k <= n; k++) {
			if ((s[i + k] & 0xC0U) != 0x80U) {
				return false;
			}
			cp = (cp << 6) | (s[i + k] & 0x3FU);
		}

		if ((cp < min) || (cp > 0x10FFFFU) || ((cp >= 0xD800U) && (cp <= 0xDFFFU))) {
			return false;
		}

		i += n + 1U;
	}

	return true;
}

int ndef_record_decode(const uint8_t *buf, size_t len, struct ndef_record *rec)
{
	size_t off = 2U;
	uint8_t hdr;
	uint8_t type_len;
	uint8_t id_len = 0U;
	uint32_t payload_len;
	enum ndef_tnf tnf;

	if ((buf == NULL) || (rec == NULL)) {
		return -EINVAL;
	}

	if (len < 3U) {
		return -EBADMSG;
	}

	hdr = buf[0];
	{
		uint8_t tnf_value = hdr & NDEF_HDR_TNF_MASK;

		tnf = (enum ndef_tnf)tnf_value;
	}
	type_len = buf[1];

	if ((hdr & NDEF_HDR_SR) != 0U) {
		payload_len = buf[off];
		off += 1U;
	} else {
		if ((len - off) < 4U) {
			return -EBADMSG;
		}
		payload_len = ndef_get_be32(&buf[off]);
		off += 4U;
	}

	if ((hdr & NDEF_HDR_IL) != 0U) {
		if (off >= len) {
			return -EBADMSG;
		}
		id_len = buf[off];
		off += 1U;
	}

	if ((len - off) < ((size_t)type_len + id_len)) {
		return -EBADMSG;
	}

	rec->type = (type_len > 0U) ? &buf[off] : NULL;
	off += type_len;
	rec->id = (id_len > 0U) ? &buf[off] : NULL;
	off += id_len;

	if ((len - off) < payload_len) {
		return -EBADMSG;
	}

	rec->payload = &buf[off];
	off += payload_len;

	switch (tnf) {
	case NDEF_TNF_EMPTY:
		if ((type_len != 0U) || (id_len != 0U) || (payload_len != 0U) ||
		    ((hdr & NDEF_HDR_CF) != 0U)) {
			return -EBADMSG;
		}
		break;
	case NDEF_TNF_WELL_KNOWN:
	case NDEF_TNF_MEDIA:
	case NDEF_TNF_ABSOLUTE_URI:
	case NDEF_TNF_EXTERNAL:
		if (type_len == 0U) {
			return -EBADMSG;
		}
		break;
	case NDEF_TNF_UNKNOWN:
		if (type_len != 0U) {
			return -EBADMSG;
		}
		break;
	case NDEF_TNF_UNCHANGED:
		/* Middle and terminating chunks carry neither TYPE nor ID. */
		if ((type_len != 0U) || ((hdr & NDEF_HDR_IL) != 0U)) {
			return -EBADMSG;
		}
		break;
	default:
		/* Reserved TNF: to be treated as Unknown (NDEF 1.0 section 3.2.6). */
		tnf = NDEF_TNF_UNKNOWN;
		break;
	}

	rec->header = hdr;
	rec->tnf = tnf;
	rec->type_len = type_len;
	rec->id_len = id_len;
	rec->payload_len = payload_len;
	rec->raw = buf;
	rec->raw_len = off;
	rec->chunk_count = 1U;

	return 0;
}

int ndef_msg_iter_init(struct ndef_msg_iter *it, const uint8_t *buf, size_t len)
{
	if ((it == NULL) || ((buf == NULL) && (len > 0U))) {
		return -EINVAL;
	}

	it->buf = buf;
	it->len = len;
	it->off = 0U;
	it->index = 0U;
	it->done = false;

	return 0;
}

/* Decode and accumulate one continuation chunk. */
static int continuation_next(const struct ndef_msg_iter *it, struct ndef_record *rec, size_t *off,
			     struct ndef_record *chunk)
{
	int ret;

	if (*off >= it->len) {
		return -EBADMSG;
	}

	ret = ndef_record_decode(&it->buf[*off], it->len - *off, chunk);
	if (ret < 0) {
		return ret;
	}

	if (((chunk->header & NDEF_HDR_MB) != 0U) || (chunk->tnf != NDEF_TNF_UNCHANGED) ||
	    (((chunk->header & NDEF_HDR_CF) != 0U) && ((chunk->header & NDEF_HDR_ME) != 0U))) {
		return -EBADMSG;
	}

	if (chunk->payload_len > (UINT32_MAX - rec->payload_len)) {
		return -EOVERFLOW;
	}

	rec->payload_len += chunk->payload_len;
	rec->chunk_count++;
	rec->raw_len += chunk->raw_len;
	*off += chunk->raw_len;

	return 0;
}

int ndef_msg_iter_next(struct ndef_msg_iter *it, struct ndef_record *rec)
{
	struct ndef_record chunk;
	size_t off;
	int ret;

	if ((it == NULL) || (rec == NULL)) {
		return -EINVAL;
	}

	if (it->done) {
		return -ENOENT;
	}

	/* A message ends with ME: running out of data is an error. */
	if (it->off >= it->len) {
		it->done = true;
		return -EBADMSG;
	}

	ret = ndef_record_decode(&it->buf[it->off], it->len - it->off, rec);
	if (ret < 0) {
		it->done = true;
		return ret;
	}

	ret = -EBADMSG;

	if (((rec->header & NDEF_HDR_MB) != 0U) != (it->index == 0U)) {
		it->done = true;
		return ret;
	}

	if (rec->tnf == NDEF_TNF_UNCHANGED) {
		it->done = true;
		return ret;
	}

	off = it->off + rec->raw_len;

	if ((rec->header & NDEF_HDR_CF) != 0U) {
		/* A chunked payload never spans several messages. */
		if ((rec->header & NDEF_HDR_ME) != 0U) {
			it->done = true;
			return ret;
		}

		do {
			ret = continuation_next(it, rec, &off, &chunk);
			if (ret < 0) {
				it->done = true;
				return ret;
			}
		} while ((chunk.header & NDEF_HDR_CF) != 0U);

		if ((chunk.header & NDEF_HDR_ME) != 0U) {
			it->done = true;
		}
	} else if ((rec->header & NDEF_HDR_ME) != 0U) {
		it->done = true;
	} else {
		/* A following record must terminate the message. */
	}

	it->off = off;
	it->index++;

	return 0;
}

int ndef_msg_validate_buf(const uint8_t *buf, size_t len, uint32_t flags, uint8_t *scratch,
			  size_t scratch_size, size_t *msg_len, uint32_t *count)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	int ret;

	if ((scratch == NULL) && (scratch_size > 0U)) {
		return -EINVAL;
	}

	ret = ndef_msg_iter_init(&it, buf, len);
	if (ret < 0) {
		return ret;
	}

#if !defined(CONFIG_ZNDEF_VALIDATE_RTD)
	if ((flags & NDEF_VALIDATE_RTD) != 0U) {
		return -ENOTSUP;
	}
#endif

	for (ret = ndef_msg_iter_next(&it, &rec); ret == 0; ret = ndef_msg_iter_next(&it, &rec)) {
		/* Generators must not use the reserved TNF. */
		if ((rec.header & NDEF_HDR_TNF_MASK) == (uint8_t)NDEF_TNF_RESERVED) {
			return -EBADMSG;
		}

		if (((flags & NDEF_VALIDATE_TYPES) != 0U) &&
		    ((ndef_type_validate(rec.tnf, rec.type, rec.type_len) < 0) ||
		     (ndef_id_validate(rec.id, rec.id_len) < 0))) {
			return -EBADMSG;
		}

#if defined(CONFIG_ZNDEF_VALIDATE_RTD)
		if (((flags & NDEF_VALIDATE_RTD) != 0U) && (rec.tnf == NDEF_TNF_WELL_KNOWN)) {
			int val = ndef_rtd_validate(&rec, scratch, scratch_size);
			if (val < 0) {
				return val;
			}
		}
#endif
	}

	if (ret != -ENOENT) {
		return ret;
	}

	if (((flags & NDEF_VALIDATE_EXACT) != 0U) && (it.off != len)) {
		return -EBADMSG;
	}

	if (msg_len != NULL) {
		*msg_len = it.off;
	}

	if (count != NULL) {
		*count = it.index;
	}

	return 0;
}

int ndef_msg_validate(const uint8_t *buf, size_t len, uint32_t flags, size_t *msg_len,
		      uint32_t *count)
{
	return ndef_msg_validate_buf(buf, len, flags, NULL, 0U, msg_len, count);
}

int ndef_msg_find_type(const uint8_t *buf, size_t len, enum ndef_tnf tnf, const uint8_t *type,
		       size_t type_len, struct ndef_record *rec)
{
	struct ndef_msg_iter it;
	int ret;

	if ((rec == NULL) || ((type == NULL) && (type_len > 0U))) {
		return -EINVAL;
	}

	ret = ndef_msg_iter_init(&it, buf, len);
	if (ret < 0) {
		return ret;
	}

	for (ret = ndef_msg_iter_next(&it, rec); ret == 0; ret = ndef_msg_iter_next(&it, rec)) {
		if (ndef_record_type_eq(rec, tnf, type, type_len)) {
			return 0;
		}
	}

	return ret;
}

int ndef_msg_find_id(const uint8_t *buf, size_t len, const uint8_t *id, size_t id_len,
		     struct ndef_record *rec)
{
	struct ndef_msg_iter it;
	int ret;

	if ((rec == NULL) || (id == NULL) || (id_len == 0U)) {
		return -EINVAL;
	}

	ret = ndef_msg_iter_init(&it, buf, len);
	if (ret < 0) {
		return ret;
	}

	for (ret = ndef_msg_iter_next(&it, rec); ret == 0; ret = ndef_msg_iter_next(&it, rec)) {
		if ((rec->id_len == id_len) && (memcmp(rec->id, id, id_len) == 0)) {
			return 0;
		}
	}

	return ret;
}

/* Length of a media type without its parameters and trailing white space. */
static size_t media_base_len(const uint8_t *type, size_t len)
{
	size_t n = 0;

	while ((n < len) && (type[n] != (uint8_t)';')) {
		n++;
	}

	while ((n > 0U) && ((type[n - 1U] == (uint8_t)' ') || (type[n - 1U] == (uint8_t)'\t'))) {
		n--;
	}

	return n;
}

bool ndef_record_type_eq(const struct ndef_record *rec, enum ndef_tnf tnf, const uint8_t *type,
			 size_t type_len)
{
	const uint8_t *t = type;
	size_t a;
	size_t b;

	if ((rec == NULL) || (rec->tnf != tnf) || ((t == NULL) && (type_len > 0U))) {
		return false;
	}

	switch (tnf) {
	case NDEF_TNF_MEDIA:
		a = media_base_len(rec->type, rec->type_len);
		b = media_base_len(t, type_len);
		return (a == b) && ndef_casecmp_eq(rec->type, t, a);
	case NDEF_TNF_EXTERNAL:
		return (rec->type_len == type_len) && ndef_casecmp_eq(rec->type, t, type_len);
	case NDEF_TNF_WELL_KNOWN:
	case NDEF_TNF_ABSOLUTE_URI:
		return (rec->type_len == type_len) && (memcmp(rec->type, t, type_len) == 0);
	default:
		return (rec->type_len == 0U) && (type_len == 0U);
	}
}

bool ndef_record_is_wkt(const struct ndef_record *rec, const char *type)
{
	if (type == NULL) {
		return false;
	}

	return ndef_record_type_eq(rec, NDEF_TNF_WELL_KNOWN, (const uint8_t *)type, strlen(type));
}

int ndef_record_payload_read(const struct ndef_record *rec, size_t offset, uint8_t *dst, size_t len)
{
	struct ndef_record chunk;
	uint8_t *out = dst;
	size_t pos = 0;
	size_t scratch_size = len;
	size_t scratch_offset = offset;

	if ((rec == NULL) || ((dst == NULL) && (len > 0U)) || (offset > rec->payload_len) ||
	    (len > (rec->payload_len - offset))) {
		return -EINVAL;
	}

	if (rec->chunk_count <= 1U) {
		if (len > 0U) {
			(void)memcpy(out, &rec->payload[offset], len);
		}
		return 0;
	}

	while (scratch_size > 0U) {
		int ret;

		if (pos >= rec->raw_len) {
			return -EBADMSG;
		}

		ret = ndef_record_decode(&rec->raw[pos], rec->raw_len - pos, &chunk);
		if (ret < 0) {
			return ret;
		}

		if (scratch_offset < chunk.payload_len) {
			size_t n = chunk.payload_len - scratch_offset;

			if (n > scratch_size) {
				n = scratch_size;
			}
			(void)memcpy(out, &chunk.payload[scratch_offset], n);
			out = &out[n];
			scratch_size -= n;
			scratch_offset = 0U;
		} else {
			scratch_offset -= chunk.payload_len;
		}

		pos += chunk.raw_len;
	}

	return 0;
}

int ndef_record_reassemble(struct ndef_record *rec, uint8_t *dst, size_t size)
{
	int ret;

	if (rec == NULL) {
		return -EINVAL;
	}

	if (rec->chunk_count <= 1U) {
		return 0;
	}

	if ((dst == NULL) || (size < rec->payload_len)) {
		return (dst == NULL) ? -EINVAL : -ENOSPC;
	}

	ret = ndef_record_payload_read(rec, 0U, dst, rec->payload_len);
	if (ret < 0) {
		return ret;
	}

	rec->payload = dst;
	rec->chunk_count = 1U;

	return 0;
}
