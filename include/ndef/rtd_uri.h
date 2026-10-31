/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum URI Record Type Definition (RTD-URI 1.0), type "U".
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_URI_H_
#define ZNDEF_INCLUDE_NDEF_RTD_URI_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_uri URI record
 * @ingroup ndef
 * @brief Well-known type "U".
 * @{
 */

/** Well-known type of a URI record. */
#define NDEF_RTD_URI "U"

/** Largest defined URI identifier code ("urn:nfc:"). */
#define NDEF_URI_CODE_MAX 0x23U

/** @brief Decoded URI record payload. */
struct ndef_uri {
	/** URI identifier code. */
	uint8_t code;
	/**
	 * NUL terminated abbreviation of @ref code. Empty for code 0x00 and
	 * for RFU codes, which the specification says to treat as 0x00.
	 */
	const char *prefix;
	/** URI field (UTF-8), not NUL terminated, points into the payload. */
	const char *field;
	/** Length of @ref field. */
	size_t field_len;
};

/**
 * @brief Abbreviation of a URI identifier code.
 *
 * @param code Identifier code.
 *
 * @return NUL terminated prefix, empty string for 0x00 and RFU codes.
 */
const char *ndef_uri_prefix(uint8_t code);

/**
 * @brief Decode a URI record payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param uri Decoded URI.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Empty payload, URI field not valid UTF-8 or holding a
 *         character between 0 and 31.
 */
int ndef_uri_payload_decode(const uint8_t *payload, size_t len, struct ndef_uri *uri);

/**
 * @brief Decode a URI record.
 *
 * @param rec Record, of well-known type "U" and not chunked.
 * @param uri Decoded URI.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a URI record.
 * @retval -ENOTSUP Chunked record.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_uri_parse(const struct ndef_record *rec, struct ndef_uri *uri);

/**
 * @brief Rebuild the full URI as a NUL terminated string.
 *
 * @param uri Decoded URI.
 * @param dst Destination buffer.
 * @param size Size of @p dst, NUL terminator included.
 * @param out_len If not NULL, set to the string length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC @p dst too small.
 */
int ndef_uri_to_string(const struct ndef_uri *uri, char *dst, size_t size, size_t *out_len);

/**
 * @brief Encode a URI record payload.
 *
 * The longest matching abbreviation is selected.
 *
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param str UTF-8 URI.
 * @param str_len Length of @p str.
 * @param out_len Set to the payload length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument, URI not valid UTF-8 or holding a
 *         character between 0 and 31.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_uri_payload_encode(uint8_t *buf, size_t size, const char *str, size_t str_len,
			    size_t *out_len);

/**
 * @brief Append a URI record to a message.
 *
 * @param w Writer.
 * @param str NUL terminated UTF-8 URI.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_uri_write(struct ndef_writer *w, const char *str);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_URI_H_ */
