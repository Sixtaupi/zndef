/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZNDEF_SRC_NDEF_PRIV_H_
#define ZNDEF_SRC_NDEF_PRIV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ndef/ndef.h>

/* State of a struct ndef_writer. */
#define NDEF_WRITER_IDLE    0U
#define NDEF_WRITER_CHUNK   1U
#define NDEF_WRITER_PAYLOAD 2U

static inline uint16_t ndef_get_be16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static inline uint32_t ndef_get_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static inline void ndef_put_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

static inline void ndef_put_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static inline uint8_t ndef_ascii_lower(uint8_t c)
{
	return ((c >= (uint8_t)'A') && (c <= (uint8_t)'Z')) ? (uint8_t)(c + ('a' - 'A')) : c;
}

static inline bool ndef_is_alpha(uint8_t c)
{
	return ((c >= (uint8_t)'a') && (c <= (uint8_t)'z')) ||
	       ((c >= (uint8_t)'A') && (c <= (uint8_t)'Z'));
}

static inline bool ndef_is_digit(uint8_t c)
{
	return (c >= (uint8_t)'0') && (c <= (uint8_t)'9');
}

static inline bool ndef_is_hex(uint8_t c)
{
	return ndef_is_digit(c) || ((c >= (uint8_t)'a') && (c <= (uint8_t)'f')) ||
	       ((c >= (uint8_t)'A') && (c <= (uint8_t)'F'));
}

/* Case insensitive comparison of two ASCII strings of the same length. */
bool ndef_casecmp_eq(const uint8_t *a, const uint8_t *b, size_t len);

/* Length of a NUL terminated string bounded by max; returns max + 1 when longer. */
size_t ndef_strnlen(const char *s, size_t max);

/* Check that a record has a given well-known type and is not chunked. */
int ndef_rtd_check(const struct ndef_record *rec, const char *type);

/* Check a known RTD payload, using scratch to reassemble chunked records. */
int ndef_rtd_validate(const struct ndef_record *rec, uint8_t *scratch, size_t scratch_size);

#endif /* ZNDEF_SRC_NDEF_PRIV_H_ */
