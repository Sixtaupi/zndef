/*
 * Copyright (c) 2026 Romain Pelletant
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/ztest.h>
#include <ndef/items.h>
#include <ndef/ndef.h>

ZTEST(ndef_features, test_core_remains_available)
{
	static const struct ndef_record_desc desc = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "a/b");
	static const uint8_t expected[] = {0xD2, 0x03, 0x01, 'a', '/', 'b', 'x'};
	struct ndef_writer w;
	struct ndef_record rec;
	uint8_t buf[16];
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_writer_record_add(&w, &desc, "x", 1U));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_equal(len, sizeof(expected));
	zassert_mem_equal(buf, expected, sizeof(expected));
	zassert_ok(
		ndef_msg_validate(buf, len, NDEF_VALIDATE_TYPES | NDEF_VALIDATE_EXACT, NULL, NULL));
	zassert_ok(ndef_record_decode(buf, len, &rec));
	zassert_equal(rec.payload_len, 1U);
	zassert_equal(rec.payload[0], 'x');
}

#if defined(CONFIG_ZNDEF_ITEMS)
ZTEST(ndef_features, test_items_report_disabled_record_kinds)
{
	static const struct ndef_item items[] = {
		NDEF_ITEM_TEXT("en", "x"),
		NDEF_ITEM_URI("https://example.com"),
	};
	static const struct ndef_item poster[] = {NDEF_ITEM_SP(items)};
	uint8_t buf[128];
	size_t len;

	int text_ret = ndef_items_encode(&items[0], 1U, buf, sizeof(buf), &len);
	int uri_ret = ndef_items_encode(&items[1], 1U, buf, sizeof(buf), &len);
	int sp_ret = ndef_items_encode(poster, 1U, buf, sizeof(buf), &len);

	zassert_equal(text_ret, IS_ENABLED(CONFIG_ZNDEF_RTD_TEXT) ? 0 : -ENOTSUP);
	zassert_equal(uri_ret, IS_ENABLED(CONFIG_ZNDEF_RTD_URI) ? 0 : -ENOTSUP);
	zassert_equal(sp_ret, IS_ENABLED(CONFIG_ZNDEF_RTD_SP) ? 0 : -ENOTSUP);
}
#endif

ZTEST_SUITE(ndef_features, NULL, NULL, NULL, NULL, NULL);
