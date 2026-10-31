/* Copyright (c) 2026 Romain Pelletant */
/* SPDX-License-Identifier: Apache-2.0 */

#include <ndef/items.h>

static const struct ndef_item message[] = {
	NDEF_ITEM_TEXT("en", "Hello"),
	NDEF_ITEM_URI("https://zephyrproject.org"),
};

int encode_message(uint8_t *buf, size_t size, size_t *len)
{
	return ndef_items_encode(message, NDEF_ITEMS_COUNT(message), buf, size, len);
}
