/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <ndef/rtd_handover.h>

#include "ndef_priv.h"

#define CPS_MASK 0x03U
#define CTF_MASK 0x07U

static const char *const ho_types[4] = {
	[NDEF_HO_REQUEST] = NDEF_RTD_HO_REQUEST,
	[NDEF_HO_SELECT] = NDEF_RTD_HO_SELECT,
	[NDEF_HO_MEDIATION] = NDEF_RTD_HO_MEDIATION,
	[NDEF_HO_INITIATE] = NDEF_RTD_HO_INITIATE,
};

#define HO_KIND_COUNT (sizeof(ho_types) / sizeof(ho_types[0]))

const char *ndef_ho_type(enum ndef_ho_kind kind)
{
	return ((unsigned int)kind < (HO_KIND_COUNT)) ? ho_types[kind] : NULL;
}

int ndef_ho_ac_payload_decode(const uint8_t *payload, size_t len, struct ndef_ho_ac *ac)
{
	size_t off = 0;
	size_t aux_start;
	uint8_t cps;

	if ((payload == NULL) || (ac == NULL)) {
		return -EINVAL;
	}

	/* CPS octet, carrier data reference length and at least one octet. */
	if (len < 3U) {
		return -EBADMSG;
	}

	cps = payload[off++] & CPS_MASK;
	ac->cps = (enum ndef_ho_cps)cps;
	ac->carrier_ref_len = payload[off++];
	if ((ac->carrier_ref_len == 0U) || ((len - off) < ac->carrier_ref_len)) {
		return -EBADMSG;
	}
	ac->carrier_ref = &payload[off];
	off += ac->carrier_ref_len;

	if (off >= len) {
		return -EBADMSG;
	}
	ac->aux_count = payload[off++];
	aux_start = off;
	ac->aux = &payload[off];

	for (uint8_t i = 0; i < ac->aux_count; i++) {
		uint8_t n;

		if (off >= len) {
			return -EBADMSG;
		}
		n = payload[off++];
		if ((n == 0U) || ((len - off) < n)) {
			return -EBADMSG;
		}
		off += n;
	}

	ac->aux_len = off - aux_start;

	/* Trailing octets are reserved for future use and ignored. */
	return 0;
}

int ndef_ho_ac_aux_get(const struct ndef_ho_ac *ac, uint8_t index, struct ndef_ho_ref *ref)
{
	size_t off = 0;
	uint8_t i = 0;

	if ((ac == NULL) || (ref == NULL)) {
		return -EINVAL;
	}

	if (index >= ac->aux_count) {
		return -ENOENT;
	}

	while (off < ac->aux_len) {
		uint8_t n = ac->aux[off];

		if (i == index) {
			ref->id = &ac->aux[off + 1U];
			ref->len = n;
			return 0;
		}
		off += 1U + (size_t)n;
		i++;
	}

	return -ENOENT;
}

static int err_decode(const struct ndef_record *rec, struct ndef_ho *ho)
{
	if (rec->payload_len < 1U) {
		return -EBADMSG;
	}

	ho->err_reason = rec->payload[0];

	switch (ho->err_reason) {
	case NDEF_HO_ERR_TEMP_MEMORY:
	case NDEF_HO_ERR_CARRIER:
		if (rec->payload_len != 2U) {
			return -EBADMSG;
		}
		ho->err_data = rec->payload[1];
		break;
	case NDEF_HO_ERR_PERM_MEMORY:
		if (rec->payload_len != 5U) {
			return -EBADMSG;
		}
		ho->err_data = ndef_get_be32(&rec->payload[1]);
		break;
	default:
		/* Reserved reasons: data kept opaque. */
		ho->err_data = 0U;
		break;
	}

	return 0;
}

int ndef_ho_payload_decode(enum ndef_ho_kind kind, const uint8_t *payload, size_t len,
			   struct ndef_ho *ho)
{
	struct ndef_msg_iter it;
	struct ndef_record rec;
	struct ndef_ho_ac ac;
	uint8_t minor;

	if ((payload == NULL) || (ho == NULL) || (ndef_ho_type(kind) == NULL)) {
		return -EINVAL;
	}

	(void)memset(ho, 0, sizeof(*ho));

	if (len < 1U) {
		return -EBADMSG;
	}

	ho->version = payload[0];
	if (NDEF_HO_VERSION_MAJOR(ho->version) != 1U) {
		return -ENOTSUP;
	}
	minor = NDEF_HO_VERSION_MINOR(ho->version);

	if (len > 1U) {
		int ret;

		ho->msg = &payload[1];
		ho->msg_len = len - 1U;

		(void)ndef_msg_iter_init(&it, ho->msg, ho->msg_len);

		for (ret = ndef_msg_iter_next(&it, &rec); ret == 0;
		     ret = ndef_msg_iter_next(&it, &rec)) {
			if ((rec.chunk_count > 1U) && (ndef_record_is_wkt(&rec, NDEF_RTD_HO_AC) ||
						       ndef_record_is_wkt(&rec, NDEF_RTD_HO_CR) ||
						       ndef_record_is_wkt(&rec, NDEF_RTD_HO_ERR))) {
				return -ENOTSUP;
			}

			if (ndef_record_is_wkt(&rec, NDEF_RTD_HO_AC)) {
				if (ndef_ho_ac_payload_decode(rec.payload, rec.payload_len, &ac) <
				    0) {
					return -EBADMSG;
				}
				ho->ac_count++;
			} else if (ndef_record_is_wkt(&rec, NDEF_RTD_HO_CR)) {
				if (ho->has_cr || (rec.payload_len != 2U)) {
					return -EBADMSG;
				}
				ho->has_cr = true;
				ho->cr = ndef_get_be16(rec.payload);
			} else if (ndef_record_is_wkt(&rec, NDEF_RTD_HO_ERR)) {
				if (ho->has_err || (err_decode(&rec, ho) < 0)) {
					return -EBADMSG;
				}
				ho->has_err = true;
			} else {
				/* Unknown records are ignored. */
			}
		}

		if (ret != -ENOENT) {
			return -EBADMSG;
		}
	} else if (kind != NDEF_HO_SELECT) {
		/* Only a Select, which may offer no carrier, has no nested message. */
		return -EBADMSG;
	} else {
		/* A Select may contain only its version octet. */
	}

	/* A request offers at least one carrier and, from 1.2, a random number. */
	if ((kind == NDEF_HO_REQUEST) && ((ho->ac_count == 0U) || ((minor >= 2U) && !ho->has_cr))) {
		return -EBADMSG;
	}

	return 0;
}

int ndef_ho_parse(const struct ndef_record *rec, enum ndef_ho_kind *kind, struct ndef_ho *ho)
{
	for (unsigned int k = 0; k < (HO_KIND_COUNT); k++) {
		int ret = ndef_rtd_check(rec, ho_types[k]);

		if (ret == -ENOMSG) {
			continue;
		}
		if (ret < 0) {
			return ret;
		}
		if (kind != NULL) {
			*kind = (enum ndef_ho_kind)k;
		}
		return ndef_ho_payload_decode((enum ndef_ho_kind)k, rec->payload, rec->payload_len,
					      ho);
	}

	return -ENOMSG;
}

int ndef_ho_ac_get(const struct ndef_ho *ho, uint32_t index, struct ndef_ho_ac *ac)
{
	struct ndef_msg_iter it;
	uint32_t remaining = index;
	struct ndef_record rec;
	int ret;

	if ((ho == NULL) || (ac == NULL)) {
		return -EINVAL;
	}

	if (ho->msg == NULL) {
		return -ENOENT;
	}

	(void)ndef_msg_iter_init(&it, ho->msg, ho->msg_len);

	for (ret = ndef_msg_iter_next(&it, &rec); ret == 0; ret = ndef_msg_iter_next(&it, &rec)) {
		if (ndef_record_is_wkt(&rec, NDEF_RTD_HO_AC)) {
			if (remaining == 0U) {
				return ndef_ho_ac_payload_decode(rec.payload, rec.payload_len, ac);
			}
			remaining--;
		}
	}

	return -ENOENT;
}

int ndef_ho_carrier_payload_decode(const uint8_t *payload, size_t len, struct ndef_ho_carrier *hc)
{
	uint8_t ctf;
	uint8_t type_len;

	if ((payload == NULL) || (hc == NULL)) {
		return -EINVAL;
	}

	if (len < 3U) {
		return -EBADMSG;
	}

	ctf = payload[0] & CTF_MASK;
	type_len = payload[1];

	if ((ctf < (unsigned int)NDEF_HO_CTF_WELL_KNOWN) ||
	    (ctf > (unsigned int)NDEF_HO_CTF_EXTERNAL) || (type_len == 0U) ||
	    ((len - 2U) < type_len) ||
	    (ndef_type_validate((enum ndef_tnf)ctf, &payload[2], type_len) < 0)) {
		return -EBADMSG;
	}

	hc->ctf = (enum ndef_ho_ctf)ctf;
	hc->type = &payload[2];
	hc->type_len = type_len;
	hc->data_len = len - 2U - type_len;
	hc->data = (hc->data_len > 0U) ? &payload[2U + type_len] : NULL;

	return 0;
}

int ndef_ho_begin(struct ndef_writer *w, enum ndef_ho_kind kind, uint8_t version,
		  struct ndef_writer *sub)
{
	struct ndef_record_desc desc = {.tnf = NDEF_TNF_WELL_KNOWN};
	const char *type = ndef_ho_type(kind);

	if ((type == NULL) || (NDEF_HO_VERSION_MAJOR(version) != 1U)) {
		return -EINVAL;
	}

	desc.type = type;
	desc.type_len = 2U;

	return ndef_writer_nested_begin(w, &desc, &version, 1U, sub);
}

int ndef_ho_end(struct ndef_writer *w, const struct ndef_writer *sub)
{
	struct ndef_record rec;
	struct ndef_ho ho;
	const uint8_t *payload;
	size_t payload_len;
	int ret = -EBADMSG;

	if ((w == NULL) || (sub == NULL) || (w->state != NDEF_WRITER_PAYLOAD) ||
	    (sub->buf != &w->buf[w->open_payload + 1U])) {
		return -EINVAL;
	}

	if (sub->state != NDEF_WRITER_IDLE) {
		return -EBUSY;
	}

	/* The payload is the version octet followed by the nested message. */
	payload = &w->buf[w->open_payload];
	payload_len = sub->len + 1U;

	/* ndef_ho_begin() writes a record without ID: TYPE ends the header. */
	rec.tnf = NDEF_TNF_WELL_KNOWN;
	rec.type = &w->buf[w->open_payload - 2U];
	rec.type_len = 2U;

	for (unsigned int k = 0; k < (HO_KIND_COUNT); k++) {
		if (ndef_record_is_wkt(&rec, ho_types[k])) {
			ret = ndef_ho_payload_decode((enum ndef_ho_kind)k, payload, payload_len,
						     &ho);
			break;
		}
	}

	if (ret < 0) {
		ndef_writer_record_abort(w);
		return (ret == -ENOTSUP) ? ret : -EBADMSG;
	}

	return ndef_writer_nested_end(w, sub);
}

int ndef_ho_ac_payload_encode(uint8_t *buf, size_t size, enum ndef_ho_cps cps,
			      const struct ndef_ho_ref *carrier, const struct ndef_ho_ref *aux,
			      size_t aux_count, size_t *out_len)
{
	size_t off = 0;

	if ((buf == NULL) || (out_len == NULL) || (carrier == NULL) || (carrier->id == NULL) ||
	    (carrier->len == 0U) || ((unsigned int)cps > CPS_MASK) ||
	    (aux_count > NDEF_HO_AUX_MAX) || ((aux == NULL) && (aux_count > 0U))) {
		return -EINVAL;
	}

	if (size < (3U + carrier->len)) {
		return -ENOSPC;
	}

	buf[off++] = (uint8_t)cps;
	buf[off++] = carrier->len;
	(void)memcpy(&buf[off], carrier->id, carrier->len);
	off += carrier->len;
	buf[off++] = (uint8_t)aux_count;

	for (size_t i = 0; i < aux_count; i++) {
		if ((aux[i].id == NULL) || (aux[i].len == 0U)) {
			return -EINVAL;
		}
		if ((size - off) < (1U + (size_t)aux[i].len)) {
			return -ENOSPC;
		}
		buf[off++] = aux[i].len;
		(void)memcpy(&buf[off], aux[i].id, aux[i].len);
		off += aux[i].len;
	}

	*out_len = off;

	return 0;
}

int ndef_ho_ac_write(struct ndef_writer *sub, enum ndef_ho_cps cps,
		     const struct ndef_ho_ref *carrier, const struct ndef_ho_ref *aux,
		     size_t aux_count)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_HO_AC);
	uint8_t *payload;
	size_t cap;
	size_t len;
	int ret;

	ret = ndef_writer_record_begin(sub, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_ho_ac_payload_encode(payload, cap, cps, carrier, aux, aux_count, &len);
	if (ret < 0) {
		ndef_writer_record_abort(sub);
		return ret;
	}

	return ndef_writer_record_end(sub, len);
}

int ndef_ho_cr_write(struct ndef_writer *sub, uint16_t random_number)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_HO_CR);
	uint8_t v[2];

	ndef_put_be16(v, random_number);

	return ndef_writer_record_add(sub, &desc, v, sizeof(v));
}

int ndef_ho_err_write(struct ndef_writer *sub, enum ndef_ho_err_reason reason, uint32_t data)
{
	static const struct ndef_record_desc desc =
		NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_HO_ERR);
	uint8_t v[5];
	size_t len;

	v[0] = (uint8_t)reason;

	switch (reason) {
	case NDEF_HO_ERR_TEMP_MEMORY:
	case NDEF_HO_ERR_CARRIER:
		if (data > UINT8_MAX) {
			return -EINVAL;
		}
		v[1] = (uint8_t)data;
		len = 2U;
		break;
	case NDEF_HO_ERR_PERM_MEMORY:
		ndef_put_be32(&v[1], data);
		len = 5U;
		break;
	default:
		return -EINVAL;
	}

	return ndef_writer_record_add(sub, &desc, v, len);
}

int ndef_ho_carrier_write(struct ndef_writer *w, const void *id, uint8_t id_len,
			  enum ndef_ho_ctf ctf, const void *type, size_t type_len, const void *data,
			  size_t data_len)
{
	struct ndef_record_desc desc = NDEF_RECORD_DESC(NDEF_TNF_WELL_KNOWN, NDEF_RTD_HO_CARRIER);
	uint8_t *payload;
	size_t cap;
	int ret;

	if ((id == NULL) || (id_len == 0U) || (type == NULL) || (type_len == 0U) ||
	    (type_len > UINT8_MAX) || ((data == NULL) && (data_len > 0U)) ||
	    ((unsigned int)ctf < (unsigned int)NDEF_HO_CTF_WELL_KNOWN) ||
	    ((unsigned int)ctf > (unsigned int)NDEF_HO_CTF_EXTERNAL) ||
	    (ndef_type_validate((enum ndef_tnf)ctf, type, type_len) < 0)) {
		return -EINVAL;
	}

	desc.id = id;
	desc.id_len = id_len;

	ret = ndef_writer_record_begin(w, &desc, &payload, &cap);
	if (ret < 0) {
		return ret;
	}

	if ((cap < (2U + type_len)) || ((cap - 2U - type_len) < data_len)) {
		ndef_writer_record_abort(w);
		return -ENOSPC;
	}

	payload[0] = (uint8_t)ctf;
	payload[1] = (uint8_t)type_len;
	(void)memcpy(&payload[2], type, type_len);
	if (data_len > 0U) {
		(void)memcpy(&payload[2U + type_len], data, data_len);
	}

	return ndef_writer_record_end(w, 2U + type_len + data_len);
}
