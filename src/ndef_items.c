/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/items.h>
#include <ndef/rtd_sp.h>
#include <ndef/rtd_text.h>
#include <ndef/rtd_uri.h>

#include "ndef_priv.h"

static int media_write(struct ndef_writer *w, const struct ndef_item *item)
{
	struct ndef_record_desc desc = {.tnf = NDEF_TNF_MEDIA};
	size_t type_len;

	if (item->u.media.type == NULL) {
		return -EINVAL;
	}

	type_len = ndef_strnlen(item->u.media.type, UINT8_MAX);
	if (type_len > UINT8_MAX) {
		return -EINVAL;
	}

	desc.type = item->u.media.type;
	desc.type_len = (uint8_t)type_len;

	return ndef_writer_record_add(w, &desc, item->u.media.payload, item->u.media.len);
}

/* Write an item that cannot contain a nested message. */
static int item_write(struct ndef_writer *w, const struct ndef_item *item, bool in_sp)
{
	(void)in_sp;

	switch (item->kind) {
	case NDEF_ITEM_KIND_RAW:
		return ndef_writer_record_add(w, &item->u.raw.desc, item->u.raw.payload,
					      item->u.raw.len);
	case NDEF_ITEM_KIND_MEDIA:
		return media_write(w, item);
	case NDEF_ITEM_KIND_TEXT:
#if defined(CONFIG_ZNDEF_RTD_TEXT)
		return ndef_text_write(w, item->u.text.lang, item->u.text.str);
#else
		return -ENOTSUP;
#endif
	case NDEF_ITEM_KIND_URI:
#if defined(CONFIG_ZNDEF_RTD_URI)
		return ndef_uri_write(w, item->u.uri.str);
#else
		return -ENOTSUP;
#endif
	case NDEF_ITEM_KIND_SP:
#if defined(CONFIG_ZNDEF_RTD_SP)
		return -EINVAL;
#else
		return -ENOTSUP;
#endif
	case NDEF_ITEM_KIND_SP_ACTION:
#if defined(CONFIG_ZNDEF_RTD_SP)
		return in_sp ? ndef_sp_action_write(w, (enum ndef_sp_action)item->u.action)
			     : -EINVAL;
#else
		return -ENOTSUP;
#endif
	default:
		return -EINVAL;
	}
}

#if defined(CONFIG_ZNDEF_RTD_SP)
static int sp_write(struct ndef_writer *w, const struct ndef_item *item)
{
	struct ndef_writer sub;
	int ret;

	if ((item->u.sp.items == NULL) || (item->u.sp.count == 0U)) {
		return -EINVAL;
	}

	ret = ndef_sp_begin(w, &sub);
	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < item->u.sp.count; i++) {
		ret = item_write(&sub, &item->u.sp.items[i], true);
		if (ret < 0) {
			ndef_writer_record_abort(w);
			return ret;
		}
	}

	return ndef_sp_end(w, &sub);
}
#endif

int ndef_items_write(struct ndef_writer *w, const struct ndef_item *items, size_t count)
{
	int ret;

	if ((w == NULL) || ((items == NULL) && (count > 0U))) {
		return -EINVAL;
	}

	for (size_t i = 0; i < count; i++) {
#if defined(CONFIG_ZNDEF_RTD_SP)
		if (items[i].kind == NDEF_ITEM_KIND_SP) {
			ret = sp_write(w, &items[i]);
		} else {
			ret = item_write(w, &items[i], false);
		}
#else
		ret = item_write(w, &items[i], false);
#endif
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int ndef_items_encode(const struct ndef_item *items, size_t count, uint8_t *buf, size_t size,
		      size_t *len)
{
	struct ndef_writer w;
	int ret;

	if ((items == NULL) || (count == 0U) || (len == NULL)) {
		return -EINVAL;
	}

	ret = ndef_writer_init(&w, buf, size);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_items_write(&w, items, count);
	if (ret < 0) {
		return ret;
	}

	return ndef_writer_finish(&w, len);
}
