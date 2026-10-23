/* Krypto-Unterbau fuer den Pruefbau am PC: Windows CNG (bcrypt.dll).
 *
 * Warum nicht die eigene Rechnung: AES und CMAC werden in diesem Projekt
 * nicht selbst geschrieben. Windows bringt beides mit - AES-CMAC als
 * BCRYPT_AES_CMAC_ALGORITHM, AES-ECB und AES-CBC als Betriebsarten des
 * AES-Anbieters. Damit laufen die Tests ohne eine einzige Fremdbibliothek
 * im Repo und ohne selbstgebaute Krypto.
 *
 * Auf dem Geraet laeuft statt dessen se050_crypto_psa.c. */
#include "se050_crypto.h"

#if defined(_WIN32) && !defined(OSK_CRYPTO_PSA)

#include <windows.h>
#include <bcrypt.h>
#include <string.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

osk_result_t osk_crypto_init(void)
{
	/* CNG braucht keine Vorbereitung; die Anbieter werden je Aufruf
	 * geoeffnet und wieder geschlossen. */
	return OSK_OK;
}

/* Einen Anbieter oeffnen und die Betriebsart setzen. */
static osk_result_t open_aes(BCRYPT_ALG_HANDLE *alg, LPCWSTR chain_mode)
{
	NTSTATUS st = BCryptOpenAlgorithmProvider(alg, BCRYPT_AES_ALGORITHM,
						  NULL, 0);
	if (st != STATUS_SUCCESS) {
		return OSK_ERR_INTERNAL;
	}
	st = BCryptSetProperty(*alg, BCRYPT_CHAINING_MODE,
			       (PUCHAR)chain_mode,
			       (ULONG)((wcslen(chain_mode) + 1) * sizeof(wchar_t)),
			       0);
	if (st != STATUS_SUCCESS) {
		BCryptCloseAlgorithmProvider(*alg, 0);
		*alg = NULL;
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

osk_result_t osk_aes_cmac(const uint8_t key[OSK_AES_KEY_LEN],
			  const uint8_t *msg, size_t msg_len,
			  uint8_t mac[OSK_AES_BLOCK])
{
	if (key == NULL || mac == NULL || (msg_len > 0 && msg == NULL)) {
		return OSK_ERR_ARG;
	}
	if (msg_len > 0xFFFFFFFFu) {
		return OSK_ERR_ARG;
	}

	BCRYPT_ALG_HANDLE alg = NULL;
	NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_CMAC_ALGORITHM,
						  NULL, 0);
	if (st != STATUS_SUCCESS) {
		return OSK_ERR_INTERNAL;
	}

	BCRYPT_HASH_HANDLE h = NULL;
	osk_result_t r = OSK_ERR_INTERNAL;

	st = BCryptCreateHash(alg, &h, NULL, 0, (PUCHAR)key, OSK_AES_KEY_LEN, 0);
	if (st != STATUS_SUCCESS) {
		goto out;
	}
	if (msg_len > 0) {
		st = BCryptHashData(h, (PUCHAR)msg, (ULONG)msg_len, 0);
		if (st != STATUS_SUCCESS) {
			goto out;
		}
	}
	st = BCryptFinishHash(h, (PUCHAR)mac, OSK_AES_BLOCK, 0);
	if (st != STATUS_SUCCESS) {
		goto out;
	}
	r = OSK_OK;

out:
	if (h != NULL) {
		BCryptDestroyHash(h);
	}
	BCryptCloseAlgorithmProvider(alg, 0);
	if (r != OSK_OK) {
		osk_wipe(mac, OSK_AES_BLOCK);
	}
	return r;
}

osk_result_t osk_aes_ecb_encrypt_block(const uint8_t key[OSK_AES_KEY_LEN],
				       const uint8_t in[OSK_AES_BLOCK],
				       uint8_t out[OSK_AES_BLOCK])
{
	if (key == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}

	BCRYPT_ALG_HANDLE alg = NULL;
	osk_result_t r = open_aes(&alg, BCRYPT_CHAIN_MODE_ECB);
	if (r != OSK_OK) {
		return r;
	}

	BCRYPT_KEY_HANDLE k = NULL;
	r = OSK_ERR_INTERNAL;

	NTSTATUS st = BCryptGenerateSymmetricKey(alg, &k, NULL, 0,
						 (PUCHAR)key, OSK_AES_KEY_LEN, 0);
	if (st != STATUS_SUCCESS) {
		goto out;
	}

	ULONG done = 0;
	st = BCryptEncrypt(k, (PUCHAR)in, OSK_AES_BLOCK, NULL, NULL, 0,
			   (PUCHAR)out, OSK_AES_BLOCK, &done, 0);
	if (st != STATUS_SUCCESS || done != OSK_AES_BLOCK) {
		goto out;
	}
	r = OSK_OK;

out:
	if (k != NULL) {
		BCryptDestroyKey(k);
	}
	BCryptCloseAlgorithmProvider(alg, 0);
	if (r != OSK_OK) {
		osk_wipe(out, OSK_AES_BLOCK);
	}
	return r;
}

/* CNG schreibt den Startwert im uebergebenen Puffer fort, darum eine Kopie:
 * der Startwert des Aufrufers bleibt unberuehrt. */
static osk_result_t cbc(const uint8_t key[OSK_AES_KEY_LEN],
			const uint8_t iv[OSK_AES_BLOCK],
			const uint8_t *in, uint8_t *out, size_t len,
			int encrypt)
{
	if (key == NULL || iv == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (len == 0 || (len % OSK_AES_BLOCK) != 0 || len > 0xFFFFFFFFu) {
		return OSK_ERR_ARG;
	}

	BCRYPT_ALG_HANDLE alg = NULL;
	osk_result_t r = open_aes(&alg, BCRYPT_CHAIN_MODE_CBC);
	if (r != OSK_OK) {
		return r;
	}

	BCRYPT_KEY_HANDLE k = NULL;
	uint8_t iv_copy[OSK_AES_BLOCK];
	memcpy(iv_copy, iv, sizeof(iv_copy));
	r = OSK_ERR_INTERNAL;

	NTSTATUS st = BCryptGenerateSymmetricKey(alg, &k, NULL, 0,
						 (PUCHAR)key, OSK_AES_KEY_LEN, 0);
	if (st != STATUS_SUCCESS) {
		goto out;
	}

	ULONG done = 0;
	if (encrypt) {
		st = BCryptEncrypt(k, (PUCHAR)in, (ULONG)len, NULL,
				   iv_copy, OSK_AES_BLOCK,
				   (PUCHAR)out, (ULONG)len, &done, 0);
	} else {
		st = BCryptDecrypt(k, (PUCHAR)in, (ULONG)len, NULL,
				   iv_copy, OSK_AES_BLOCK,
				   (PUCHAR)out, (ULONG)len, &done, 0);
	}
	if (st != STATUS_SUCCESS || done != len) {
		goto out;
	}
	r = OSK_OK;

out:
	if (k != NULL) {
		BCryptDestroyKey(k);
	}
	BCryptCloseAlgorithmProvider(alg, 0);
	osk_wipe(iv_copy, sizeof(iv_copy));
	if (r != OSK_OK) {
		osk_wipe(out, len);
	}
	return r;
}

osk_result_t osk_aes_cbc_encrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK],
				 const uint8_t *in, uint8_t *out, size_t len)
{
	return cbc(key, iv, in, out, len, 1);
}

osk_result_t osk_aes_cbc_decrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK],
				 const uint8_t *in, uint8_t *out, size_t len)
{
	return cbc(key, iv, in, out, len, 0);
}

osk_result_t osk_random(uint8_t *out, size_t len)
{
	if (out == NULL || len == 0 || len > 0xFFFFFFFFu) {
		return OSK_ERR_ARG;
	}
	const NTSTATUS st = BCryptGenRandom(NULL, (PUCHAR)out, (ULONG)len,
					    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (st != STATUS_SUCCESS) {
		osk_wipe(out, len);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

#endif /* _WIN32 und kein PSA */
