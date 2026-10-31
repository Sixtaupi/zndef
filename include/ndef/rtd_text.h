/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum Text Record Type Definition (RTD-Text 1.0), type "T".
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_TEXT_H_
#define ZNDEF_INCLUDE_NDEF_RTD_TEXT_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_text Text record
 * @ingroup ndef
 * @brief Well-known type "T".
 * @{
 */

/** Well-known type of a Text record. */
#define NDEF_RTD_TEXT "T"

/** Largest IANA language code length (6 bit field). */
#define NDEF_TEXT_LANG_MAX 63U

/** @brief Text encoding, bit 7 of the status octet. */
enum ndef_text_encoding {
	/** UTF-8. */
	NDEF_TEXT_UTF8 = 0,
	/** UTF-16, big endian unless a byte order mark says otherwise. */
	NDEF_TEXT_UTF16 = 1,
};

/** @brief Decoded Text record payload. Pointers reference the payload. */
struct ndef_text {
	/** Encoding of @ref text. */
	enum ndef_text_encoding encoding;
	/** IANA language code (RFC 5646), US-ASCII, not NUL terminated. */
	const char *lang;
	/** Length of @ref lang. */
	uint8_t lang_len;
	/** Text, not NUL terminated, possibly starting with a byte order mark. */
	const uint8_t *text;
	/** Length of @ref text in octets. */
	size_t text_len;
};

/**
 * @brief Decode a Text record payload.
 *
 * The reserved bit of the status octet is ignored, as the specification
 * requires. Language codes follow RFC 3066 syntax; IANA registration is
 * not checked. Unicode must be well formed and line breaks must use CRLF.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param text Decoded text.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed payload: missing or invalid language-tag syntax,
 *         invalid UTF-8/UTF-16, or an isolated CR or LF.
 */
int ndef_text_payload_decode(const uint8_t *payload, size_t len, struct ndef_text *text);

/**
 * @brief Decode a Text record.
 *
 * @param rec Record, of well-known type "T" and not chunked (see
 *            ndef_record_reassemble()).
 * @param text Decoded text.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a Text record.
 * @retval -ENOTSUP Chunked record.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_text_parse(const struct ndef_record *rec, struct ndef_text *text);

/**
 * @brief Convert the text to NUL terminated UTF-8.
 *
 * UTF-16 text is converted, honoring a byte order mark (which is dropped)
 * and surrogate pairs.
 *
 * @param text Decoded text.
 * @param dst Destination buffer.
 * @param size Size of @p dst, NUL terminator included.
 * @param out_len If not NULL, set to the string length (terminator excluded).
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or unknown encoding.
 * @retval -ENOSPC @p dst too small.
 * @retval -EBADMSG Invalid UTF-8/UTF-16 or line breaks not encoded as CRLF.
 */
int ndef_text_to_utf8(const struct ndef_text *text, char *dst, size_t size, size_t *out_len);

/**
 * @brief Encode a Text record payload.
 *
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param encoding Encoding of @p str.
 * @param lang Language code, RFC 3066 syntax ("en", "en-US"), at most
 *             63 characters.
 * @param lang_len Length of @p lang.
 * @param str Text, already in @p encoding.
 * @param str_len Length of @p str in octets.
 * @param out_len Set to the payload length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument, language-tag syntax, encoding, Unicode,
 *         or line breaks not encoded as CRLF.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_text_payload_encode(uint8_t *buf, size_t size, enum ndef_text_encoding encoding,
			     const char *lang, size_t lang_len, const void *str, size_t str_len,
			     size_t *out_len);

/**
 * @brief Append a UTF-8 Text record to a message.
 *
 * @param w Writer.
 * @param lang NUL terminated IANA language code, such as "en".
 * @param str NUL terminated UTF-8 text.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_text_write(struct ndef_writer *w, const char *lang, const char *str);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_TEXT_H_ */
