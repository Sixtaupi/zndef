/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum Smart Poster Record Type Definition (RTD-SP 1.0), type "Sp".
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_SP_H_
#define ZNDEF_INCLUDE_NDEF_RTD_SP_H_

#include <ndef/ndef.h>
#include <ndef/rtd_text.h>
#include <ndef/rtd_uri.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_sp Smart Poster record
 * @ingroup ndef
 * @brief Well-known type "Sp".
 *
 * The payload is an NDEF message holding exactly one URI record, any number
 * of Text records (title, one per language), and optionally an action
 * ("act"), a size ("s"), a type ("t") and icons (image or video media
 * records). Unknown records are ignored as permitted by the specification.
 * @{
 */

/** Well-known type of a Smart Poster record. */
#define NDEF_RTD_SP        "Sp"
/** Local type of the action record. */
#define NDEF_RTD_SP_ACTION "act"
/** Local type of the size record. */
#define NDEF_RTD_SP_SIZE   "s"
/** Local type of the type record. */
#define NDEF_RTD_SP_TYPE   "t"

/** @brief Recommended action. */
enum ndef_sp_action {
	/** Do the action (open the URI, send the SMS, ...). */
	NDEF_SP_ACTION_DO = 0x00,
	/** Save for later. */
	NDEF_SP_ACTION_SAVE = 0x01,
	/** Open for editing. */
	NDEF_SP_ACTION_EDIT = 0x02,
};

/** @brief Decoded Smart Poster. Pointers reference the payload. */
struct ndef_sp {
	/** Nested message. */
	const uint8_t *msg;
	/** Length of @ref msg. */
	size_t msg_len;
	/** The URI. */
	struct ndef_uri uri;
	/** true when an action record is present. */
	bool has_action;
	/** Action; values above 0x02 are RFU and reported as is. */
	uint8_t action;
	/** true when a size record is present. */
	bool has_size;
	/** Size in octets of the referenced object. */
	uint32_t size;
	/** MIME type of the referenced object, NULL when absent. */
	const char *type;
	/** Length of @ref type. */
	size_t type_len;
	/** Number of title records. */
	uint32_t title_count;
	/** Number of icon records. */
	uint32_t icon_count;
};

/**
 * @brief Decode a Smart Poster record payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param sp Decoded smart poster.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOTSUP A recognized nested well-known record is chunked.
 * @retval -EBADMSG Malformed payload: no URI or several URIs, duplicated
 *         title language, duplicated or malformed action, size or type.
 */
int ndef_sp_payload_decode(const uint8_t *payload, size_t len, struct ndef_sp *sp);

/**
 * @brief Decode a Smart Poster record.
 *
 * @param rec Record, of well-known type "Sp" and not chunked.
 * @param sp Decoded smart poster.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a Smart Poster record.
 * @retval -ENOTSUP This record or a recognized nested well-known record is chunked.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_sp_parse(const struct ndef_record *rec, struct ndef_sp *sp);

/**
 * @brief Get a title.
 *
 * @param sp Decoded smart poster.
 * @param index Index of the title, below @c title_count.
 * @param title Decoded title.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such title.
 */
int ndef_sp_title_get(const struct ndef_sp *sp, uint32_t index, struct ndef_text *title);

/**
 * @brief Find the title in a language.
 *
 * Language codes are compared without regard to case. A title whose code
 * is a prefix followed by '-' also matches ("en" matches "en-US") when no
 * exact match exists.
 *
 * @param sp Decoded smart poster.
 * @param lang NUL terminated language code.
 * @param title Decoded title.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No title in this language.
 */
int ndef_sp_title_find(const struct ndef_sp *sp, const char *lang, struct ndef_text *title);

/**
 * @brief Get an icon record.
 *
 * @param sp Decoded smart poster.
 * @param index Index of the icon, below @c icon_count.
 * @param icon Icon record (media type "image/..." or "video/...").
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such icon.
 */
int ndef_sp_icon_get(const struct ndef_sp *sp, uint32_t index, struct ndef_record *icon);

/**
 * @brief Open a Smart Poster record.
 *
 * Add the content to @p sub with ndef_uri_write(), ndef_text_write(),
 * ndef_sp_action_write(), ndef_sp_size_write(), ndef_sp_type_write() and
 * ndef_sp_icon_write(), then call ndef_sp_end().
 *
 * @param w Writer of the enclosing message.
 * @param sub Writer of the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_sp_begin(struct ndef_writer *w, struct ndef_writer *sub);

/**
 * @brief Close a Smart Poster record.
 *
 * The nested message is checked with ndef_sp_payload_decode(); on failure the
 * record is dropped.
 *
 * @param w Writer of the enclosing message.
 * @param sub Writer of the nested message.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Invalid smart poster content (missing URI, ...).
 * @retval -ENOTSUP A recognized nested well-known record is chunked.
 */
int ndef_sp_end(struct ndef_writer *w, const struct ndef_writer *sub);

/**
 * @brief Append an action record to a Smart Poster.
 *
 * @param sub Writer of the nested message.
 * @param action Action.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_sp_action_write(struct ndef_writer *sub, enum ndef_sp_action action);

/**
 * @brief Append a size record to a Smart Poster.
 *
 * @param sub Writer of the nested message.
 * @param size Size in octets of the referenced object.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_sp_size_write(struct ndef_writer *sub, uint32_t size);

/**
 * @brief Append a type record to a Smart Poster.
 *
 * @param sub Writer of the nested message.
 * @param mime NUL terminated MIME type of the referenced object.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or MIME type.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_sp_type_write(struct ndef_writer *sub, const char *mime);

/**
 * @brief Append an icon record to a Smart Poster.
 *
 * @param sub Writer of the nested message.
 * @param mime NUL terminated media type, "image/..." or "video/...".
 * @param data Icon data.
 * @param len Length of @p data.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or media type.
 * @retval -ENOSPC Buffer too small.
 */
int ndef_sp_icon_write(struct ndef_writer *sub, const char *mime, const void *data, size_t len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_SP_H_ */
