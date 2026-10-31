/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_di.h>

#include "ndef_priv.h"

#define DI_DEFINED_MAX ((uint8_t)NDEF_DI_FIRMWARE)

static struct ndef_di_elem *defined_slot(struct ndef_di *di, uint8_t type)
{
	switch (type) {
	case NDEF_DI_MANUFACTURER:
		return &di->manufacturer;
	case NDEF_DI_MODEL:
		return &di->model;
	case NDEF_DI_UNIQUE_NAME:
		return &di->unique_name;
	case NDEF_DI_UUID:
		return &di->uuid;
	case NDEF_DI_FIRMWARE:
		return &di->firmware;
	default:
		return NULL;
	}
}

/* Check the value of a defined element. */
static bool elem_valid(const struct ndef_di_elem *e)
{
	if (e->type == (uint8_t)NDEF_DI_UUID) {
		return e->len == NDEF_DI_UUID_LEN;
	}

	if (e->type <= DI_DEFINED_MAX) {
		return ndef_utf8_valid(e->value, e->len);
	}

	return true;
}

int ndef_di_payload_decode(const uint8_t *payload, size_t len, struct ndef_di *di)
{
	struct ndef_di_elem e;
	size_t off = 0;

	if ((payload == NULL) || (di == NULL)) {
		return -EINVAL;
	}

	(void)memset(di, 0, sizeof(*di));
	di->payload = payload;
	di->len = len;

	while (off < len) {
		struct ndef_di_elem *slot;

		if ((len - off) < 2U) {
			return -EBADMSG;
		}

		e.type = payload[off];
		e.len = payload[off + 1U];
		off += 2U;

		if ((len - off) < e.len) {
			return -EBADMSG;
		}

		e.value = &payload[off];
		off += e.len;

		if (!elem_valid(&e)) {
			return -EBADMSG;
		}

		slot = defined_slot(di, e.type);
		if (slot != NULL) {
			if (slot->value != NULL) {
				return -EBADMSG;
			}
			*slot = e;
		}

		di->count++;
	}

	if ((di->manufacturer.value == NULL) || (di->model.value == NULL)) {
		return -EBADMSG;
	}

	return 0;
}

int ndef_di_parse(const struct ndef_record *rec, struct ndef_di *di)
{
	int ret = ndef_rtd_check(rec, NDEF_RTD_DI);

	if (ret < 0) {
		return ret;
	}

	return ndef_di_payload_decode(rec->payload, rec->payload_len, di);
}

int ndef_di_elem_get(const struct ndef_di *di, uint32_t index, struct ndef_di_elem *elem)
{
	size_t off = 0;

	if ((di == NULL) || (elem == NULL)) {
		return -EINVAL;
	}

	if (index >= di->count) {
		return -ENOENT;
	}

	for (uint32_t i = 0; i <= index; i++) {
		elem->type = di->payload[off];
		elem->len = di->payload[off + 1U];
		elem->value = &di->payload[off + 2U];
		off += 2U + elem->len;
	}

	return 0;
}

int ndef_di_payload_encode(const struct ndef_di_elem *elems, size_t count, uint8_t *buf,
			   size_t size, size_t *out_len)
{
	uint8_t seen = 0;
	size_t off = 0;

	if ((elems == NULL) || (buf == NULL) || (out_len == NULL)) {
		return -EINVAL;
	}

	for (size_t i = 0; i < count; i++) {
		const struct ndef_di_elem *e = &elems[i];

		if (((e->value == NULL) && (e->len > 0U)) || !elem_valid(e)) {
			return -EINVAL;
		}

		if (e->type <= DI_DEFINED_MAX) {
			if ((seen & (1U << e->type)) != 0U) {
				return -EINVAL;
			}
			seen |= (uint8_t)(1U << e->type);
		}

		if (((size - off) < 2U) || ((size - off - 2U) < e->len)) {
			return -ENOSPC;
		}

		buf[off++] = e->type;
		buf[off++] = e->len;
		if (e->len > 0U) {
			(void)memcpy(&buf[off], e->value, e->len);
			off += e->len;
		}
	}

	if ((seen &
	     ((1U << (unsigned int)NDEF_DI_MANUFACTURER) | (1U << (unsigned int)NDEF_DI_MODEL))) !=
	    ((1U << (unsigned int)NDEF_DI_MANUFACTURER) | (1U << (unsigned int)NDEF_DI_MODEL))) {
		return -EINVAL;
	}

	*out_len = off;

	return 0;
}

int ndef_di_write(struct ndef_writer *w, const struct ndef_di_elem *elems, size_t count)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_DI);
	uint8_t *payload;
	size_t cap;
	size_t len;
	int ret;

	ret = ndef_writer_record_begin(w, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_di_payload_encode(elems, count, payload, cap, &len);
	if (ret < 0) {
		ndef_writer_record_abort(w);
		return ret;
	}

	return ndef_writer_record_end(w, len);
}
