/* Krypto-Unterbau fuer das Geraet: PSA Crypto aus dem nRF Connect SDK.
 *
 * Auf dem nRF52840 laeuft das im CryptoCell-Beschleuniger; faellt der aus,
 * nimmt PSA die geprueften Softwarefassungen. In beiden Faellen ist es
 * fremder, geprueefter Code - hier wird nur aufgerufen.
 *
 * Dieser Unterbau wird nur uebersetzt, wenn PSA da ist. Der Pruefbau am PC
 * nimmt se050_crypto_cng.c.
 *
 * STAND: geschrieben, am PC nicht uebersetzbar (kein nRF Connect SDK auf
 * diesem Rechner) und am Geraet nicht gelaufen - es gibt noch keine
 * Hardware. Siehe docs/se050.md. */
#include "se050_crypto.h"

#if defined(CONFIG_PSA_CRYPTO_CLIENT) || defined(OSK_CRYPTO_PSA)

#include <psa/crypto.h>
#include <string.h>

/* PSA-Fehler werden nicht durchgereicht: der Aufrufer soll nicht die
 * Fehlerwelt einer Bibliothek kennen muessen, und ein Fehlercode von dort
 * im Protokoll waere eine Spur auf das Geheimnis. */
static osk_result_t map(psa_status_t st)
{
	switch (st) {
	case PSA_SUCCESS:
		return OSK_OK;
	case PSA_ERROR_INVALID_ARGUMENT:
	case PSA_ERROR_BUFFER_TOO_SMALL:
		return OSK_ERR_ARG;
	case PSA_ERROR_NOT_SUPPORTED:
		return OSK_ERR_UNSUPPORTED;
	case PSA_ERROR_INVALID_SIGNATURE:
		return OSK_ERR_DENIED;
	default:
		return OSK_ERR_INTERNAL;
	}
}

osk_result_t osk_crypto_init(void)
{
	const psa_status_t st = psa_crypto_init();
	/* Ein zweiter Aufruf ist erlaubt und meldet ebenfalls Erfolg. */
	return map(st);
}

/* Einen fluechtigen AES-Schluessel anlegen. Er lebt nur fuer diesen einen
 * Vorgang und wird am Ende wieder zerstoert, damit kein Schluesselbund im
 * nRF52840 stehen bleibt - dessen Auslesesperre gilt uns als gebrochen. */
static osk_result_t key_import(const uint8_t key[OSK_AES_KEY_LEN],
			       psa_algorithm_t alg, psa_key_usage_t usage,
			       psa_key_id_t *id_out)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attr, OSK_AES_KEY_LEN * 8u);
	psa_set_key_algorithm(&attr, alg);
	psa_set_key_usage_flags(&attr, usage);
	psa_set_key_lifetime(&attr, PSA_KEY_LIFETIME_VOLATILE);

	const psa_status_t st = psa_import_key(&attr, key, OSK_AES_KEY_LEN, id_out);
	psa_reset_key_attributes(&attr);
	return map(st);
}

osk_result_t osk_aes_cmac(const uint8_t key[OSK_AES_KEY_LEN],
			  const uint8_t *msg, size_t msg_len,
			  uint8_t mac[OSK_AES_BLOCK])
{
	if (key == NULL || mac == NULL || (msg_len > 0 && msg == NULL)) {
		return OSK_ERR_ARG;
	}

	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r = key_import(key, PSA_ALG_CMAC, PSA_KEY_USAGE_SIGN_MESSAGE, &id);
	if (r != OSK_OK) {
		return r;
	}

	size_t out_len = 0;
	const psa_status_t st = psa_mac_compute(id, PSA_ALG_CMAC, msg, msg_len,
						mac, OSK_AES_BLOCK, &out_len);
	psa_destroy_key(id);

	if (st != PSA_SUCCESS) {
		osk_wipe(mac, OSK_AES_BLOCK);
		return map(st);
	}
	if (out_len != OSK_AES_BLOCK) {
		osk_wipe(mac, OSK_AES_BLOCK);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

osk_result_t osk_aes_ecb_encrypt_block(const uint8_t key[OSK_AES_KEY_LEN],
				       const uint8_t in[OSK_AES_BLOCK],
				       uint8_t out[OSK_AES_BLOCK])
{
	if (key == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}

	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r = key_import(key, PSA_ALG_ECB_NO_PADDING,
				    PSA_KEY_USAGE_ENCRYPT, &id);
	if (r != OSK_OK) {
		return r;
	}

	size_t out_len = 0;
	const psa_status_t st = psa_cipher_encrypt(id, PSA_ALG_ECB_NO_PADDING,
						   in, OSK_AES_BLOCK,
						   out, OSK_AES_BLOCK, &out_len);
	psa_destroy_key(id);

	if (st != PSA_SUCCESS || out_len != OSK_AES_BLOCK) {
		osk_wipe(out, OSK_AES_BLOCK);
		return (st != PSA_SUCCESS) ? map(st) : OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

/* PSA stellt den Startwert bei psa_cipher_encrypt() selbst voran. SCP03
 * schreibt den Startwert aber vor, darum laeuft CBC hier ueber die
 * schrittweise Schnittstelle mit psa_cipher_set_iv(). */
static osk_result_t cbc(const uint8_t key[OSK_AES_KEY_LEN],
			const uint8_t iv[OSK_AES_BLOCK],
			const uint8_t *in, uint8_t *out, size_t len,
			int encrypt)
{
	if (key == NULL || iv == NULL || in == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (len == 0 || (len % OSK_AES_BLOCK) != 0) {
		return OSK_ERR_ARG;
	}

	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r = key_import(key, PSA_ALG_CBC_NO_PADDING,
				    encrypt ? PSA_KEY_USAGE_ENCRYPT
					    : PSA_KEY_USAGE_DECRYPT,
				    &id);
	if (r != OSK_OK) {
		return r;
	}

	psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;
	psa_status_t st = encrypt
		? psa_cipher_encrypt_setup(&op, id, PSA_ALG_CBC_NO_PADDING)
		: psa_cipher_decrypt_setup(&op, id, PSA_ALG_CBC_NO_PADDING);

	if (st == PSA_SUCCESS) {
		st = psa_cipher_set_iv(&op, iv, OSK_AES_BLOCK);
	}

	size_t done = 0;
	if (st == PSA_SUCCESS) {
		st = psa_cipher_update(&op, in, len, out, len, &done);
	}
	if (st == PSA_SUCCESS) {
		size_t tail = 0;
		st = psa_cipher_finish(&op, out + done, len - done, &tail);
		done += tail;
	}

	if (st != PSA_SUCCESS) {
		psa_cipher_abort(&op);
	}
	psa_destroy_key(id);

	if (st != PSA_SUCCESS || done != len) {
		osk_wipe(out, len);
		return (st != PSA_SUCCESS) ? map(st) : OSK_ERR_INTERNAL;
	}
	return OSK_OK;
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
	if (out == NULL || len == 0) {
		return OSK_ERR_ARG;
	}
	const psa_status_t st = psa_generate_random(out, len);
	if (st != PSA_SUCCESS) {
		osk_wipe(out, len);
		return map(st);
	}
	return OSK_OK;
}

#endif /* PSA vorhanden */
