/* Krypto-Attrappe des Datentraegers - siehe osk_volume_crypto.h. KEINE
 * Sicherheit, und ausdruecklich KEIN ChaCha20-Poly1305.
 *
 * Sie leiht sich den AEAD des Tresors (osk_crypto_seal/open, also die
 * GCM-Attrappe) und reicht ihn durch. Das ist richtig so: geprueft wird der
 * ABLAUF des Datentraegers - Nonce aus Schreibzaehler und Blocknummer,
 * Stempel im Stempelfeld statt im Block, Empfaengerzeilen -, nicht das
 * Verfahren. Dass auf dem Geraet ChaCha20-Poly1305 gerechnet wird und dass
 * der Beschleuniger es wirklich kann, ist AM GERAET UNGEPRUEFT.
 *
 * Die Stempellaenge ist in beiden Verfahren 16 Byte; wird sie es nicht mehr,
 * faellt es hier beim Uebersetzen auf. */

#include <string.h>
#include "osk_volume_crypto.h"
#include "fake_se050.h"

_Static_assert(OSK_VOL_AEAD_TAG_LEN == OSK_TAG_LEN, "Stempellaengen laufen auseinander");
_Static_assert(OSK_VOL_AEAD_NONCE_LEN == OSK_NONCE_LEN, "Noncelaengen laufen auseinander");

osk_result_t osk_vol_aead_seal(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			       size_t plain_len, uint8_t *ct_out, size_t ct_cap, size_t *ct_len_out)
{
	return osk_crypto_seal(key, nonce, aad, aad_len, plain, plain_len, ct_out, ct_cap,
			       ct_len_out);
}

osk_result_t osk_vol_aead_open(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			       uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out)
{
	return osk_crypto_open(key, nonce, aad, aad_len, ct, ct_len, plain_out, plain_cap,
			       plain_len_out);
}

osk_result_t osk_vol_sig_verify(const uint8_t pub[OSK_VOL_PUB_LEN], const uint8_t *msg,
				size_t msg_len, const uint8_t sig[OSK_VOL_SIG_LEN])
{
	if (pub == NULL || msg == NULL || sig == NULL) {
		return OSK_ERR_ARG;
	}
	/* Die Attrappe des Chips rechnet die Signatur nach. Warum das eine
	 * Attrappe und kein Beweis ist, steht in fake_se050.c. */
	return fake_se050_sig_matches(pub, msg, msg_len, sig) ? OSK_OK : OSK_ERR_FORMAT;
}
