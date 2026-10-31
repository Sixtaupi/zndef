/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_sp.h>

#include "ndef_priv.h"

static bool is_icon(const struct ndef_record *rec)
{
	static const char image[] = "image/";
	static const char video[] = "video/";

	if ((rec->tnf != NDEF_TNF_MEDIA) || (rec->type_len < (sizeof(image) - 1U)) ||
	    (ndef_type_validate(NDEF_TNF_MEDIA, rec->type, rec->type_len) < 0)) {
		return false;
	}

	return ndef_casecmp_eq(rec->type, (const uint8_t *)image, sizeof(image) - 1U) ||
	       ndef_casecmp_eq(rec->type, (const uint8_t *)video, sizeof(video) - 1U);
}

static bool lang_eq(const struct ndef_text *a, const struct ndef_text *b)
{
	return (a->lang_len == b->lang_len) &&
	       ndef_casecmp_eq((const uint8_t *)a->lang, (const uint8_t *)b->lang, a->lang_len);
}

/* Check that no title before index has the language of title. */
static int title_unique(const struct ndef_sp *sp, uint32_t index, const struct ndef_text *title)
{
	struct ndef_text other;

	for (uint32_t i = 0; i < index; i++) {
		int ret = ndef_sp_title_get(sp, i, &other);

		if (ret < 0) {
			return ret;
		}
		if (lang_eq(&other, title)) {
			return -EBADMSG;
		}
	}

	return 0;
}

int ndef_sp_payload_decode(const uint8_t *payload, size_t len, struct ndef_sp *sp)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	struct ndef_text title;
	bool has_uri = false;
	int ret;

	if ((payload == NULL) || (sp == NULL)) {
		return -EINVAL;
	}

	(void)memset(sp, 0, sizeof(*sp));
	sp->msg = payload;
	sp->msg_len = len;

	(void)ndef_msg_iter_init(&it, payload, len);

	for (ret = ndef_msg_iter_next(&it, &rec); ret == 0; ret = ndef_msg_iter_next(&it, &rec)) {
		if ((rec.chunk_count > 1U) && (ndef_record_is_wkt(&rec, NDEF_RTD_URI) ||
					       ndef_record_is_wkt(&rec, NDEF_RTD_TEXT) ||
					       ndef_record_is_wkt(&rec, NDEF_RTD_SP_ACTION) ||
					       ndef_record_is_wkt(&rec, NDEF_RTD_SP_SIZE) ||
					       ndef_record_is_wkt(&rec, NDEF_RTD_SP_TYPE))) {
			/* Typed views require a contiguous nested payload. */
			return -ENOTSUP;
		}

		if (ndef_record_is_wkt(&rec, NDEF_RTD_URI)) {
			if (has_uri) {
				return -EBADMSG;
			}
			int val = ndef_uri_payload_decode(rec.payload, rec.payload_len, &sp->uri);
			if (val < 0) {
				return -EBADMSG;
			}
			has_uri = true;
		} else if (ndef_record_is_wkt(&rec, NDEF_RTD_TEXT)) {
			if (ndef_text_payload_decode(rec.payload, rec.payload_len, &title) < 0) {
				return -EBADMSG;
			}
			sp->title_count++;
			int val = title_unique(sp, sp->title_count - 1U, &title);
			if (val < 0) {
				return -EBADMSG;
			}
		} else if (ndef_record_is_wkt(&rec, NDEF_RTD_SP_ACTION)) {
			if (sp->has_action || (rec.payload_len != 1U)) {
				return -EBADMSG;
			}
			sp->has_action = true;
			sp->action = rec.payload[0];
		} else if (ndef_record_is_wkt(&rec, NDEF_RTD_SP_SIZE)) {
			if (sp->has_size || (rec.payload_len != 4U)) {
				return -EBADMSG;
			}
			sp->has_size = true;
			sp->size = ndef_get_be32(rec.payload);
		} else if (ndef_record_is_wkt(&rec, NDEF_RTD_SP_TYPE)) {
			if ((sp->type != NULL) || (ndef_type_validate(NDEF_TNF_MEDIA, rec.payload,
								      rec.payload_len) < 0)) {
				return -EBADMSG;
			}
			sp->type = (const char *)rec.payload;
			sp->type_len = rec.payload_len;
		} else if (is_icon(&rec)) {
			sp->icon_count++;
		} else {
			/* Extra records may be ignored (RTD-SP 1.0 section 3.2.1). */
		}
	}

	/* The entire payload is one nested message (RTD-SP 1.0 section 3.2.1). */
	if ((ret != -ENOENT) || (it.off != len)) {
		return -EBADMSG;
	}

	return has_uri ? 0 : -EBADMSG;
}

int ndef_sp_parse(const struct ndef_record *rec, struct ndef_sp *sp)
{
	int ret = ndef_rtd_check(rec, NDEF_RTD_SP);

	if (ret < 0) {
		return ret;
	}

	return ndef_sp_payload_decode(rec->payload, rec->payload_len, sp);
}

int ndef_sp_title_get(const struct ndef_sp *sp, uint32_t index, struct ndef_text *title)
{
	struct ndef_msg_iter it;
	uint32_t remaining = index;
	struct ndef_record rec;

	if ((sp == NULL) || (title == NULL)) {
		return -EINVAL;
	}

	(void)ndef_msg_iter_init(&it, sp->msg, sp->msg_len);

	while (ndef_msg_iter_next(&it, &rec) == 0) {
		if (ndef_record_is_wkt(&rec, NDEF_RTD_TEXT)) {
			if (remaining == 0U) {
				return ndef_text_payload_decode(rec.payload, rec.payload_len,
								title);
			}
			remaining--;
		}
	}

	return -ENOENT;
}

int ndef_sp_title_find(const struct ndef_sp *sp, const char *lang, struct ndef_text *title)
{
	struct ndef_text t;
	size_t len;
	bool partial = false;

	if ((sp == NULL) || (lang == NULL) || (title == NULL)) {
		return -EINVAL;
	}

	len = strlen(lang);

	for (uint32_t i = 0; i < sp->title_count; i++) {
		if (ndef_sp_title_get(sp, i, &t) < 0) {
			break;
		}

		if ((t.lang_len == len) &&
		    ndef_casecmp_eq((const uint8_t *)t.lang, (const uint8_t *)lang, len)) {
			*title = t;
			return 0;
		}

		if (!partial && (t.lang_len > len) && (t.lang[len] == '-') &&
		    ndef_casecmp_eq((const uint8_t *)t.lang, (const uint8_t *)lang, len)) {
			*title = t;
			partial = true;
		}
	}

	return partial ? 0 : -ENOENT;
}

int ndef_sp_icon_get(const struct ndef_sp *sp, uint32_t index, struct ndef_record *icon)
{
	struct ndef_msg_iter it;
	uint32_t remaining = index;

	if ((sp == NULL) || (icon == NULL)) {
		return -EINVAL;
	}

	(void)ndef_msg_iter_init(&it, sp->msg, sp->msg_len);

	while (ndef_msg_iter_next(&it, icon) == 0) {
		if (is_icon(icon)) {
			if (remaining == 0U) {
				return 0;
			}
			remaining--;
		}
	}

	return -ENOENT;
}

int ndef_sp_begin(struct ndef_writer *w, struct ndef_writer *sub)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_SP);

	return ndef_writer_nested_begin(w, &desc, NULL, 0U, sub);
}

int ndef_sp_end(struct ndef_writer *w, const struct ndef_writer *sub)
{
	struct ndef_sp sp;
	int ret;

	if ((w == NULL) || (sub == NULL)) {
		return -EINVAL;
	}

	ret = (sub->len == 0U) ? -EBADMSG : ndef_sp_payload_decode(sub->buf, sub->len, &sp);
	if (ret < 0) {
		ndef_writer_record_abort(w);
		return (ret == -ENOTSUP) ? ret : -EBADMSG;
	}

	return ndef_writer_nested_end(w, sub);
}

int ndef_sp_action_write(struct ndef_writer *sub, enum ndef_sp_action action)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_SP_ACTION);
	uint8_t v = (uint8_t)action;

	if ((unsigned int)action > (unsigned int)NDEF_SP_ACTION_EDIT) {
		return -EINVAL;
	}

	return ndef_writer_record_add(sub, &desc, &v, sizeof(v));
}

int ndef_sp_size_write(struct ndef_writer *sub, uint32_t size)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_SP_SIZE);
	uint8_t v[4];

	ndef_put_be32(v, size);

	return ndef_writer_record_add(sub, &desc, v, sizeof(v));
}

int ndef_sp_type_write(struct ndef_writer *sub, const char *mime)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_SP_TYPE);
	size_t len;

	if (mime == NULL) {
		return -EINVAL;
	}

	len = strlen(mime);
	if (ndef_type_validate(NDEF_TNF_MEDIA, (const uint8_t *)mime, len) < 0) {
		return -EINVAL;
	}

	return ndef_writer_record_add(sub, &desc, mime, len);
}

int ndef_sp_icon_write(struct ndef_writer *sub, const char *mime, const void *data, size_t len)
{
	struct ndef_record_desc desc = {.tnf = NDEF_TNF_MEDIA};
	struct ndef_record rec = {.tnf = NDEF_TNF_MEDIA};
	size_t type_len;

	if (mime == NULL) {
		return -EINVAL;
	}

	type_len = ndef_strnlen(mime, UINT8_MAX);
	if (type_len > UINT8_MAX) {
		return -EINVAL;
	}

	rec.type = (const uint8_t *)mime;
	rec.type_len = (uint8_t)type_len;
	if (!is_icon(&rec)) {
		return -EINVAL;
	}

	desc.type = mime;
	desc.type_len = (uint8_t)type_len;

	return ndef_writer_record_add(sub, &desc, data, len);
}
