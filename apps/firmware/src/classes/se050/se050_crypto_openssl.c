/* Krypto-Unterbau fuer den Pruefbau auf Unix: OpenSSL (libcrypto).
 *
 * Warum ein dritter Unterbau: die Pruefungen der Firmware laufen in der CI
 * auf Linux mit gcc, und Windows CNG gibt es dort nicht. AES und AES-CMAC
 * werden in diesem Projekt nicht selbst geschrieben, also kommt sie hier
 * aus libcrypto - fremder, geprueffter Code, der nur aufgerufen wird.
 *
 * Drei Unterbauten, ein Kopf (se050_crypto.h):
 *   se050_crypto_psa.c     - auf dem Geraet, PSA Crypto im nRF Connect SDK
 *   se050_crypto_cng.c     - Pruefbau auf Windows, bcrypt.dll
 *   se050_crypto_openssl.c - Pruefbau auf Unix und in der CI, libcrypto
 *
 * Eigene AES- oder CMAC-Rechnung gibt es nirgends. */
#include "se050_crypto.h"

#if !defined(_WIN32) && !defined(OSK_CRYPTO_PSA)

#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>
#include <openssl/params.h>

osk_result_t osk_crypto_init(void)
{
	/* libcrypto 3 richtet sich selbst ein. */
	return OSK_OK;
}

osk_result_t osk_aes_cmac(const uint8_t key[OSK_AES_KEY_LEN], const uint8_t *msg, size_t msg_len,
			  uint8_t mac[OSK_AES_BLOCK])
{
	if (key == NULL || mac == NULL || (msg_len > 0 && msg == NULL)) {
		return OSK_ERR_ARG;
	}

	EVP_MAC *algo = EVP_MAC_fetch(NULL, "CMAC", NULL);
	if (algo == NULL) {
		return OSK_ERR_INTERNAL;
	}

	EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(algo);
	EVP_MAC_free(algo);
	if (ctx == NULL) {
		return OSK_ERR_INTERNAL;
	}

	char cipher[] = "AES-128-CBC";
	OSSL_PARAM params[] = {
	    OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, cipher, 0),
	    OSSL_PARAM_construct_end(),
	};

	osk_result_t r = OSK_ERR_INTERNAL;
	size_t out_len = 0;

	if (EVP_MAC_init(ctx, key, OSK_AES_KEY_LEN, params) != 1) {
		goto out;
	}
	if (msg_len > 0 && EVP_MAC_update(ctx, msg, msg_len) != 1) {
		goto out;
	}
	if (EVP_MAC_final(ctx, mac, &out_len, OSK_AES_BLOCK) != 1) {
		goto out;
	}
	if (out_len != OSK_AES_BLOCK) {
		goto out;
	}
	r = OSK_OK;

out:
	EVP_MAC_CTX_free(ctx);
	if (r != OSK_OK) {
		osk_wipe(mac, OSK_AES_BLOCK);
	}
	return r;
}

/* Gemeinsamer Weg fuer ECB und CBC. OpenSSL fuellt von sich aus auf; das
 * wird abgeschaltet, weil das Auffuellen nach ISO 9797-1 Verfahren 2 zur
 * Nachricht gehoert und in se050_scp03.c passiert. */
static osk_result_t run_cipher(const EVP_CIPHER *type, const uint8_t key[OSK_AES_KEY_LEN],
			       const uint8_t *iv, const uint8_t *in, uint8_t *out, size_t len,
			       int encrypt)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		return OSK_ERR_INTERNAL;
	}

	osk_result_t r = OSK_ERR_INTERNAL;
	int done = 0;
	int tail = 0;

	if (EVP_CipherInit_ex(ctx, type, NULL, key, iv, encrypt) != 1) {
		goto out;
	}
	if (EVP_CIPHER_CTX_set_padding(ctx, 0) != 1) {
		goto out;
	}
	if (EVP_CipherUpdate(ctx, out, &done, in, (int)len) != 1) {
		goto out;
	}
	if (EVP_CipherFinal_ex(ctx, out + done, &tail) != 1) {
		goto out;
	}
	if ((size_t)(done + tail) != len) {
		goto out;
	}
	r = OSK_OK;

out:
	EVP_CIPHER_CTX_free(ctx);
	if (r != OSK_OK) {
		osk_wipe(out, len);
	}
	return r;
}

osk_result_t osk_aes_ecb_encrypt_block(const uint8_t key[OSK_AES_KEY_LEN],
				       const uint8_t in[OSK_AES_BLOCK], uint8_t out[OSK_AES_BLOCK])
{
	if (key == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	return run_cipher(EVP_aes_128_ecb(), key, NULL, in, out, OSK_AES_BLOCK, 1);
}

osk_result_t osk_aes_cbc_encrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK], const uint8_t *in, uint8_t *out,
				 size_t len)
{
	if (key == NULL || iv == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (len == 0 || (len % OSK_AES_BLOCK) != 0) {
		return OSK_ERR_ARG;
	}
	return run_cipher(EVP_aes_128_cbc(), key, iv, in, out, len, 1);
}

osk_result_t osk_aes_cbc_decrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK], const uint8_t *in, uint8_t *out,
				 size_t len)
{
	if (key == NULL || iv == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (len == 0 || (len % OSK_AES_BLOCK) != 0) {
		return OSK_ERR_ARG;
	}
	return run_cipher(EVP_aes_128_cbc(), key, iv, in, out, len, 0);
}

osk_result_t osk_random(uint8_t *out, size_t len)
{
	if (out == NULL || len == 0 || len > 0x7FFFFFFF) {
		return OSK_ERR_ARG;
	}
	if (RAND_bytes(out, (int)len) != 1) {
		osk_wipe(out, len);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

#endif /* Unix und kein PSA */
