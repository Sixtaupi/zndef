/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum Device Information Record Type Definition, type "Di".
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_DI_H_
#define ZNDEF_INCLUDE_NDEF_RTD_DI_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_di Device Information record
 * @ingroup ndef
 * @brief Well-known type "Di": a list of type, length, value elements.
 * @{
 */

/** Well-known type of a Device Information record. */
#define NDEF_RTD_DI "Di"

/** Length of a UUID element. */
#define NDEF_DI_UUID_LEN 16U

/** @brief Data element types. */
enum ndef_di_type {
	/** Manufacturer name, UTF-8, mandatory. */
	NDEF_DI_MANUFACTURER = 0x00,
	/** Model name, UTF-8, mandatory. */
	NDEF_DI_MODEL = 0x01,
	/** Device unique name, UTF-8. */
	NDEF_DI_UNIQUE_NAME = 0x02,
	/** UUID, 16 octets. */
	NDEF_DI_UUID = 0x03,
	/** Firmware version, UTF-8. */
	NDEF_DI_FIRMWARE = 0x04,
};

/** @brief Data element. */
struct ndef_di_elem {
	/** Element type; values 0x05 to 0xFF are not defined and kept as is. */
	uint8_t type;
	/** Length of @ref value. */
	uint8_t len;
	/** Value. */
	const uint8_t *value;
};

/** @brief Decoded Device Information payload. */
struct ndef_di {
	/** Payload. */
	const uint8_t *payload;
	/** Length of @ref payload. */
	size_t len;
	/** Number of elements. */
	uint32_t count;
	/** Manufacturer name. */
	struct ndef_di_elem manufacturer;
	/** Model name. */
	struct ndef_di_elem model;
	/** Unique name, @c value is NULL when absent. */
	struct ndef_di_elem unique_name;
	/** UUID, @c value is NULL when absent. */
	struct ndef_di_elem uuid;
	/** Firmware version, @c value is NULL when absent. */
	struct ndef_di_elem firmware;
};

/**
 * @brief Decode a Device Information payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param di Decoded information.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed payload: truncated element, mandatory element
 *         missing, defined element repeated, bad UUID length or invalid
 *         UTF-8.
 */
int ndef_di_payload_decode(const uint8_t *payload, size_t len, struct ndef_di *di);

/**
 * @brief Decode a Device Information record.
 *
 * @param rec Record, of well-known type "Di" and not chunked.
 * @param di Decoded information.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a Device Information record.
 * @retval -ENOTSUP Chunked record.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_di_parse(const struct ndef_record *rec, struct ndef_di *di);

/**
 * @brief Get an element, defined or not.
 *
 * @param di Decoded information.
 * @param index Index, below @c count.
 * @param elem Element.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such element.
 */
int ndef_di_elem_get(const struct ndef_di *di, uint32_t index, struct ndef_di_elem *elem);

/**
 * @brief Encode a Device Information payload.
 *
 * Elements are written in the given order.
 *
 * @param elems Elements; manufacturer and model are mandatory.
 * @param count Number of elements.
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param out_len Set to the payload length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or element list.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_di_payload_encode(const struct ndef_di_elem *elems, size_t count, uint8_t *buf,
			   size_t size, size_t *out_len);

/**
 * @brief Append a Device Information record.
 *
 * @param w Writer.
 * @param elems Elements; manufacturer and model are mandatory.
 * @param count Number of elements.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or element list.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_di_write(struct ndef_writer *w, const struct ndef_di_elem *elems, size_t count);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_DI_H_ */
