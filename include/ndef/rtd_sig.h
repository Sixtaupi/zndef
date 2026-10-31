/*
 * Copyright (c) 2026 Romain Pelletant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NFC Forum Signature Record Type Definition (RTD-Sig 1.0 and 2.0),
 *        type "Sig".
 *
 * The library encodes and decodes Signature records and identifies the
 * records a signature covers. It performs no cryptography: hashing and
 * signature checks are left to the application (PSA Crypto, mbedTLS, ...).
 */

#ifndef ZNDEF_INCLUDE_NDEF_RTD_SIG_H_
#define ZNDEF_INCLUDE_NDEF_RTD_SIG_H_

#include <ndef/ndef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ndef_rtd_sig Signature record
 * @ingroup ndef
 * @brief Well-known type "Sig".
 * @{
 */

/** Well-known type of a Signature record. */
#define NDEF_RTD_SIG "Sig"

/** Version octet of Signature RTD 1.0. */
#define NDEF_SIG_VERSION_1_0 0x01U
/** Version octet of Signature RTD 2.0. */
#define NDEF_SIG_VERSION_2_0 0x20U

/** Largest number of certificates in the chain (4 bit field). */
#define NDEF_SIG_CERT_MAX 15U

/** @brief Signature type, 7 bit field. */
enum ndef_sig_type {
	/** No signature: marks the start of a signed range. */
	NDEF_SIG_TYPE_NONE = 0x00,
	/** RSASSA-PSS, 1024 bit key. */
	NDEF_SIG_TYPE_RSASSA_PSS_1024 = 0x01,
	/** RSASSA-PKCS1-v1_5, 1024 bit key. */
	NDEF_SIG_TYPE_RSASSA_PKCS1_1024 = 0x02,
	/** DSA, 1024 bit key. */
	NDEF_SIG_TYPE_DSA_1024 = 0x03,
	/** ECDSA on curve P-192. */
	NDEF_SIG_TYPE_ECDSA_P192 = 0x04,
	/** RSASSA-PSS, 2048 bit key. */
	NDEF_SIG_TYPE_RSASSA_PSS_2048 = 0x05,
	/** RSASSA-PKCS1-v1_5, 2048 bit key. */
	NDEF_SIG_TYPE_RSASSA_PKCS1_2048 = 0x06,
	/** DSA, 2048 bit key. */
	NDEF_SIG_TYPE_DSA_2048 = 0x07,
	/** ECDSA on curve P-224. */
	NDEF_SIG_TYPE_ECDSA_P224 = 0x08,
	/** ECDSA on curve K-233. */
	NDEF_SIG_TYPE_ECDSA_K233 = 0x09,
	/** ECDSA on curve B-233. */
	NDEF_SIG_TYPE_ECDSA_B233 = 0x0A,
	/** ECDSA on curve P-256. */
	NDEF_SIG_TYPE_ECDSA_P256 = 0x0B,
};

/** @brief Hash type (Signature RTD 2.0 only). */
enum ndef_sig_hash {
	/** Not present (Signature RTD 1.0, implied by the signature type). */
	NDEF_SIG_HASH_NONE = 0x00,
	/** SHA-256. */
	NDEF_SIG_HASH_SHA256 = 0x02,
};

/** @brief Certificate format, 3 bit field. */
enum ndef_sig_cert_format {
	/** X.509. */
	NDEF_SIG_CERT_X509 = 0x00,
	/** Machine to Machine (M2M) certificate. */
	NDEF_SIG_CERT_M2M = 0x01,
};

/** @brief Octet string. */
struct ndef_sig_blob {
	/** Data. */
	const uint8_t *data;
	/** Length of @ref data, at most 65535. */
	uint16_t len;
};

/**
 * @brief Signature record content.
 *
 * Filled by ndef_sig_payload_decode(), with pointers into the payload, or by the
 * application before ndef_sig_payload_encode().
 */
struct ndef_sig {
	/** Version octet, @ref NDEF_SIG_VERSION_1_0 or @ref NDEF_SIG_VERSION_2_0. */
	uint8_t version;
	/** Signature type, see @ref ndef_sig_type. */
	uint8_t type;
	/** Hash type (2.0 only), see @ref ndef_sig_hash. */
	uint8_t hash;
	/** true when @ref signature holds a URI to the signature. */
	bool signature_is_uri;
	/** Signature, or UTF-8 URI to the signature. */
	struct ndef_sig_blob signature;
	/** Certificate format, see @ref ndef_sig_cert_format. */
	uint8_t cert_format;
	/** Number of certificates in the chain. */
	uint8_t cert_count;
	/**
	 * Certificates, used by ndef_sig_payload_encode() only (@ref cert_count
	 * entries). Use ndef_sig_cert_get() on a decoded record.
	 */
	const struct ndef_sig_blob *certs;
	/** true when @ref cert_uri is present. */
	bool has_cert_uri;
	/** UTF-8 URI of the next certificate in the chain. */
	struct ndef_sig_blob cert_uri;
	/** @cond INTERNAL_HIDDEN */
	const uint8_t *cert_store;
	size_t cert_store_len;
	/** @endcond */
};

/**
 * @brief Callback receiving the records covered by a signature.
 *
 * @param rec Signed record (chunks merged, see ndef_record_payload_read()).
 * @param user_data User data.
 *
 * @return 0 to continue, a negative value to stop and return that value.
 */
typedef int (*ndef_sig_record_cb)(const struct ndef_record *rec, void *user_data);

/**
 * @brief Callback receiving the octets to hash.
 *
 * @param data Data.
 * @param len Length of @p data.
 * @param user_data User data.
 *
 * @return 0 to continue, a negative value to stop and return that value.
 */
typedef int (*ndef_sig_data_cb)(const uint8_t *data, size_t len, void *user_data);

/**
 * @brief Decode a Signature record payload.
 *
 * @param payload Payload.
 * @param len Length of @p payload.
 * @param sig Decoded signature. @c certs is set to NULL.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed payload.
 * @retval -ENOTSUP Unknown major version.
 */
int ndef_sig_payload_decode(const uint8_t *payload, size_t len, struct ndef_sig *sig);

/**
 * @brief Decode a Signature record.
 *
 * @param rec Record, of well-known type "Sig" and not chunked.
 * @param sig Decoded signature.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOMSG Not a Signature record.
 * @retval -ENOTSUP Chunked record or unknown major version.
 * @retval -EBADMSG Malformed payload.
 */
int ndef_sig_parse(const struct ndef_record *rec, struct ndef_sig *sig);

/**
 * @brief Get a certificate of a decoded Signature record.
 *
 * @param sig Decoded signature.
 * @param index Index in the chain, below @c cert_count.
 * @param cert Certificate.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -ENOENT No such certificate.
 */
int ndef_sig_cert_get(const struct ndef_sig *sig, uint8_t index, struct ndef_sig_blob *cert);

/**
 * @brief Encode a Signature record payload.
 *
 * @param sig Content. @c certs must hold @c cert_count entries.
 * @param buf Destination buffer.
 * @param size Size of @p buf.
 * @param out_len Set to the payload length.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or content.
 * @retval -ENOSPC @p buf too small.
 */
int ndef_sig_payload_encode(const struct ndef_sig *sig, uint8_t *buf, size_t size, size_t *out_len);

/**
 * @brief Append a Signature record to a message.
 *
 * @param w Writer.
 * @param sig Content.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument or content.
 * @retval -ENOSPC Buffer too small.
 * @retval -EBUSY A chunk sequence or a payload is open.
 */
int ndef_sig_write(struct ndef_writer *w, const struct ndef_sig *sig);

/**
 * @brief Walk the records covered by a Signature record.
 *
 * A signature covers the records that precede it, back to the previous
 * Signature record or to the start of the message.
 *
 * @param msg Encoded message.
 * @param len Length of @p msg.
 * @param sig_index Index of the Signature record in the message, as given
 *                  by ndef_msg_iter_index().
 * @param cb Callback, called once per covered record.
 * @param user_data Passed to @p cb.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument, or @p sig_index is not a Signature record.
 * @retval -EBADMSG Malformed message.
 * @return Any negative value returned by @p cb.
 */
int ndef_sig_foreach_signed(const uint8_t *msg, size_t len, uint32_t sig_index,
			    ndef_sig_record_cb cb, void *user_data);

/**
 * @brief Feed the fields of a record that are protected by a signature.
 *
 * Sends the TYPE, ID and PAYLOAD fields, in this order, chunked payloads
 * being sent chunk by chunk. Header octets (flags and lengths), which change
 * when records are reordered or rechunked, are not sent.
 *
 * @param rec Record given to an @ref ndef_sig_record_cb.
 * @param cb Callback receiving the data.
 * @param user_data Passed to @p cb.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EBADMSG Malformed chunk sequence.
 * @return Any negative value returned by @p cb.
 */
int ndef_sig_record_data(const struct ndef_record *rec, ndef_sig_data_cb cb, void *user_data);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZNDEF_INCLUDE_NDEF_RTD_SIG_H_ */
