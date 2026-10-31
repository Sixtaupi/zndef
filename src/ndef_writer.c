/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/ndef.h>

#include "ndef_priv.h"

#define NO_RECORD SIZE_MAX

/* Payload length field size of a long record. */
#define LONG_LEN_SIZE 4U

static int desc_check(const struct ndef_record_desc *desc, size_t payload_len)
{
	if ((desc == NULL) || ((desc->type == NULL) && (desc->type_len > 0U)) ||
	    ((desc->id == NULL) && (desc->id_len > 0U)) || (payload_len > UINT32_MAX)) {
		return -EINVAL;
	}

	/* Unchanged is reserved to chunks, written by ndef_writer_chunk(). */
	if ((desc->tnf == NDEF_TNF_UNCHANGED) || (desc->tnf == NDEF_TNF_RESERVED)) {
		return -EINVAL;
	}

	if ((desc->tnf == NDEF_TNF_EMPTY) && ((desc->id_len > 0U) || (payload_len > 0U))) {
		return -EINVAL;
	}

	if (ndef_id_validate(desc->id, desc->id_len) < 0) {
		return -EINVAL;
	}

	return ndef_type_validate(desc->tnf, desc->type, desc->type_len);
}

static size_t header_size(const struct ndef_record_desc *desc, bool sr)
{
	return 2U + (sr ? 1U : LONG_LEN_SIZE) + ((desc->id_len > 0U) ? 1U : 0U) + desc->type_len +
	       desc->id_len;
}

/* Write the header, TYPE and ID of a record; returns the number of octets. */
static size_t header_put(uint8_t *dst, uint8_t flags, const struct ndef_record_desc *desc,
			 uint32_t payload_len, bool sr)
{
	size_t off = 0;
	uint8_t header_flags = flags;

	header_flags |= (uint8_t)desc->tnf;
	if (sr) {
		header_flags |= NDEF_HDR_SR;
	}
	if (desc->id_len > 0U) {
		header_flags |= NDEF_HDR_IL;
	}

	dst[off++] = header_flags;
	dst[off++] = desc->type_len;

	if (sr) {
		dst[off++] = (uint8_t)payload_len;
	} else {
		ndef_put_be32(&dst[off], payload_len);
		off += LONG_LEN_SIZE;
	}

	if (desc->id_len > 0U) {
		dst[off++] = desc->id_len;
	}

	if (desc->type_len > 0U) {
		(void)memcpy(&dst[off], desc->type, desc->type_len);
		off += desc->type_len;
	}

	if (desc->id_len > 0U) {
		(void)memcpy(&dst[off], desc->id, desc->id_len);
		off += desc->id_len;
	}

	return off;
}

/* Make the record starting at hdr the last one of the message. */
static void set_last(struct ndef_writer *w, size_t hdr)
{
	if (w->last_hdr != NO_RECORD) {
		w->buf[w->last_hdr] &= (uint8_t)~NDEF_HDR_ME;
	}

	w->buf[hdr] |= NDEF_HDR_ME;
	w->last_hdr = hdr;
}

static uint8_t mb_flag(const struct ndef_writer *w)
{
	return (w->len == 0U) ? NDEF_HDR_MB : 0U;
}

int ndef_writer_init(struct ndef_writer *w, uint8_t *buf, size_t size)
{
	if ((w == NULL) || ((buf == NULL) && (size > 0U))) {
		return -EINVAL;
	}

	w->buf = buf;
	w->size = size;
	w->len = 0U;
	w->last_hdr = NO_RECORD;
	w->open_hdr = 0U;
	w->open_payload = 0U;
	w->chunk_len = 0U;
	w->state = NDEF_WRITER_IDLE;

	return 0;
}

int ndef_writer_record_add(struct ndef_writer *w, const struct ndef_record_desc *desc,
			   const void *payload, size_t payload_len)
{
	bool sr = payload_len <= NDEF_SR_PAYLOAD_MAX;
	size_t hdr_len;
	size_t start;
	int ret;

	if ((w == NULL) || ((payload == NULL) && (payload_len > 0U))) {
		return -EINVAL;
	}

	if (w->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	ret = desc_check(desc, payload_len);
	if (ret < 0) {
		return ret;
	}

	hdr_len = header_size(desc, sr);
	if (((w->size - w->len) < hdr_len) || ((w->size - w->len - hdr_len) < payload_len)) {
		return -ENOSPC;
	}

	start = w->len;
	hdr_len = header_put(&w->buf[start], mb_flag(w), desc, (uint32_t)payload_len, sr);
	if (payload_len > 0U) {
		(void)memcpy(&w->buf[start + hdr_len], payload, payload_len);
	}

	w->len = start + hdr_len + payload_len;
	set_last(w, start);

	return 0;
}

int ndef_writer_record_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			     uint8_t **payload, size_t *cap)
{
	size_t hdr_len;
	size_t room;
	int ret;

	if ((w == NULL) || (payload == NULL) || (cap == NULL)) {
		return -EINVAL;
	}

	if (w->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	ret = desc_check(desc, 0U);
	if (ret < 0) {
		return ret;
	}

	if (desc->tnf == NDEF_TNF_EMPTY) {
		return -EINVAL;
	}

	/* Long format first, compacted by ndef_writer_record_end() if it fits. */
	hdr_len = header_size(desc, false);
	if ((w->size - w->len) < hdr_len) {
		return -ENOSPC;
	}

	w->open_hdr = w->len;
	hdr_len = header_put(&w->buf[w->len], mb_flag(w), desc, 0U, false);
	w->open_payload = w->open_hdr + hdr_len;
	w->state = NDEF_WRITER_PAYLOAD;

	room = w->size - w->open_payload;
	if (room > UINT32_MAX) {
		room = UINT32_MAX;
	}

	*payload = &w->buf[w->open_payload];
	*cap = room;

	return 0;
}

int ndef_writer_record_end(struct ndef_writer *w, size_t payload_len)
{
	size_t len_field;

	if ((w == NULL) || (w->state != NDEF_WRITER_PAYLOAD)) {
		return -EINVAL;
	}

	if ((payload_len > (w->size - w->open_payload)) || (payload_len > UINT32_MAX)) {
		return -EINVAL;
	}

	len_field = w->open_hdr + 2U;

	if (payload_len <= NDEF_SR_PAYLOAD_MAX) {
		size_t tail = w->open_payload + payload_len - (len_field + LONG_LEN_SIZE);

		(void)memmove(&w->buf[len_field + 1U], &w->buf[len_field + LONG_LEN_SIZE], tail);
		w->buf[len_field] = (uint8_t)payload_len;
		w->buf[w->open_hdr] |= NDEF_HDR_SR;
		w->len = len_field + 1U + tail;
	} else {
		ndef_put_be32(&w->buf[len_field], (uint32_t)payload_len);
		w->len = w->open_payload + payload_len;
	}

	w->state = NDEF_WRITER_IDLE;
	set_last(w, w->open_hdr);

	return 0;
}

void ndef_writer_record_abort(struct ndef_writer *w)
{
	if ((w != NULL) && (w->state == NDEF_WRITER_PAYLOAD)) {
		w->len = w->open_hdr;
		w->state = NDEF_WRITER_IDLE;
	}
}

int ndef_writer_nested_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			     const void *prefix, size_t prefix_len, struct ndef_writer *sub)
{
	uint8_t *payload;
	size_t cap;
	int ret;

	if ((sub == NULL) || (sub == w) || ((prefix == NULL) && (prefix_len > 0U))) {
		return -EINVAL;
	}

	ret = ndef_writer_record_begin(w, desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	if (cap < prefix_len) {
		ndef_writer_record_abort(w);
		return -ENOSPC;
	}

	if (prefix_len > 0U) {
		(void)memcpy(payload, prefix, prefix_len);
	}

	return ndef_writer_init(sub, &payload[prefix_len], cap - prefix_len);
}

int ndef_writer_nested_end(struct ndef_writer *w, const struct ndef_writer *sub)
{
	size_t prefix_len;

	if ((w == NULL) || (sub == NULL) || (w->state != NDEF_WRITER_PAYLOAD) ||
	    (w->open_payload > w->size) || (sub->size > (w->size - w->open_payload))) {
		return -EINVAL;
	}

	if (sub->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	/* The child receives all remaining capacity after the prefix. */
	prefix_len = w->size - w->open_payload - sub->size;
	if (sub->buf != &w->buf[w->open_payload + prefix_len]) {
		return -EINVAL;
	}

	return ndef_writer_record_end(w, prefix_len + sub->len);
}

int ndef_writer_chunk_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			    const void *chunk, size_t chunk_len)
{
	bool sr = chunk_len <= NDEF_SR_PAYLOAD_MAX;
	size_t hdr_len;
	size_t start;
	int ret;

	if ((w == NULL) || ((chunk == NULL) && (chunk_len > 0U))) {
		return -EINVAL;
	}

	if (w->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	ret = desc_check(desc, chunk_len);
	if (ret < 0) {
		return ret;
	}

	if (desc->tnf == NDEF_TNF_EMPTY) {
		return -EINVAL;
	}

	hdr_len = header_size(desc, sr);
	if (((w->size - w->len) < hdr_len) || ((w->size - w->len - hdr_len) < chunk_len)) {
		return -ENOSPC;
	}

	start = w->len;
	hdr_len =
		header_put(&w->buf[start], mb_flag(w) | NDEF_HDR_CF, desc, (uint32_t)chunk_len, sr);
	if (chunk_len > 0U) {
		(void)memcpy(&w->buf[start + hdr_len], chunk, chunk_len);
	}

	/* The message is incomplete until the terminating chunk sets ME. */
	if (w->last_hdr != NO_RECORD) {
		w->buf[w->last_hdr] &= (uint8_t)~NDEF_HDR_ME;
	}
	w->last_hdr = NO_RECORD;

	w->len = start + hdr_len + chunk_len;
	w->open_hdr = start;
	w->chunk_len = (uint32_t)chunk_len;
	w->state = NDEF_WRITER_CHUNK;

	return 0;
}

int ndef_writer_chunk(struct ndef_writer *w, const void *chunk, size_t chunk_len, bool last)
{
	static const struct ndef_record_desc unchanged = {.tnf = NDEF_TNF_UNCHANGED};
	bool sr = chunk_len <= NDEF_SR_PAYLOAD_MAX;
	size_t hdr_len;
	size_t start;

	if ((w == NULL) || ((chunk == NULL) && (chunk_len > 0U)) ||
	    (w->state != NDEF_WRITER_CHUNK)) {
		return -EINVAL;
	}

	if (chunk_len > (UINT32_MAX - w->chunk_len)) {
		return -EINVAL;
	}

	hdr_len = header_size(&unchanged, sr);
	if (((w->size - w->len) < hdr_len) || ((w->size - w->len - hdr_len) < chunk_len)) {
		return -ENOSPC;
	}

	start = w->len;
	hdr_len = header_put(&w->buf[start], last ? 0U : NDEF_HDR_CF, &unchanged,
			     (uint32_t)chunk_len, sr);
	if (chunk_len > 0U) {
		(void)memcpy(&w->buf[start + hdr_len], chunk, chunk_len);
	}

	w->len = start + hdr_len + chunk_len;
	w->chunk_len += (uint32_t)chunk_len;

	if (last) {
		w->state = NDEF_WRITER_IDLE;
		set_last(w, start);
	}

	return 0;
}

int ndef_writer_finish(const struct ndef_writer *w, size_t *len)
{
	if (w == NULL) {
		return -EINVAL;
	}

	if (w->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	if (w->len == 0U) {
		return -ENODATA;
	}

	if (len != NULL) {
		*len = w->len;
	}

	return 0;
}
