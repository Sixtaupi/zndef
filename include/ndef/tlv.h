/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TLV blocks holding NDEF messages on NFC Forum Type 1, 2 and 5 tags.
 */

#ifndef ZNDEF_INCLUDE_NDEF_TLV_H_
#define ZNDEF_INCLUDE_NDEF_TLV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_tlv Tag TLV blocks
 * @ingroup ndef
 * @{
 */

/** Largest value length (3 octet length format). */
#define NDEF_TLV_LEN_MAX 0xFFFEU

/** @brief TLV block types. */
enum ndef_tlv_type {
	/** NULL TLV: single octet, no length. */
	NDEF_TLV_NULL = 0x00,
	/** Lock Control TLV. */
	NDEF_TLV_LOCK_CONTROL = 0x01,
	/** Memory Control TLV. */
	NDEF_TLV_MEMORY_CONTROL = 0x02,
	/** NDEF Message TLV. */
	NDEF_TLV_NDEF = 0x03,
	/** Proprietary TLV. */
	NDEF_TLV_PROPRIETARY = 0xFD,
	/** Terminator TLV: single octet, no length. */
	NDEF_TLV_TERMINATOR = 0xFE,
};

/** @brief Decoded TLV block. */
struct ndef_tlv {
	/** Type. */
	uint8_t type;
	/** Length of @ref value. */
	uint16_t len;
	/** Value, NULL for NULL and Terminator TLVs. */
	const uint8_t *value;
	/** Offset of the block in the area. */
	size_t offset;
};

/** @brief TLV iterator. Treat as opaque; initialize with ndef_tlv_iter_init(). */
struct ndef_tlv_iter {
	/** @cond INTERNAL_HIDDEN */
	const uint8_t *buf;
	size_t len;
	size_t off;
	bool done;
	/** @endcond */
};

/**
 * @brief Encoded size of a TLV block.
 *
 * @param len Length of the value.
 */
#define NDEF_TLV_SIZE(len) (((len) < 0xFFU ? 2U : 4U) + (len))

/**
 * @brief Start iterating over the TLV blocks of a tag data area.
 *
 * @param it Iterator.
 * @param buf Data area.
 * @param len Length of @p buf.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 */
int ndef_tlv_iter_init(struct ndef_tlv_iter *it, const uint8_t *buf, size_t len);

/**
 * @brief Get the next TLV block.
 *
 * NULL TLVs are returned. Iteration ends after a Terminator TLV or at the
 * end of the area.
 *
 * @param it Iterator.
 * @param tlv Next block.
 *
 * @retval 0 Success.
 * @retval -ENOENT No more blocks.
 * @retval -EBADMSG Truncated block or invalid length.
 */
int ndef_tlv_iter_next(struct ndef_tlv_iter *it, struct ndef_tlv *tlv);

/**
 * @brief Find the first NDEF Message TLV of a data area.
 *
 * @param buf Data area.
 * @param len Length of @p buf.
 * @param msg Set to the message, NULL for an empty NDEF Message TLV.
 * @param msg_len Set to the message length, 0 when the tag holds no message.
 *
 * @retval 0 Found.
 * @retval -ENOENT No NDEF Message TLV.
 * @retval -EBADMSG Malformed area.
 */
int ndef_tlv_find_ndef(const uint8_t *buf, size_t len, const uint8_t **msg, size_t *msg_len);

/**
 * @brief Encode the type and length of a TLV block.
 *
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param type Type, neither NULL nor Terminator.
 * @param len Length of the value, at most @ref NDEF_TLV_LEN_MAX.
 * @param out_len Set to the header length (2 or 4).
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_tlv_header_encode(uint8_t *buf, size_t size, uint8_t type, size_t len, size_t *out_len);

/**
 * @brief Wrap an NDEF message into an NDEF Message TLV and a Terminator TLV.
 *
 * @p msg may overlap @p buf; the message is moved as needed.
 *
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param msg Encoded message, may be NULL when @p msg_len is 0.
 * @param msg_len Length of @p msg.
 * @param terminator Append a Terminator TLV.
 * @param out_len Set to the encoded length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_tlv_wrap(uint8_t *buf, size_t size, const uint8_t *msg, size_t msg_len, bool terminator,
		  size_t *out_len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_TLV_H_ */
