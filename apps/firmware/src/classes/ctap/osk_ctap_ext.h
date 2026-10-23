/* Erweiterungen, die fremde Programme von uns verlangen:
 *
 *   hmac-secret   - Bitwarden, BitLocker und LUKS entsperren damit ihre
 *                   Datentresore. Zwei Geheimnisse je Nachweis, eines fuer
 *                   Zugriffe mit Benutzerpruefung, eines ohne.
 *   PRF           - dasselbe von WebAuthn aus gesehen: der Browser bildet
 *                   salt = SHA-256("WebAuthn PRF" || 0x00 || Eingabe) und
 *                   schickt es als hmac-secret. Am Geraet ist nichts weiter
 *                   zu tun, als hmac-secret richtig zu koennen.
 *   largeBlob     - groessere Daten je Nachweis (Erweiterung aus 12.4,
 *                   nicht der Befehl authenticatorLargeBlobs).
 *   credProtect   - Schutzstufe, die in der Credential-ID mitfaehrt.
 *   minPinLength  - meldet die Mindestlaenge an befugte Dienste.
 *
 * Alle Ausgaben, die in das signierte authData gehoeren, sammelt dieses
 * Modul als kanonisches CBOR; was ungesigniert zurueckgeht, steht getrennt. */
#ifndef OSK_CTAP_EXT_H
#define OSK_CTAP_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_cbor.h"
#include "osk_credid.h"

/* Was in der Anfrage stand. */
typedef struct {
	bool hmac_secret_create;    /* MakeCredential: "hmac-secret": true */
	bool hmac_secret_get;       /* GetAssertion: Abbildung mit Salzen */
	uint8_t peer_pub[64];
	const uint8_t *salt_enc;
	size_t salt_enc_len;
	const uint8_t *salt_auth;
	size_t salt_auth_len;
	uint64_t hmac_protocol;
	bool have_hmac_protocol;

	bool cred_protect;
	uint8_t cred_protect_value;

	bool min_pin_length;

	bool large_blob_create;     /* MakeCredential: support required/preferred */
	bool large_blob_required;
	bool large_blob_read;
	const uint8_t *large_blob_write;
	size_t large_blob_write_len;
	uint64_t large_blob_original_size;
	bool large_blob_have_size;
} osk_ctap_ext_in_t;

/* Liest die Abbildung "extensions" einer Anfrage. Unbekanntes wird
 * uebersprungen, nicht beanstandet. */
osk_result_t osk_ctap_ext_parse(osk_cbor_reader_t *r, bool for_make_credential,
				osk_ctap_ext_in_t *out);

/* Ausgaben fuer MakeCredential: schreibt die Abbildung fuer authData.
 * Liefert 0 Bytes, wenn keine Erweiterung etwas zu sagen hat. */
osk_result_t osk_ctap_ext_make_output(const osk_ctap_ext_in_t *in,
				      const char *rp_id, size_t rp_id_len,
				      uint8_t cred_protect_set,
				      uint8_t *out, size_t cap, size_t *len_out);

/* Ausgaben fuer GetAssertion. uv sagt, welches CredRandom gilt. */
osk_result_t osk_ctap_ext_get_output(const osk_ctap_ext_in_t *in,
				     const osk_credid_t *cred, bool uv,
				     uint8_t *out, size_t cap, size_t *len_out,
				     uint8_t *status_out);

/* Ungesignierte Ausgaben (largeBlob). Schreibt eine CBOR-Abbildung oder
 * nichts. */
osk_result_t osk_ctap_ext_unsigned_output(const osk_ctap_ext_in_t *in,
					  const osk_credid_t *cred,
					  bool allow_list_present,
					  uint8_t *out, size_t cap, size_t *len_out);

/* Dienste, die die Mindestlaenge der PIN erfahren duerfen. */
osk_result_t osk_ctap_minpin_rp_add(const char *rp_id);
void osk_ctap_minpin_rp_clear(void);

#endif
