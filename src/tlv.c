/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/tlv.h>

#include "ndef_priv.h"

#define TLV_LONG_LEN 0xFFU

int ndef_tlv_iter_init(struct ndef_tlv_iter *it, const uint8_t *buf, size_t len)
{
	if ((it == NULL) || ((buf == NULL) && (len > 0U))) {
		return -EINVAL;
	}

	it->buf = buf;
	it->len = len;
	it->off = 0U;
	it->done = false;

	return 0;
}

int ndef_tlv_iter_next(struct ndef_tlv_iter *it, struct ndef_tlv *tlv)
{
	size_t off;
	uint16_t len;

	if ((it == NULL) || (tlv == NULL)) {
		return -EINVAL;
	}

	if (it->done || (it->off >= it->len)) {
		it->done = true;
		return -ENOENT;
	}

	off = it->off;
	tlv->offset = off;
	tlv->type = it->buf[off++];

	if ((tlv->type == (uint8_t)NDEF_TLV_NULL) || (tlv->type == (uint8_t)NDEF_TLV_TERMINATOR)) {
		tlv->len = 0U;
		tlv->value = NULL;
		it->off = off;
		it->done = (tlv->type == (uint8_t)NDEF_TLV_TERMINATOR);
		return 0;
	}

	if (off >= it->len) {
		it->done = true;
		return -EBADMSG;
	}

	len = it->buf[off++];
	if (len == TLV_LONG_LEN) {
		if ((it->len - off) < 2U) {
			it->done = true;
			return -EBADMSG;
		}
		len = ndef_get_be16(&it->buf[off]);
		off += 2U;
		/* 0xFFFF is reserved. */
		if (len > NDEF_TLV_LEN_MAX) {
			it->done = true;
			return -EBADMSG;
		}
	}

	if ((it->len - off) < len) {
		it->done = true;
		return -EBADMSG;
	}

	tlv->len = len;
	tlv->value = &it->buf[off];
	it->off = off + len;

	return 0;
}

int ndef_tlv_find_ndef(const uint8_t *buf, size_t len, const uint8_t **msg, size_t *msg_len)
{
	struct ndef_tlv_iter it;
	struct ndef_tlv tlv;
	int ret;

	if ((msg == NULL) || (msg_len == NULL)) {
		return -EINVAL;
	}

	ret = ndef_tlv_iter_init(&it, buf, len);
	if (ret < 0) {
		return ret;
	}

	for (ret = ndef_tlv_iter_next(&it, &tlv); ret == 0; ret = ndef_tlv_iter_next(&it, &tlv)) {
		if (tlv.type == (uint8_t)NDEF_TLV_NDEF) {
			*msg = (tlv.len > 0U) ? tlv.value : NULL;
			*msg_len = tlv.len;
			return 0;
		}
	}

	return ret;
}

int ndef_tlv_header_encode(uint8_t *buf, size_t size, uint8_t type, size_t len, size_t *out_len)
{
	size_t n;

	if ((buf == NULL) || (out_len == NULL) || (type == (uint8_t)NDEF_TLV_NULL) ||
	    (type == (uint8_t)NDEF_TLV_TERMINATOR) || (len > NDEF_TLV_LEN_MAX)) {
		return -EINVAL;
	}

	n = (len < TLV_LONG_LEN) ? 2U : 4U;
	if (size < n) {
		return -ENOSPC;
	}

	buf[0] = type;
	if (n == 2U) {
		buf[1] = (uint8_t)len;
	} else {
		buf[1] = TLV_LONG_LEN;
		ndef_put_be16(&buf[2], (uint16_t)len);
	}

	*out_len = n;

	return 0;
}

int ndef_tlv_wrap(uint8_t *buf, size_t size, const uint8_t *msg, size_t msg_len, bool terminator,
		  size_t *out_len)
{
	size_t hdr;
	size_t total;
	uint8_t tmp[4];
	int ret;

	if ((buf == NULL) || (out_len == NULL) || ((msg == NULL) && (msg_len > 0U))) {
		return -EINVAL;
	}

	ret = ndef_tlv_header_encode(tmp, sizeof(tmp), NDEF_TLV_NDEF, msg_len, &hdr);
	if (ret < 0) {
		return ret;
	}

	total = hdr + msg_len + (terminator ? 1U : 0U);
	if (size < total) {
		return -ENOSPC;
	}

	if (msg_len > 0U) {
		(void)memmove(&buf[hdr], msg, msg_len);
	}
	(void)memcpy(buf, tmp, hdr);
	if (terminator) {
		buf[hdr + msg_len] = NDEF_TLV_TERMINATOR;
	}

	*out_len = total;

	return 0;
}
