/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <ndef/items.h>
#include <ndef/rtd_sp.h>

static uint8_t buf[256];
static uint8_t ref[256];

static const uint8_t icon[] = {0x89, 'P', 'N', 'G'};

static const struct ndef_item poster[] = {
	NDEF_ITEM_URI("https://zephyrproject.org"),
	NDEF_ITEM_TEXT("en", "Zephyr Project"),
	NDEF_ITEM_TEXT("fr", "Projet Zephyr"),
	NDEF_ITEM_SP_ACTION(NDEF_SP_ACTION_SAVE),
	NDEF_ITEM_MEDIA("image/png", icon, sizeof(icon)),
};

static const struct ndef_item msg[] = {
	NDEF_ITEM_SP(poster),
	NDEF_ITEM_MEDIA("text/plain", "hello", 5U),
	NDEF_ITEM_EXTERNAL("example.com:id", "\x01\x02", 2U),
	NDEF_ITEM_RAW(NDEF_TNF_EMPTY, "", NULL, 0U),
};

ZTEST(ndef_items, test_same_octets_as_writer)
{
	static const struct ndef_record_desc plain = NDEF_RECORD_DESC(NDEF_TNF_MEDIA, "text/plain");
	static const struct ndef_record_desc ext =
		NDEF_RECORD_DESC(NDEF_TNF_EXTERNAL, "example.com:id");
	static const struct ndef_record_desc empty = {.tnf = NDEF_TNF_EMPTY};
	struct ndef_writer w;
	struct ndef_writer sp;
	size_t ref_len;
	size_t len;

	zassert_ok(ndef_writer_init(&w, ref, sizeof(ref)));
	zassert_ok(ndef_sp_begin(&w, &sp));
	zassert_ok(ndef_uri_write(&sp, "https://zephyrproject.org"));
	zassert_ok(ndef_text_write(&sp, "en", "Zephyr Project"));
	zassert_ok(ndef_text_write(&sp, "fr", "Projet Zephyr"));
	zassert_ok(ndef_sp_action_write(&sp, NDEF_SP_ACTION_SAVE));
	zassert_ok(ndef_sp_icon_write(&sp, "image/png", icon, sizeof(icon)));
	zassert_ok(ndef_sp_end(&w, &sp));
	zassert_ok(ndef_writer_record_add(&w, &plain, "hello", 5U));
	zassert_ok(ndef_writer_record_add(&w, &ext, "\x01\x02", 2U));
	zassert_ok(ndef_writer_record_add(&w, &empty, NULL, 0U));
	zassert_ok(ndef_writer_finish(&w, &ref_len));

	zassert_ok(ndef_items_encode(msg, NDEF_ITEMS_COUNT(msg), buf, sizeof(buf), &len));
	zassert_equal(len, ref_len);
	zassert_mem_equal(buf, ref, len);
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, NULL));
}

ZTEST(ndef_items, test_mixed_with_writer)
{
	static const struct ndef_item tail[] = {
		NDEF_ITEM_URI("tel:+33123456789"),
	};
	struct ndef_writer w;
	uint32_t count;
	size_t len;

	zassert_ok(ndef_writer_init(&w, buf, sizeof(buf)));
	zassert_ok(ndef_text_write(&w, "en", "first"));
	zassert_ok(ndef_items_write(&w, tail, NDEF_ITEMS_COUNT(tail)));
	zassert_ok(ndef_writer_finish(&w, &len));
	zassert_ok(ndef_msg_validate(buf, len, NDEF_VALIDATE_ALL, NULL, &count));
	zassert_equal(count, 2U);
}

ZTEST(ndef_items, test_errors)
{
	static const struct ndef_item no_uri[] = {
		NDEF_ITEM_TEXT("en", "x"),
	};
	static const struct ndef_item bad_sp[] = {
		NDEF_ITEM_SP(no_uri),
	};
	static const struct ndef_item lone_action[] = {
		NDEF_ITEM_SP_ACTION(NDEF_SP_ACTION_DO),
	};
	static const struct ndef_item nested_sp[] = {
		NDEF_ITEM_URI("a:b"),
		NDEF_ITEM_SP(poster),
	};
	static const struct ndef_item sp_in_sp[] = {
		NDEF_ITEM_SP(nested_sp),
	};
	static const struct ndef_item bad_media[] = {
		NDEF_ITEM_MEDIA("text", "x", 1U),
	};
	static const struct ndef_item bad_lang[] = {
		NDEF_ITEM_TEXT("en_US", "x"),
	};
	size_t len;

	zassert_equal(ndef_items_encode(bad_sp, 1U, buf, sizeof(buf), &len), -EBADMSG);
	zassert_equal(ndef_items_encode(lone_action, 1U, buf, sizeof(buf), &len), -EINVAL);
	zassert_equal(ndef_items_encode(sp_in_sp, 1U, buf, sizeof(buf), &len), -EINVAL);
	zassert_equal(ndef_items_encode(bad_media, 1U, buf, sizeof(buf), &len), -EINVAL);
	zassert_equal(ndef_items_encode(bad_lang, 1U, buf, sizeof(buf), &len), -EINVAL);
	zassert_equal(ndef_items_encode(msg, 0U, buf, sizeof(buf), &len), -EINVAL);
	zassert_equal(ndef_items_encode(msg, NDEF_ITEMS_COUNT(msg), buf, 20U, &len), -ENOSPC);
}

ZTEST_SUITE(ndef_items, NULL, NULL, NULL, NULL, NULL);
