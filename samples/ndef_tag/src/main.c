/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <ndef/ndef.h>
#include <ndef/rtd_sp.h>
#include <ndef/tlv.h>

/* Data area of an NFC Forum Type 2 Tag (NTAG215 user memory). */
static uint8_t tag[504];

static int write_tag(size_t *len)
{
	struct ndef_writer w;
	struct ndef_writer sp;
	size_t msg_len;
	size_t hdr;
	int ret;

	/* Leave room for the NDEF Message TLV header, then wrap in place. */
	ret = ndef_writer_init(&w, &tag[4], sizeof(tag) - 5U);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_sp_begin(&w, &sp);
	if (ret == 0) {
		ret = ndef_uri_write(&sp, "https://zephyrproject.org");
	}
	if (ret == 0) {
		ret = ndef_text_write(&sp, "en", "Zephyr Project");
	}
	if (ret == 0) {
		ret = ndef_text_write(&sp, "fr", "Projet Zephyr");
	}
	if (ret == 0) {
		ret = ndef_sp_action_write(&sp, NDEF_SP_ACTION_DO);
	}
	if (ret == 0) {
		ret = ndef_sp_end(&w, &sp);
	}
	if (ret == 0) {
		ret = ndef_writer_finish(&w, &msg_len);
	}
	if (ret < 0) {
		return ret;
	}

	ret = ndef_tlv_wrap(tag, sizeof(tag), &tag[4], msg_len, true, &hdr);
	if (ret < 0) {
		return ret;
	}

	*len = hdr;

	return 0;
}

static int read_tag(size_t len)
{
	static const char *const langs[] = {"en", "fr"};
	struct ndef_record rec;
	struct ndef_text title;
	struct ndef_sp sp;
	const uint8_t *msg;
	size_t msg_len;
	char str[64];
	int ret;

	ret = ndef_tlv_find_ndef(tag, len, &msg, &msg_len);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_msg_validate(msg, msg_len, NDEF_VALIDATE_ALL, NULL, NULL);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_msg_find_type(msg, msg_len, NDEF_TNF_WELL_KNOWN, (const uint8_t *)NDEF_RTD_SP,
				 2U, &rec);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_sp_parse(&rec, &sp);
	if (ret < 0) {
		return ret;
	}

	ret = ndef_uri_to_string(&sp.uri, str, sizeof(str), NULL);
	if (ret < 0) {
		return ret;
	}
	printk("URI: %s\n", str);

	for (size_t i = 0; i < ARRAY_SIZE(langs); i++) {
		ret = ndef_sp_title_find(&sp, langs[i], &title);
		if (ret == 0) {
			ret = ndef_text_to_utf8(&title, str, sizeof(str), NULL);
		}
		if (ret < 0) {
			return ret;
		}
		printk("title [%s]: %s\n", langs[i], str);
	}

	return 0;
}

int main(void)
{
	size_t len;
	int ret;

	ret = write_tag(&len);
	if (ret < 0) {
		printk("write failed: %d\n", ret);
		return 0;
	}
	printk("tag area: %zu octets\n", len);

	ret = read_tag(len);
	if (ret < 0) {
		printk("read failed: %d\n", ret);
		return 0;
	}

	printk("done\n");

	return 0;
}
