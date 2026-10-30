/* Krypto fuer das Geraet: alles ueber die PSA-Crypto-Schnittstelle des
 * nRF Connect SDK. Hier wird nichts selbst gerechnet - diese Datei uebersetzt
 * nur zwischen osk_crypto.h und PSA.
 *
 * Warum PSA und nicht etwas Eigenes: AES-GCM, SHA-256, HKDF und Argon2id
 * richtig zu schreiben ist Spezialistenarbeit, und ein Fehler darin fiele
 * niemandem auf. Der Code aus dem SDK ist geprueft; ein eigener waere es nie.
 *
 * STAND: am PC nicht uebersetzt (dort laeuft die Attrappe aus
 * apps/firmware/test/store/fake_crypto.c), am Geraet nicht gelaufen - es ist
 * noch keine Hardware da. Was hier zu pruefen ist, sobald sie da ist:
 *  - ob der PSA-Treiber dieses SDK Argon2id wirklich anbietet (bei Nordic
 *    kommt es aus dem Oberon-Treiber; sonst muss das Hauptkennwort auf dem
 *    Rechner abgeleitet werden)
 *  - wie lange Argon2id mit OSK_ARGON2_DEV_* auf dem nRF52840 braucht
 *  - dass psa_crypto_init() vor dem ersten Aufruf gelaufen ist
 *
 * Gebaut wird diese Datei nur im Firmware-Build (CONFIG_PSA_CRYPTO_...). */

#include <string.h>
#include "osk_crypto.h"

#ifdef OSK_TARGET_DEVICE

#include <psa/crypto.h>

/* ---- Fehler uebersetzen ----------------------------------------------- */

static osk_result_t from_psa(psa_status_t s)
{
	switch (s) {
	case PSA_SUCCESS:
		return OSK_OK;
	case PSA_ERROR_INVALID_ARGUMENT:
	case PSA_ERROR_INVALID_HANDLE:
		return OSK_ERR_ARG;
	case PSA_ERROR_BUFFER_TOO_SMALL:
		return OSK_ERR_FULL;
	case PSA_ERROR_INVALID_SIGNATURE:
	case PSA_ERROR_INVALID_PADDING:
		return OSK_ERR_FORMAT; /* Pruefwert stimmt nicht */
	case PSA_ERROR_NOT_SUPPORTED:
		return OSK_ERR_UNSUPPORTED;
	case PSA_ERROR_INSUFFICIENT_MEMORY:
		return OSK_ERR_FULL;
	default:
		return OSK_ERR_INTERNAL;
	}
}

/* Ein Sitzungsschluessel wird fuer jeden Aufruf eingelegt und danach wieder
 * entfernt. Er liegt damit nur so kurz wie noetig im Chip. */
static osk_result_t import_aes(const osk_key_t *key, psa_algorithm_t alg, psa_key_usage_t usage,
			       psa_key_id_t *id_out)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t s;

	psa_set_key_usage_flags(&attr, usage);
	psa_set_key_algorithm(&attr, alg);
	psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attr, 256);

	s = psa_import_key(&attr, key->b, OSK_KEY_LEN, id_out);
	psa_reset_key_attributes(&attr);
	return from_psa(s);
}

/* ---- AES-256-GCM ------------------------------------------------------ */

osk_result_t osk_crypto_seal(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			     size_t plain_len, uint8_t *ct_out, size_t ct_cap, size_t *ct_len_out)
{
	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r;

	if (key == NULL || nonce == NULL || plain == NULL || ct_out == NULL || ct_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	r = import_aes(key, PSA_ALG_GCM, PSA_KEY_USAGE_ENCRYPT, &id);
	if (r != OSK_OK) {
		return r;
	}
	psa_status_t s = psa_aead_encrypt(id, PSA_ALG_GCM, nonce, OSK_NONCE_LEN, aad, aad_len,
					  plain, plain_len, ct_out, ct_cap, ct_len_out);

	(void)psa_destroy_key(id);
	return from_psa(s);
}

osk_result_t osk_crypto_open(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			     uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out)
{
	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r;

	if (key == NULL || nonce == NULL || ct == NULL || plain_out == NULL ||
	    plain_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	r = import_aes(key, PSA_ALG_GCM, PSA_KEY_USAGE_DECRYPT, &id);
	if (r != OSK_OK) {
		return r;
	}
	/* PSA gibt bei falschem Pruefwert nichts heraus - genau das verspricht
	 * osk_crypto.h dem Tresor. */
	psa_status_t s = psa_aead_decrypt(id, PSA_ALG_GCM, nonce, OSK_NONCE_LEN, aad, aad_len, ct,
					  ct_len, plain_out, plain_cap, plain_len_out);

	(void)psa_destroy_key(id);
	if (s != PSA_SUCCESS) {
		*plain_len_out = 0;
		osk_crypto_wipe(plain_out, plain_cap);
	}
	return from_psa(s);
}

/* ---- SHA-256 ---------------------------------------------------------- */

osk_result_t osk_crypto_sha256(const uint8_t *msg, size_t len, uint8_t out[OSK_HASH_LEN])
{
	size_t got = 0;

	if (out == NULL || (msg == NULL && len > 0u)) {
		return OSK_ERR_ARG;
	}
	psa_status_t s = psa_hash_compute(PSA_ALG_SHA_256, msg, len, out, OSK_HASH_LEN, &got);

	if (s == PSA_SUCCESS && got != OSK_HASH_LEN) {
		return OSK_ERR_INTERNAL;
	}
	return from_psa(s);
}

/* ---- HKDF-SHA256 ------------------------------------------------------ */

osk_result_t osk_crypto_hkdf(const uint8_t *ikm, size_t ikm_len, const uint8_t *salt,
			     size_t salt_len, const uint8_t *info, size_t info_len, uint8_t *out,
			     size_t out_len)
{
	psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
	psa_status_t s;

	if (ikm == NULL || out == NULL || out_len == 0u) {
		return OSK_ERR_ARG;
	}
	s = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
	if (s == PSA_SUCCESS && salt != NULL && salt_len > 0u) {
		s = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt,
						   salt_len);
	}
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm,
						   ikm_len);
	}
	if (s == PSA_SUCCESS && info != NULL && info_len > 0u) {
		s = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, info,
						   info_len);
	}
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_output_bytes(&op, out, out_len);
	}
	(void)psa_key_derivation_abort(&op);
	if (s != PSA_SUCCESS) {
		osk_crypto_wipe(out, out_len);
	}
	return from_psa(s);
}

/* ---- Argon2id --------------------------------------------------------- */

osk_result_t osk_crypto_argon2id(const osk_argon2_params_t *p, const char *passphrase,
				 size_t pass_len, const uint8_t salt[OSK_SALT_LEN], uint8_t *out,
				 size_t out_len)
{
#if defined(PSA_ALG_ARGON2ID)
	psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
	psa_status_t s;

	if (p == NULL || passphrase == NULL || salt == NULL || out == NULL || out_len == 0u) {
		return OSK_ERR_ARG;
	}
	s = psa_key_derivation_setup(&op, PSA_ALG_ARGON2ID);
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_input_integer(&op, PSA_KEY_DERIVATION_INPUT_COST, p->t_cost);
	}
	if (s == PSA_SUCCESS) {
		/* PSA zaehlt den Speicher in Byte, die Datei in KiB. */
		s = psa_key_derivation_input_integer(&op, PSA_KEY_DERIVATION_INPUT_MEMORY_COST,
						     (uint64_t)p->m_cost_kib * 1024u);
	}
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt,
						   OSK_SALT_LEN);
	}
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_PASSWORD,
						   (const uint8_t *)passphrase, pass_len);
	}
	if (s == PSA_SUCCESS) {
		s = psa_key_derivation_output_bytes(&op, out, out_len);
	}
	(void)psa_key_derivation_abort(&op);
	if (s != PSA_SUCCESS) {
		osk_crypto_wipe(out, out_len);
	}
	return from_psa(s);
#else
	/* Kein Argon2id im Treiber: dann wird die Sicherungsdatei auf dem
	 * Rechner angelegt und geoeffnet, nicht im Stick. Lieber eine klare
	 * Auskunft als eine schwache Ableitung von Hand. */
	(void)p;
	(void)passphrase;
	(void)pass_len;
	(void)salt;
	if (out != NULL) {
		osk_crypto_wipe(out, out_len);
	}
	return OSK_ERR_UNSUPPORTED;
#endif
}

/* ---- Zufall und Kleinigkeiten ---------------------------------------- */

osk_result_t osk_crypto_random(uint8_t *out, size_t len)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	return from_psa(psa_generate_random(out, len));
}

/* ---- Datentraeger: ChaCha20-Poly1305 und Ed25519-Pruefung -------------
 *
 * HINZUGEFUEGT vom datentraeger-Strang. Steht hier und nicht in einer eigenen
 * Datei, damit die Umsetzung von from_psa() nicht zweimal existiert - eine
 * zweite Zuordnung von PSA-Fehlern auf osk_result_t waere eine Zahl an zwei
 * Stellen.
 *
 * Warum ein anderes Verfahren als der Tresor: der Beschleuniger des nRF52840
 * kann laut Treibertabelle weder AES-256 noch GCM, und beim Datentraeger geht
 * es um Megabyte am Stueck. Begruendung in docs/datentraeger-konzept.md.
 *
 * AM GERAET UNGEPRUEFT, und hier liegen zwei Annahmen, die am Brett zu
 * pruefen sind:
 *  - dass PSA_ALG_CHACHA20_POLY1305 auf diesem Brett vom Beschleuniger kommt
 *    und nicht in Software laeuft,
 *  - dass PSA_ALG_PURE_EDDSA zum Pruefen fremder Signaturen ueberhaupt
 *    angeboten wird. */

#include "osk_volume_crypto.h"

static osk_result_t import_chacha(const osk_key_t *key, psa_key_usage_t usage, psa_key_id_t *id_out)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t s;

	psa_set_key_usage_flags(&attr, usage);
	psa_set_key_algorithm(&attr, PSA_ALG_CHACHA20_POLY1305);
	psa_set_key_type(&attr, PSA_KEY_TYPE_CHACHA20);
	psa_set_key_bits(&attr, 256);

	s = psa_import_key(&attr, key->b, OSK_KEY_LEN, id_out);
	psa_reset_key_attributes(&attr);
	return from_psa(s);
}

osk_result_t osk_vol_aead_seal(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			       size_t plain_len, uint8_t *ct_out, size_t ct_cap, size_t *ct_len_out)
{
	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r;
	psa_status_t s;

	if (key == NULL || nonce == NULL || plain == NULL || ct_out == NULL || ct_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	r = import_chacha(key, PSA_KEY_USAGE_ENCRYPT, &id);
	if (r != OSK_OK) {
		return r;
	}
	s = psa_aead_encrypt(id, PSA_ALG_CHACHA20_POLY1305, nonce, OSK_VOL_AEAD_NONCE_LEN, aad,
			     aad_len, plain, plain_len, ct_out, ct_cap, ct_len_out);
	(void)psa_destroy_key(id);
	return from_psa(s);
}

osk_result_t osk_vol_aead_open(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			       uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out)
{
	psa_key_id_t id = PSA_KEY_ID_NULL;
	osk_result_t r;
	psa_status_t s;

	if (key == NULL || nonce == NULL || ct == NULL || plain_out == NULL ||
	    plain_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	r = import_chacha(key, PSA_KEY_USAGE_DECRYPT, &id);
	if (r != OSK_OK) {
		return r;
	}
	s = psa_aead_decrypt(id, PSA_ALG_CHACHA20_POLY1305, nonce, OSK_VOL_AEAD_NONCE_LEN, aad,
			     aad_len, ct, ct_len, plain_out, plain_cap, plain_len_out);
	(void)psa_destroy_key(id);
	if (s != PSA_SUCCESS) {
		/* Bei falschem Stempel gibt PSA nichts heraus. Trotzdem
		 * wischen - der Aufrufer soll auch keinen Rest sehen. */
		*plain_len_out = 0u;
		osk_crypto_wipe(plain_out, plain_cap);
	}
	return from_psa(s);
}

osk_result_t osk_vol_sig_verify(const uint8_t pub[OSK_VOL_PUB_LEN], const uint8_t *msg,
				size_t msg_len, const uint8_t sig[OSK_VOL_SIG_LEN])
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t id = PSA_KEY_ID_NULL;
	psa_status_t s;

	if (pub == NULL || msg == NULL || sig == NULL) {
		return OSK_ERR_ARG;
	}
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attr, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attr, 255);

	s = psa_import_key(&attr, pub, OSK_VOL_PUB_LEN, &id);
	psa_reset_key_attributes(&attr);
	if (s != PSA_SUCCESS) {
		return from_psa(s);
	}
	s = psa_verify_message(id, PSA_ALG_PURE_EDDSA, msg, msg_len, sig, OSK_VOL_SIG_LEN);
	(void)psa_destroy_key(id);
	/* Eine falsche Signatur ist OSK_ERR_FORMAT, nicht OSK_ERR_DENIED: die
	 * Daten passen nicht zum Siegel. Wer das mit "falscher Stick"
	 * zusammenwirft, meldet dem Nutzer das Falsche. */
	return from_psa(s);
}

#endif /* OSK_TARGET_DEVICE */

/* Die beiden letzten brauchen kein PSA und gelten fuer jeden Bau. Sie stehen
 * trotzdem hier, damit es nur eine Umsetzung gibt. */
#ifdef OSK_TARGET_DEVICE

bool osk_crypto_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
	uint8_t diff = 0;

	if (a == NULL || b == NULL) {
		return false;
	}
	/* Ohne Abbruch bei der ersten Abweichung: die Laufzeit darf nicht
	 * verraten, wie weit zwei Pruefwerte uebereinstimmen. */
	for (size_t i = 0; i < len; i++) {
		diff |= (uint8_t)(a[i] ^ b[i]);
	}
	return diff == 0u;
}

void osk_crypto_wipe(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	if (buf == NULL) {
		return;
	}
	/* volatile, damit der Uebersetzer das Loeschen nicht wegwirft, weil
	 * danach niemand mehr liest. */
	while (len-- > 0u) {
		*p++ = 0u;
	}
}

#endif /* OSK_TARGET_DEVICE */
