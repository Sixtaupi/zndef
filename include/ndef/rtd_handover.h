/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum Connection Handover (CH 1.x) records.
 *
 * Global types "Hr" (request), "Hs" (select), "Hm" (mediation), "Hi"
 * (initiate) and "Hc" (carrier); local types "ac" (alternative carrier),
 * "cr" (collision resolution) and "err" (error).
 *
 * Carrier configuration records (Bluetooth OOB, Wi-Fi, ...) are ordinary
 * media records written with ndef_writer_record_add(); their ID is the carrier data
 * reference of the alternative carrier record.
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_HANDOVER_H_
#define ZNDEF_INCLUDE_NDEF_RTD_HANDOVER_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_handover Connection Handover records
 * @ingroup ndef
 * @{
 */

/** Handover Request. */
#define NDEF_RTD_HO_REQUEST   "Hr"
/** Handover Select. */
#define NDEF_RTD_HO_SELECT    "Hs"
/** Handover Mediation. */
#define NDEF_RTD_HO_MEDIATION "Hm"
/** Handover Initiate. */
#define NDEF_RTD_HO_INITIATE  "Hi"
/** Handover Carrier. */
#define NDEF_RTD_HO_CARRIER   "Hc"
/** Alternative Carrier (local). */
#define NDEF_RTD_HO_AC        "ac"
/** Collision Resolution (local). */
#define NDEF_RTD_HO_CR        "cr"
/** Error (local). */
#define NDEF_RTD_HO_ERR       "err"

/** Build a version octet. */
#define NDEF_HO_VERSION(major, minor) ((uint8_t)((((major) & 0x0FU) << 4) | ((minor) & 0x0FU)))
/** Major number of a version octet. */
#define NDEF_HO_VERSION_MAJOR(v)      (((v) >> 4) & 0x0FU)
/** Minor number of a version octet. */
#define NDEF_HO_VERSION_MINOR(v)      ((v) & 0x0FU)
/** Version written by default (Connection Handover 1.5). */
#define NDEF_HO_VERSION_DEFAULT       NDEF_HO_VERSION(1, 5)

/** Largest number of auxiliary data references in an "ac" record. */
#define NDEF_HO_AUX_MAX 255U

/** @brief Handover message kind. */
enum ndef_ho_kind {
	/** Handover Request, "Hr". */
	NDEF_HO_REQUEST,
	/** Handover Select, "Hs". */
	NDEF_HO_SELECT,
	/** Handover Mediation, "Hm". */
	NDEF_HO_MEDIATION,
	/** Handover Initiate, "Hi". */
	NDEF_HO_INITIATE,
};

/** @brief Carrier Power State. */
enum ndef_ho_cps {
	/** Inactive. */
	NDEF_HO_CPS_INACTIVE = 0x00,
	/** Active. */
	NDEF_HO_CPS_ACTIVE = 0x01,
	/** Activating. */
	NDEF_HO_CPS_ACTIVATING = 0x02,
	/** Unknown. */
	NDEF_HO_CPS_UNKNOWN = 0x03,
};

/** @brief Carrier Type Format of a Handover Carrier record (same values as TNF). */
enum ndef_ho_ctf {
	/** NFC Forum well-known type. */
	NDEF_HO_CTF_WELL_KNOWN = 0x01,
	/** Media type (RFC 2046). */
	NDEF_HO_CTF_MEDIA = 0x02,
	/** Absolute URI (RFC 3986). */
	NDEF_HO_CTF_ABSOLUTE_URI = 0x03,
	/** NFC Forum external type. */
	NDEF_HO_CTF_EXTERNAL = 0x04,
};

/** @brief Error reason of an "err" record. */
enum ndef_ho_err_reason {
	/** Temporary memory constraints; data: retry delay in ms (1 octet). */
	NDEF_HO_ERR_TEMP_MEMORY = 0x01,
	/** Permanent memory constraints; data: largest NDEF message size (4 octets). */
	NDEF_HO_ERR_PERM_MEMORY = 0x02,
	/** Carrier specific constraints; data: retry delay in ms (1 octet). */
	NDEF_HO_ERR_CARRIER = 0x03,
};

/** @brief Decoded Handover Request, Select, Mediation or Initiate payload. */
struct ndef_ho {
	/** Version octet. */
	uint8_t version;
	/** Nested message of local records, NULL when empty. */
	const uint8_t *msg;
	/** Length of @ref msg, 0 when empty (allowed in Select from 1.2). */
	size_t msg_len;
	/** Number of "ac" records. */
	uint32_t ac_count;
	/** true when a "cr" record is present. */
	bool has_cr;
	/** Random number of the "cr" record. */
	uint16_t cr;
	/** true when an "err" record is present. */
	bool has_err;
	/** Reason of the "err" record. */
	uint8_t err_reason;
	/** Data of the "err" record (delay in ms or largest message size). */
	uint32_t err_data;
};

/** @brief Decoded Alternative Carrier ("ac") payload. */
struct ndef_ho_ac {
	/** Carrier power state. */
	enum ndef_ho_cps cps;
	/** Carrier data reference: ID of the carrier record. */
	const uint8_t *carrier_ref;
	/** Length of @ref carrier_ref. */
	uint8_t carrier_ref_len;
	/** Number of auxiliary data references. */
	uint8_t aux_count;
	/** @cond INTERNAL_HIDDEN */
	const uint8_t *aux;
	size_t aux_len;
	/** @endcond */
};

/** @brief Decoded Handover Carrier ("Hc") payload. */
struct ndef_ho_carrier {
	/** Carrier Type Format. */
	enum ndef_ho_ctf ctf;
	/** Carrier type. */
	const uint8_t *type;
	/** Length of @ref type. */
	uint8_t type_len;
	/** Carrier data, NULL when absent. */
	const uint8_t *data;
	/** Length of @ref data. */
	size_t data_len;
};

/** @brief Reference (carrier or auxiliary data reference). */
struct ndef_ho_ref {
	/** ID of the referenced record. */
	const void *id;
	/** Length of @ref id, 1 to 255. */
	uint8_t len;
};

/**
 * @brief Well-known type of a handover message kind.
 *
 * @param kind Kind.
 *
 * @return NUL terminated type, NULL for an invalid kind.
 */
const char *ndef_ho_type(enum ndef_ho_kind kind);

/**
 * @brief Decode a Handover Request, Select, Mediation or Initiate payload.
 *
 * The nested message and its "ac", "cr" and "err" records are checked:
 * major version 1, at least one "ac" record in a request and, from version
 * 1.2, exactly one "cr" record in a request, at most one "err" record in a
 * select. Unknown local records are ignored.
 *
 * @param kind Kind of message.
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param ho Decoded handover message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOTSUP Unsupported major version or a recognized nested record is chunked.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_ho_payload_decode(enum ndef_ho_kind kind, const uint8_t *payload, size_t len,
			   struct ndef_ho *ho);

/**
 * @brief Decode a Handover Request, Select, Mediation or Initiate record.
 *
 * @param rec Record, not chunked.
 * @param kind Set to the kind of the record. May be NULL.
 * @param ho Decoded handover message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a handover message record.
 * @retval -ENOTSUP This record or a recognized nested record is chunked,
 *         or its major version is unsupported.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_ho_parse(const struct ndef_record *rec, enum ndef_ho_kind *kind, struct ndef_ho *ho);

/**
 * @brief Get an alternative carrier of a decoded handover message.
 *
 * @param ho Decoded handover message.
 * @param index Index, below @c ac_count.
 * @param ac Decoded alternative carrier.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such alternative carrier.
 */
int ndef_ho_ac_get(const struct ndef_ho *ho, uint32_t index, struct ndef_ho_ac *ac);

/**
 * @brief Decode an Alternative Carrier payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param ac Decoded alternative carrier.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_ho_ac_payload_decode(const uint8_t *payload, size_t len, struct ndef_ho_ac *ac);

/**
 * @brief Get an auxiliary data reference of an alternative carrier.
 *
 * @param ac Decoded alternative carrier.
 * @param index Index, below @c aux_count.
 * @param ref Reference, pointing into the payload.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such reference.
 */
int ndef_ho_ac_aux_get(const struct ndef_ho_ac *ac, uint8_t index, struct ndef_ho_ref *ref);

/**
 * @brief Decode a Handover Carrier payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param hc Decoded carrier.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed payload or carrier type.
 */
int ndef_ho_carrier_payload_decode(const uint8_t *payload, size_t len, struct ndef_ho_carrier *hc);

/**
 * @brief Open a Handover Request, Select, Mediation or Initiate record.
 *
 * Add "ac", "cr" and "err" records to @p sub with ndef_ho_ac_write(),
 * ndef_ho_cr_write() and ndef_ho_err_write(), then call ndef_ho_end().
 *
 * @param w Writer of the enclosing message.
 * @param kind Kind of message.
 * @param version Version octet, see NDEF_HO_VERSION().
 * @param sub Writer of the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_ho_begin(struct ndef_writer *w, enum ndef_ho_kind kind, uint8_t version,
		  struct ndef_writer *sub);

/**
 * @brief Close a handover message record.
 *
 * The payload is checked with ndef_ho_payload_decode(); on failure the record is
 * dropped.
 *
 * @param w Writer of the enclosing message.
 * @param sub Writer of the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Invalid content (request without "ac", ...).
 * @retval -ENOTSUP A recognized nested record is chunked.
 */
int ndef_ho_end(struct ndef_writer *w, const struct ndef_writer *sub);

/**
 * @brief Encode an Alternative Carrier payload.
 *
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param cps Carrier power state.
 * @param carrier Carrier data reference.
 * @param aux Auxiliary data references, may be NULL when @p aux_count is 0.
 * @param aux_count Number of auxiliary data references.
 * @param out_len Set to the payload length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_ho_ac_payload_encode(uint8_t *buf, size_t size, enum ndef_ho_cps cps,
			      const struct ndef_ho_ref *carrier, const struct ndef_ho_ref *aux,
			      size_t aux_count, size_t *out_len);

/**
 * @brief Append an Alternative Carrier record.
 *
 * @param sub Writer of the nested handover message.
 * @param cps Carrier power state.
 * @param carrier Carrier data reference.
 * @param aux Auxiliary data references, may be NULL when @p aux_count is 0.
 * @param aux_count Number of auxiliary data references.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_ho_ac_write(struct ndef_writer *sub, enum ndef_ho_cps cps,
		     const struct ndef_ho_ref *carrier, const struct ndef_ho_ref *aux,
		     size_t aux_count);

/**
 * @brief Append a Collision Resolution record.
 *
 * @param sub Writer of the nested handover message.
 * @param random_number Random number.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_ho_cr_write(struct ndef_writer *sub, uint16_t random_number);

/**
 * @brief Append an Error record.
 *
 * @param sub Writer of the nested handover message.
 * @param reason Error reason.
 * @param data Error data: delay in ms (at most 255) for
 *             @ref NDEF_HO_ERR_TEMP_MEMORY and @ref NDEF_HO_ERR_CARRIER,
 *             largest NDEF message size for @ref NDEF_HO_ERR_PERM_MEMORY.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_ho_err_write(struct ndef_writer *sub, enum ndef_ho_err_reason reason, uint32_t data);

/**
 * @brief Append a Handover Carrier record.
 *
 * @param w Writer of the enclosing message.
 * @param id Record ID, referenced by the "ac" record.
 * @param id_len Length of @p id, 1 to 255.
 * @param ctf Carrier Type Format.
 * @param type Carrier type.
 * @param type_len Length of @p type, 1 to 255.
 * @param data Carrier data, may be NULL when @p data_len is 0.
 * @param data_len Length of @p data.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or carrier type.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_ho_carrier_write(struct ndef_writer *w, const void *id, uint8_t id_len,
			  enum ndef_ho_ctf ctf, const void *type, size_t type_len, const void *data,
			  size_t data_len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_HANDOVER_H_ */
