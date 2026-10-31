/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Data Exchange Format (NDEF) core API.
 *
 * Implements the NFC Forum NDEF Technical Specification 1.0: record and
 * message encoding and decoding, record chunking, and the TNF and type name
 * rules.
 *
 * The library never allocates memory. Decoded records point into the
 * caller's buffer (zero copy) and encoders write into a caller supplied
 * buffer.
 */

#ifndef ZNDEF_INCLUDE_NDEF_NDEF_H_
#define ZNDEF_INCLUDE_NDEF_NDEF_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef NDEF
 * @brief NFC Data Exchange Format.
 * @{
 */

/**
 * @name Record header flags
 * @{
 */

/** Message Begin: first record of a message. */
#define NDEF_HDR_MB       0x80U
/** Message End: last record of a message. */
#define NDEF_HDR_ME       0x40U
/** Chunk Flag: first or middle chunk of a chunked payload. */
#define NDEF_HDR_CF       0x20U
/** Short Record: PAYLOAD_LENGTH is one octet. */
#define NDEF_HDR_SR       0x10U
/** ID Length present: ID_LENGTH and ID fields are present. */
#define NDEF_HDR_IL       0x08U
/** Type Name Format mask. */
#define NDEF_HDR_TNF_MASK 0x07U

/** @} */

/** Largest payload of a short record. */
#define NDEF_SR_PAYLOAD_MAX 255U

/** Size of the largest record header (flags, lengths), excluding TYPE and ID. */
#define NDEF_RECORD_HDR_MAX 7U

/**
 * @brief Encoded size of one record.
 *
 * @param type_len Length of the TYPE field.
 * @param id_len Length of the ID field, 0 when absent.
 * @param payload_len Length of the PAYLOAD field.
 */
#define NDEF_RECORD_SIZE(type_len, id_len, payload_len)                                            \
	(3U + ((payload_len) > NDEF_SR_PAYLOAD_MAX ? 3U : 0U) + ((id_len) > 0U ? 1U : 0U) +        \
	 (type_len) + (id_len) + (payload_len))

/** @brief Type Name Format (TNF) field values. */
enum ndef_tnf {
	/** Empty record: no type, ID or payload. */
	NDEF_TNF_EMPTY = 0x00,
	/** NFC Forum well-known type (NFC RTD). */
	NDEF_TNF_WELL_KNOWN = 0x01,
	/** Media type as defined in RFC 2046. */
	NDEF_TNF_MEDIA = 0x02,
	/** Absolute URI as defined in RFC 3986. */
	NDEF_TNF_ABSOLUTE_URI = 0x03,
	/** NFC Forum external type (NFC RTD). */
	NDEF_TNF_EXTERNAL = 0x04,
	/** Unknown payload type. */
	NDEF_TNF_UNKNOWN = 0x05,
	/** Unchanged: middle and terminating chunks of a chunked payload. */
	NDEF_TNF_UNCHANGED = 0x06,
	/**
	 * Reserved. Never written. Decoded records with this TNF are reported
	 * as @ref NDEF_TNF_UNKNOWN, as NDEF 1.0 recommends, and rejected by
	 * ndef_msg_validate().
	 */
	NDEF_TNF_RESERVED = 0x07,
};

/**
 * @brief Decoded NDEF record.
 *
 * All pointers reference the buffer the record was decoded from. For a
 * chunked record returned by ndef_msg_iter_next(), @ref payload points to the
 * payload of the initial chunk only and @ref payload_len is the total
 * logical payload length. Chunk headers separate the payload fragments in
 * the encoded buffer, so @c payload[0..payload_len) is not a contiguous
 * payload view. Use ndef_record_payload_read() with logical offsets to
 * iterate over the payload, or ndef_record_reassemble() to make it contiguous.
 */
struct ndef_record {
	/** Raw header octet (flags and TNF) of the first chunk. */
	uint8_t header;
	/** Type Name Format. */
	enum ndef_tnf tnf;
	/** Length of @ref type. */
	uint8_t type_len;
	/** Length of @ref id, 0 when the record has no ID. */
	uint8_t id_len;
	/** Total payload length (sum of all chunks for a chunked record). */
	uint32_t payload_len;
	/** Record type, NULL when @ref type_len is 0. */
	const uint8_t *type;
	/** Record ID, NULL when @ref id_len is 0. */
	const uint8_t *id;
	/** Payload (initial chunk only for a chunked record). */
	const uint8_t *payload;
	/** First octet of the encoded record. */
	const uint8_t *raw;
	/** Encoded length, all chunks included. */
	size_t raw_len;
	/** Number of chunks, 1 for an unchunked record. */
	uint32_t chunk_count;
};

/**
 * @brief Description of a record to encode, everything but the payload.
 */
struct ndef_record_desc {
	/** Type Name Format. */
	enum ndef_tnf tnf;
	/** Record type, may be NULL when @ref type_len is 0. */
	const void *type;
	/** Length of @ref type. */
	uint8_t type_len;
	/** Record ID, may be NULL when @ref id_len is 0. */
	const void *id;
	/** Length of @ref id, 0 for a record without ID. */
	uint8_t id_len;
};

/**
 * @brief Initialize a record description from C strings.
 *
 * @param _tnf Type Name Format.
 * @param _type NUL terminated type string literal.
 */
#define NDEF_RECORD_DESC(_tnf, _type)                                                              \
	{                                                                                          \
		.tnf = (_tnf),                                                                     \
		.type = (_type),                                                                   \
		.type_len = (uint8_t)(sizeof(_type) - 1U),                                         \
		.id = NULL,                                                                        \
		.id_len = 0U,                                                                      \
	}

/**
 * @brief Message iterator state.
 *
 * Treat as opaque; initialize with ndef_msg_iter_init().
 */
struct ndef_msg_iter {
	/** @cond INTERNAL_HIDDEN */
	const uint8_t *buf;
	size_t len;
	size_t off;
	uint32_t index;
	bool done;
	/** @endcond */
};

/**
 * @name Message validation flags
 * @{
 */

/**
 * Check the syntax of the TYPE and ID fields of each record (NFC RTD,
 * RFC 2046, RFC 3986). RTD 1.0 requires applications to ignore records
 * whose type name holds illegal characters.
 */
#define NDEF_VALIDATE_TYPES 0x01U
/** Fail when octets follow the record carrying the ME flag. */
#define NDEF_VALIDATE_EXACT 0x02U
/**
 * Check the payload of the well-known records supported by the library.
 * Requires @kconfig{CONFIG_ZNDEF_VALIDATE_RTD}. Unknown types and RTDs
 * disabled in Kconfig remain opaque. A required check that cannot be
 * performed returns -ENOTSUP; it is never silently skipped.
 */
#define NDEF_VALIDATE_RTD   0x04U
/** All checks. */
#define NDEF_VALIDATE_ALL   (NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT | NDEF_VALIDATE_RTD)

/** @} */

/**
 * @brief Decode a single record or record chunk.
 *
 * The record layout and the TNF rules applying to a single record are
 * checked. Message level rules (MB, ME, chunk sequences) are not. A record
 * with the reserved TNF is reported as @ref NDEF_TNF_UNKNOWN; the original
 * value stays in @c header.
 *
 * @param buf Encoded data.
 * @param len Length of @p buf.
 * @param rec Decoded record. Chunks are not merged: @c chunk_count is 1.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed or truncated record.
 */
int ndef_record_decode(const uint8_t *buf, size_t len, struct ndef_record *rec);

/**
 * @brief Start iterating over the records of a message.
 *
 * @param it Iterator.
 * @param buf Encoded message.
 * @param len Length of @p buf. Octets after the record carrying the ME flag
 *            are ignored.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 */
int ndef_msg_iter_init(struct ndef_msg_iter *it, const uint8_t *buf, size_t len);

/**
 * @brief Get the next logical record of a message.
 *
 * Chunks are grouped into a single logical record without copying their
 * payloads. Use ndef_record_payload_read() or ndef_record_reassemble() for
 * chunked payload access. Message level rules are checked as records are walked: MB on the first
 * record only, a chunk sequence made of an initial chunk followed by unchanged chunks, no ME inside
 * a chunk sequence, and an ME flag ending the message.
 *
 * @param it Iterator.
 * @param rec Next record.
 *
 * @retval 0 Success.
 * @retval -ENOENT No more records.
 * @retval -EBADMSG Malformed message.
 * @retval -EOVERFLOW Chunked payload larger than UINT32_MAX.
 */
int ndef_msg_iter_next(struct ndef_msg_iter *it, struct ndef_record *rec);

/**
 * @brief Index of the record last returned by ndef_msg_iter_next().
 *
 * @param it Iterator.
 *
 * @return Zero based index of the logical record.
 */
static inline uint32_t ndef_msg_iter_index(const struct ndef_msg_iter *it)
{
	return it->index - 1U;
}

/**
 * @brief Validate a whole message.
 *
 * Structural checks are always performed. With @ref NDEF_VALIDATE_RTD,
 * known RTD payloads are checked as well. A chunked RTD that needs a
 * contiguous payload returns -ENOTSUP; use ndef_msg_validate_buf() to supply
 * a reassembly buffer. Unknown types and disabled RTDs remain opaque.
 *
 * @param buf Encoded message.
 * @param len Length of @p buf.
 * @param flags Combination of NDEF_VALIDATE_* flags. Structural checks are
 *              always performed.
 * @param msg_len If not NULL, set to the encoded length on success only.
 * @param count If not NULL, set to the number of logical records on success only.
 *
 * @retval 0 Valid message.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed message or record.
 * @retval -EOVERFLOW Chunked payload larger than UINT32_MAX.
 * @retval -ENOTSUP A requested RTD check cannot be performed: validation
 *         disabled, chunked known RTD without scratch space, unsupported
 *         version, or chunked recognized record inside Smart Poster/Handover.
 */
int ndef_msg_validate(const uint8_t *buf, size_t len, uint32_t flags, size_t *msg_len,
		      uint32_t *count);

/**
 * @brief Validate a message with caller-provided RTD reassembly space.
 *
 * Performs the same checks as ndef_msg_validate(). Chunked known RTD
 * payloads are reassembled in @p scratch before validation. The buffer is
 * reused for successive records and must fit the largest such payload;
 * no memory is allocated. The encoded message is left unchanged.
 *
 * RTD payloads containing chunked recognized records inside Smart Poster
 * or Handover still return -ENOTSUP. Unknown types and disabled RTDs are
 * opaque and do not require scratch space.
 *
 * @param buf Encoded message.
 * @param len Length of @p buf.
 * @param flags Combination of NDEF_VALIDATE_* flags.
 * @param scratch Temporary buffer, disjoint from @p buf, needed only when
 *                checking a chunked known RTD. May be NULL when
 *                @p scratch_size is 0. Contents are unspecified on return.
 * @param scratch_size Capacity of @p scratch in octets.
 * @param msg_len If not NULL, set to the encoded length on success only.
 * @param count If not NULL, set to the logical record count on success only.
 *
 * @retval 0 Valid message for the requested checks.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed message or record payload.
 * @retval -EOVERFLOW Chunked payload larger than UINT32_MAX.
 * @retval -ENOSPC Supplied scratch buffer too small for a required RTD check.
 * @retval -ENOTSUP A required RTD check cannot be performed; see
 *         ndef_msg_validate().
 */
int ndef_msg_validate_buf(const uint8_t *buf, size_t len, uint32_t flags, uint8_t *scratch,
			  size_t scratch_size, size_t *msg_len, uint32_t *count);

/**
 * @brief Find the first record of a given type.
 *
 * Types are compared with ndef_record_type_eq().
 *
 * @param buf Encoded message.
 * @param len Length of @p buf.
 * @param tnf Type Name Format to look for.
 * @param type Type to look for.
 * @param type_len Length of @p type.
 * @param rec Matching record.
 *
 * @retval 0 Found.
 * @retval -ENOENT Not found.
 * @retval -EBADMSG Malformed message.
 */
int ndef_msg_find_type(const uint8_t *buf, size_t len, enum ndef_tnf tnf, const uint8_t *type,
		       size_t type_len, struct ndef_record *rec);

/**
 * @brief Find the first record with a given ID.
 *
 * IDs are URIs (RFC 3986) and are compared octet by octet.
 *
 * @param buf Encoded message.
 * @param len Length of @p buf.
 * @param id ID octets to look for.
 * @param id_len Length of @p id.
 * @param rec Matching record.
 *
 * @retval 0 Found.
 * @retval -ENOENT Not found.
 * @retval -EBADMSG Malformed message.
 */
int ndef_msg_find_id(const uint8_t *buf, size_t len, const uint8_t *id, size_t id_len,
		     struct ndef_record *rec);

/**
 * @brief Compare the type of a record.
 *
 * The comparison follows the rules of each TNF: well-known types and
 * absolute URIs are case sensitive, media types and external types are not.
 * Media type parameters (after ';') are ignored on both sides.
 *
 * @param rec Record.
 * @param tnf Type Name Format.
 * @param type Type as octets; cast string literals to const uint8_t *.
 * @param type_len Length of @p type.
 *
 * @return true when the record has this type.
 */
bool ndef_record_type_eq(const struct ndef_record *rec, enum ndef_tnf tnf, const uint8_t *type,
			 size_t type_len);

/**
 * @brief Check whether a record is a given well-known type.
 *
 * @param rec Record.
 * @param type NUL terminated RTD type name, such as "T" or "U".
 *
 * @return true when the record has this well-known type.
 */
bool ndef_record_is_wkt(const struct ndef_record *rec, const char *type);

/**
 * @brief Copy part of a record payload, chunked or not.
 *
 * @param rec Record returned by ndef_msg_iter_next() or ndef_record_decode().
 * @param offset Offset in the payload.
 * @param dst Destination octet buffer.
 * @param len Number of octets to copy.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or range outside the payload.
 * @retval -EBADMSG Malformed chunk sequence.
 */
int ndef_record_payload_read(const struct ndef_record *rec, size_t offset, uint8_t *dst,
			     size_t len);

/**
 * @brief Reassemble a chunked payload into a contiguous buffer.
 *
 * On success @p rec->payload points to @p dst. Unchunked records are left
 * untouched and @p dst is not used.
 *
 * @param rec Record to reassemble.
 * @param dst Destination buffer.
 * @param size Size of @p dst.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC @p dst too small.
 * @retval -EBADMSG Malformed chunk sequence.
 */
int ndef_record_reassemble(struct ndef_record *rec, uint8_t *dst, size_t size);

/**
 * @brief Check the syntax of a record type.
 *
 * - Well-known types: RTD 1.0 names, starting with a letter or a digit,
 *   without percent encoding.
 * - External types: "dns-part:name-part"; percent encoding is accepted in
 *   the name part.
 * - Media types: RFC 2046 "type/subtype" optionally followed by parameters.
 * - Absolute URIs: RFC 3986 absolute-URI (scheme, no fragment). Octets
 *   above 0x7F are accepted, their encoding being left to the application.
 * - Empty, unknown and unchanged: the type must be empty.
 *
 * @param tnf Type Name Format.
 * @param type Type as octets; cast string literals to const uint8_t *.
 * @param type_len Length of @p type.
 *
 * @retval 0 Valid type.
 * @retval -EINVAL Invalid type.
 */
int ndef_type_validate(enum ndef_tnf tnf, const uint8_t *type, size_t type_len);

/**
 * @brief Check the syntax of a record ID.
 *
 * The ID is an RFC 3986 URI reference, absolute or relative. Octets above
 * 0x7F are accepted. An empty ID is valid.
 *
 * @param id ID.
 * @param id_len Length of @p id.
 *
 * @retval 0 Valid ID.
 * @retval -EINVAL Invalid ID.
 */
int ndef_id_validate(const uint8_t *id, size_t id_len);

/**
 * @brief Check whether a well-known type name is local.
 *
 * Local names start with a lower case letter or a digit and are only
 * meaningful inside the record that contains them (RTD 1.0 section 3.4).
 *
 * @param type Type name.
 * @param type_len Length of @p type.
 *
 * @return true for a local type name.
 */
bool ndef_wkt_is_local(const uint8_t *type, size_t type_len);

/**
 * @brief Message writer state.
 *
 * Treat as opaque; initialize with ndef_writer_init(). The writer keeps the
 * MB and ME flags consistent at all times, so the buffer holds a valid
 * message after every successful call outside a chunk sequence or an open
 * payload.
 */
struct ndef_writer {
	/** @cond INTERNAL_HIDDEN */
	uint8_t *buf;
	size_t size;
	size_t len;
	size_t last_hdr;
	size_t open_hdr;
	size_t open_payload;
	uint32_t chunk_len;
	uint8_t state;
	/** @endcond */
};

/**
 * @brief Initialize a message writer.
 *
 * @param w Writer.
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 */
int ndef_writer_init(struct ndef_writer *w, uint8_t *buf, size_t size);

/**
 * @brief Append a record.
 *
 * The short record format is used whenever the payload fits.
 *
 * @param w Writer.
 * @param desc Type, TNF and ID of the record.
 * @param payload Payload, may be NULL when @p payload_len is 0.
 * @param payload_len Length of @p payload.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or record violating the TNF rules.
 * @retval -ENOSPC Buffer too small; the writer is unchanged.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_writer_record_add(struct ndef_writer *w, const struct ndef_record_desc *desc,
			   const void *payload, size_t payload_len);

/**
 * @brief Open a record whose payload is written in place.
 *
 * Gives access to the free space of the buffer so that a payload can be
 * built without an intermediate buffer. Close with ndef_writer_record_end().
 *
 * @param w Writer.
 * @param desc Type, TNF and ID of the record.
 * @param payload Set to the start of the payload area.
 * @param cap Set to the room available for the payload.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is already open.
 */
int ndef_writer_record_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			     uint8_t **payload, size_t *cap);

/**
 * @brief Close a record opened with ndef_writer_record_begin().
 *
 * @param w Writer.
 * @param payload_len Number of payload octets written.
 *
 * @retval 0 Success.
 * @retval -EINVAL No open payload, or @p payload_len larger than the room.
 */
int ndef_writer_record_end(struct ndef_writer *w, size_t payload_len);

/**
 * @brief Drop a record opened with ndef_writer_record_begin().
 *
 * @param w Writer.
 */
void ndef_writer_record_abort(struct ndef_writer *w);

/**
 * @brief Open a record whose payload is an NDEF message.
 *
 * Initializes @p sub over the payload area. Records added to @p sub become
 * the nested message. Close with ndef_writer_nested_end().
 *
 * @param w Writer.
 * @param desc Type, TNF and ID of the record.
 * @param prefix Octets put in the payload before the nested message, such
 *               as the version of a Handover record. May be NULL.
 * @param prefix_len Length of @p prefix.
 * @param sub Writer for the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is already open.
 */
int ndef_writer_nested_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			     const void *prefix, size_t prefix_len, struct ndef_writer *sub);

/**
 * @brief Close a record opened with ndef_writer_nested_begin().
 *
 * @param w Writer.
 * @param sub Writer of the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBUSY @p sub has an open chunk sequence or payload.
 */
int ndef_writer_nested_end(struct ndef_writer *w, const struct ndef_writer *sub);

/**
 * @brief Start a chunked record by writing its initial chunk.
 *
 * Continue with ndef_writer_chunk().
 *
 * @param w Writer.
 * @param desc Type, TNF and ID of the record. The TNF can be neither
 *             @ref NDEF_TNF_EMPTY nor @ref NDEF_TNF_UNCHANGED.
 * @param chunk Payload of the initial chunk.
 * @param chunk_len Length of @p chunk, may be 0.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small; the writer is unchanged.
 * @retval -EBUSY A chunk sequence or a payload is already open.
 */
int ndef_writer_chunk_begin(struct ndef_writer *w, const struct ndef_record_desc *desc,
			    const void *chunk, size_t chunk_len);

/**
 * @brief Append a middle or the terminating chunk.
 *
 * @param w Writer.
 * @param chunk Chunk payload.
 * @param chunk_len Length of @p chunk, may be 0.
 * @param last true for the terminating chunk.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or no chunk sequence open.
 * @retval -ENOSPC Buffer too small; the writer is unchanged.
 */
int ndef_writer_chunk(struct ndef_writer *w, const void *chunk, size_t chunk_len, bool last);

/**
 * @brief Check that a message is complete and get its encoded length.
 *
 * Checks that at least one record was written and no payload or chunk
 * sequence is open. The writer and encoded bytes are left unchanged;
 * more records may be appended after a successful call.
 *
 * @param w Writer.
 * @param len If not NULL, set to the encoded message length.
 *
 * @retval 0 Success.
 * @retval -ENODATA No record written.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_writer_finish(const struct ndef_writer *w, size_t *len);

/**
 * @brief Number of octets written so far.
 *
 * @param w Writer.
 *
 * @return Encoded length.
 */
static inline size_t ndef_writer_len(const struct ndef_writer *w)
{
	return w->len;
}

/**
 * @brief Check that a string is valid UTF-8 (RFC 3629).
 *
 * Overlong forms, surrogates and code points above U+10FFFF are rejected.
 *
 * @param s String.
 * @param len Length of @p s.
 *
 * @return true for valid UTF-8.
 */
bool ndef_utf8_valid(const uint8_t *s, size_t len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_NDEF_H_ */
