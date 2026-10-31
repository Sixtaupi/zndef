/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Declarative NDEF message encoding.
 *
 * A message is described as a table of items and encoded in one call:
 *
 * @code{.c}
 * static const struct ndef_item poster[] = {
 *	NDEF_ITEM_URI("https://zephyrproject.org"),
 *	NDEF_ITEM_TEXT("en", "Zephyr Project"),
 *	NDEF_ITEM_SP_ACTION(NDEF_SP_ACTION_DO),
 * };
 *
 * static const struct ndef_item msg[] = {
 *	NDEF_ITEM_SP(poster),
 *	NDEF_ITEM_MEDIA("text/plain", "hello", 5),
 * };
 *
 * ret = ndef_items_encode(msg, NDEF_ITEMS_COUNT(msg), buf, sizeof(buf), &len);
 * @endcode
 *
 * Items are encoded with the message writer (@ref ndef_writer), so both
 * produce the same octets. Use the writer for content only known at run
 * time or for records without an item kind (chunks, Signature, Handover).
 */

#ifndef ZNDEF_INCLUDE_NDEF_ITEMS_H_
#define ZNDEF_INCLUDE_NDEF_ITEMS_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_items Declarative encoding
 * @ingroup ndef
 * @{
 */

/** @brief Kind of an item. */
enum ndef_item_kind {
	/** Any record: TNF, type, ID and payload given as is. */
	NDEF_ITEM_KIND_RAW,
	/** Media type record. */
	NDEF_ITEM_KIND_MEDIA,
	/** UTF-8 Text record. Requires @kconfig{CONFIG_ZNDEF_RTD_TEXT}. */
	NDEF_ITEM_KIND_TEXT,
	/** URI record. Requires @kconfig{CONFIG_ZNDEF_RTD_URI}. */
	NDEF_ITEM_KIND_URI,
	/** Smart Poster record. Requires @kconfig{CONFIG_ZNDEF_RTD_SP}. */
	NDEF_ITEM_KIND_SP,
	/** Smart Poster action, only inside a Smart Poster. */
	NDEF_ITEM_KIND_SP_ACTION,
};

/**
 * @brief Message item. Initialize with the NDEF_ITEM_* macros.
 *
 * Content fields have separate storage. Only the fields selected by kind
 * are used; the remaining fields are ignored. This structure contains no union.
 */
struct ndef_item {
	/** Kind of item, selects the fields used in @c u. */
	enum ndef_item_kind kind;
	struct {
		struct {
			/** Type, TNF and ID. */
			struct ndef_record_desc desc;
			/** Payload. */
			const void *payload;
			/** Length of @c payload. */
			size_t len;
		} raw; /**< @ref NDEF_ITEM_KIND_RAW */
		struct {
			/** NUL terminated media type. */
			const char *type;
			/** Payload. */
			const void *payload;
			/** Length of @c payload. */
			size_t len;
		} media; /**< @ref NDEF_ITEM_KIND_MEDIA */
		struct {
			/** NUL terminated language code. */
			const char *lang;
			/** NUL terminated UTF-8 text. */
			const char *str;
		} text; /**< @ref NDEF_ITEM_KIND_TEXT */
		struct {
			/** NUL terminated URI. */
			const char *str;
		} uri; /**< @ref NDEF_ITEM_KIND_URI */
		struct {
			/** Content of the smart poster. */
			const struct ndef_item *items;
			/** Number of entries in @c items. */
			size_t count;
		} sp; /**< @ref NDEF_ITEM_KIND_SP */
		/** @ref NDEF_ITEM_KIND_SP_ACTION, see @c enum @c ndef_sp_action. */
		uint8_t action;
	} u; /**< Content, selected by @ref kind. */
};

/**
 * @brief Number of items of an item table.
 *
 * @param _items Array of @ref ndef_item (not a pointer).
 */
#define NDEF_ITEMS_COUNT(_items) (sizeof(_items) / sizeof((_items)[0]))

/**
 * @brief Any record.
 *
 * @param _tnf Type Name Format.
 * @param _type Type, string literal.
 * @param _payload Payload.
 * @param _len Length of @p _payload.
 */
#define NDEF_ITEM_RAW(_tnf, _type, _payload, _len)                                                 \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_RAW,                                                        \
		.u.raw = {.desc = NDEF_RECORD_DESC(_tnf, _type),                                   \
			  .payload = (_payload),                                                   \
			  .len = (_len)},                                                          \
	}

/**
 * @brief External type record.
 *
 * @param _type External type ("domain:type"), string literal.
 * @param _payload Payload.
 * @param _len Length of @p _payload.
 */
#define NDEF_ITEM_EXTERNAL(_type, _payload, _len)                                                  \
	NDEF_ITEM_RAW(NDEF_TNF_EXTERNAL, _type, _payload, _len)

/**
 * @brief Media type record.
 *
 * @param _type Media type, NUL terminated.
 * @param _payload Payload.
 * @param _len Length of @p _payload.
 */
#define NDEF_ITEM_MEDIA(_type, _payload, _len)                                                     \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_MEDIA,                                                      \
		.u.media = {.type = (_type), .payload = (_payload), .len = (_len)},                \
	}

/**
 * @brief UTF-8 Text record.
 *
 * @param _lang Language code, NUL terminated.
 * @param _str UTF-8 text, NUL terminated.
 */
#define NDEF_ITEM_TEXT(_lang, _str)                                                                \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_TEXT,                                                       \
		.u.text = {.lang = (_lang), .str = (_str)},                                        \
	}

/**
 * @brief URI record; the abbreviation is chosen automatically.
 *
 * @param _str URI, NUL terminated.
 */
#define NDEF_ITEM_URI(_str)                                                                        \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_URI,                                                        \
		.u.uri = {.str = (_str)},                                                          \
	}

/**
 * @brief Smart Poster record.
 *
 * @param _items Array of @ref ndef_item holding the content: one
 *               NDEF_ITEM_URI(), and optionally NDEF_ITEM_TEXT() titles,
 *               an NDEF_ITEM_SP_ACTION() and NDEF_ITEM_MEDIA() icons.
 */
#define NDEF_ITEM_SP(_items)                                                                       \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_SP,                                                         \
		.u.sp = {.items = (_items), .count = NDEF_ITEMS_COUNT(_items)},                    \
	}

/**
 * @brief Smart Poster action record.
 *
 * @param _action Action, see @c enum @c ndef_sp_action.
 */
#define NDEF_ITEM_SP_ACTION(_action)                                                               \
	{                                                                                          \
		.kind = NDEF_ITEM_KIND_SP_ACTION,                                                  \
		.u.action = (uint8_t)(_action),                                                    \
	}

/**
 * @brief Encode a message described by a table of items.
 *
 * @param items Items, in message order.
 * @param count Number of items.
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param len Set to the encoded message length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or item (empty table, Smart Poster
 *         without URI, Smart Poster nested in a Smart Poster, action
 *         outside a Smart Poster, ...).
 * @retval -EBADMSG Invalid Smart Poster content.
 * @retval -ENOSPC @p buf too small.
 * @retval -ENOTSUP Item kind whose RTD is disabled in Kconfig.
 */
int ndef_items_encode(const struct ndef_item *items, size_t count, uint8_t *buf, size_t size,
		      size_t *len);

/**
 * @brief Append items to a message being written.
 *
 * Mixes both styles: records can be added with the writer before and
 * after the items.
 *
 * @param w Writer.
 * @param items Items, in message order.
 * @param count Number of items.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or item.
 * @retval -EBADMSG Invalid Smart Poster content.
 * @retval -ENOSPC Buffer too small; items written before the failing one
 *         are kept.
 * @retval -ENOTSUP Item kind whose RTD is disabled in Kconfig.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_items_write(struct ndef_writer *w, const struct ndef_item *items, size_t count);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_ITEMS_H_ */
