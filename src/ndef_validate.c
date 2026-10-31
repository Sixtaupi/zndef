/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <ndef/ndef.h>
#include <ndef/rtd_di.h>
#include <ndef/rtd_handover.h>
#include <ndef/rtd_sig.h>
#include <ndef/rtd_sp.h>
#include <ndef/rtd_text.h>
#include <ndef/rtd_uri.h>

#include "ndef_priv.h"

/* Unknown types and RTDs disabled in Kconfig have opaque payloads. */
static bool rtd_supported(const struct ndef_record *rec)
{
#if defined(CONFIG_ZNDEF_RTD_TEXT)
	if (ndef_record_is_wkt(rec, NDEF_RTD_TEXT)) {
		return true;
	}
#endif
#if defined(CONFIG_ZNDEF_RTD_URI)
	if (ndef_record_is_wkt(rec, NDEF_RTD_URI)) {
		return true;
	}
#endif
#if defined(CONFIG_ZNDEF_RTD_SP)
	if (ndef_record_is_wkt(rec, NDEF_RTD_SP) || ndef_record_is_wkt(rec, NDEF_RTD_SP_ACTION)) {
		return true;
	}
#endif
#if defined(CONFIG_ZNDEF_RTD_SIG)
	if (ndef_record_is_wkt(rec, NDEF_RTD_SIG)) {
		return true;
	}
#endif
#if defined(CONFIG_ZNDEF_RTD_HANDOVER)
	if (ndef_record_is_wkt(rec, NDEF_RTD_HO_CARRIER) ||
	    ndef_record_is_wkt(rec, NDEF_RTD_HO_REQUEST) ||
	    ndef_record_is_wkt(rec, NDEF_RTD_HO_SELECT) ||
	    ndef_record_is_wkt(rec, NDEF_RTD_HO_MEDIATION) ||
	    ndef_record_is_wkt(rec, NDEF_RTD_HO_INITIATE)) {
		return true;
	}
#endif
#if defined(CONFIG_ZNDEF_RTD_DI)
	if (ndef_record_is_wkt(rec, NDEF_RTD_DI)) {
		return true;
	}
#endif
	(void)rec;

	return false;
}

static int rtd_decode(const struct ndef_record *rec)
{
#if defined(CONFIG_ZNDEF_RTD_TEXT)
	if (ndef_record_is_wkt(rec, NDEF_RTD_TEXT)) {
		struct ndef_text text;

		return ndef_text_parse(rec, &text);
	}
#endif

#if defined(CONFIG_ZNDEF_RTD_URI)
	if (ndef_record_is_wkt(rec, NDEF_RTD_URI)) {
		struct ndef_uri uri;

		return ndef_uri_parse(rec, &uri);
	}
#endif

#if defined(CONFIG_ZNDEF_RTD_SP)
	if (ndef_record_is_wkt(rec, NDEF_RTD_SP)) {
		struct ndef_sp sp;

		return ndef_sp_parse(rec, &sp);
	}

	/* A lone "act" record is an error (RTD-SP 1.0 section 3.3.3). */
	if (ndef_record_is_wkt(rec, NDEF_RTD_SP_ACTION)) {
		return -EBADMSG;
	}
#endif

#if defined(CONFIG_ZNDEF_RTD_SIG)
	if (ndef_record_is_wkt(rec, NDEF_RTD_SIG)) {
		struct ndef_sig sig;

		return ndef_sig_parse(rec, &sig);
	}
#endif

#if defined(CONFIG_ZNDEF_RTD_HANDOVER)
	if (ndef_record_is_wkt(rec, NDEF_RTD_HO_CARRIER)) {
		struct ndef_ho_carrier hc;

		return ndef_ho_carrier_payload_decode(rec->payload, rec->payload_len, &hc);
	}

	{
		struct ndef_ho ho;
		int ret = ndef_ho_parse(rec, NULL, &ho);

		if (ret != -ENOMSG) {
			return ret;
		}
	}
#endif

#if defined(CONFIG_ZNDEF_RTD_DI)
	if (ndef_record_is_wkt(rec, NDEF_RTD_DI)) {
		struct ndef_di di;

		return ndef_di_parse(rec, &di);
	}
#endif

	(void)rec;

	return 0;
}

int ndef_rtd_validate(const struct ndef_record *rec, uint8_t *scratch, size_t scratch_size)
{
	struct ndef_record contiguous;
	const struct ndef_record *record_to_decode = rec;
	int ret;

	if (!rtd_supported(rec)) {
		return 0;
	}

	if (rec->chunk_count > 1U) {
		if (scratch == NULL) {
			return -ENOTSUP;
		}
		contiguous = *rec;
		ret = ndef_record_reassemble(&contiguous, scratch, scratch_size);
		if (ret < 0) {
			return ret;
		}
		record_to_decode = &contiguous;
	}

	ret = rtd_decode(record_to_decode);
	/* Unsupported versions cannot be certified by this validator. */
	if ((ret == 0) || (ret == -ENOTSUP)) {
		return ret;
	}

	return -EBADMSG;
}
