/* PIN/UV-Auth-Protokolle 1 und 2 (CTAP 2.2, Abschnitt 6.5).
 *
 * Protokoll 2 ist der verschluesselte Kanal, den wir bevorzugen: getrennte
 * Schluessel fuer Verschluesselung und MAC, AES-256-CBC mit zufaelligem IV,
 * HMAC-SHA-256 in voller Laenge. Protokoll 1 bleibt dabei, weil aeltere
 * Programme nur das kennen.
 *
 * Die PIN selbst sieht dieses Modul nur als LEFT(SHA-256(PIN), 16) - geprueft
 * wird sie im SE050, und auch der Fehlversuchszaehler liegt dort. */
#ifndef OSK_PIN_H
#define OSK_PIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_cbor.h"

#define OSK_PIN_TOKEN_LEN 32
#define OSK_PIN_HASH_LEN 16   /* LEFT(SHA-256(PIN), 16) */
#define OSK_PIN_SHARED_LEN 64 /* HMAC-Teil || AES-Teil (Protokoll 2) */

typedef struct {
	uint8_t token[OSK_PIN_TOKEN_LEN];
	bool in_use;
	uint8_t permissions;
	bool rp_id_set;
	uint8_t rp_id_hash[32];
	bool user_present;
	bool user_verified;
	uint8_t mismatches; /* drei hintereinander sperren bis zum Stromaus */
	bool power_cycle_needed;
	bool pin_set;
	uint8_t pin_code_points;
} osk_pin_state_t;

osk_pin_state_t *osk_pin_state(void);

/* Beim Einschalten: Schluesseltauschschluessel anlegen, Zustand leeren. */
osk_result_t osk_pin_init(void);

/* Wirft den Schluesseltauschschluessel weg und zieht einen neuen. */
osk_result_t osk_pin_regenerate(void);

/* Neues Token ziehen; alle bisherigen werden damit ungueltig. */
osk_result_t osk_pin_reset_token(void);

bool osk_pin_protocol_supported(uint64_t protocol);

/* Gemeinsames Geheimnis aus dem oeffentlichen Schluessel der Gegenseite.
 * shared_out ist immer OSK_PIN_SHARED_LEN lang; bei Protokoll 1 steht
 * derselbe Wert in beiden Haelften, damit der Rest des Codes nicht
 * unterscheiden muss. */
osk_result_t osk_pin_shared_secret(uint64_t protocol, const uint8_t peer_pub[64],
				   uint8_t shared_out[OSK_PIN_SHARED_LEN]);

/* encrypt/decrypt/authenticate/verify der Spezifikation. */
osk_result_t osk_pin_encrypt(uint64_t protocol, const uint8_t *key, const uint8_t *in,
			     size_t in_len, uint8_t *out, size_t cap, size_t *len_out);
osk_result_t osk_pin_decrypt(uint64_t protocol, const uint8_t *key, const uint8_t *in,
			     size_t in_len, uint8_t *out, size_t cap, size_t *len_out);
void osk_pin_authenticate(uint64_t protocol, const uint8_t *key, size_t key_len, const uint8_t *msg,
			  size_t msg_len, uint8_t *out, size_t *len_out);
bool osk_pin_verify_mac(uint64_t protocol, const uint8_t *key, size_t key_len, const uint8_t *msg,
			size_t msg_len, const uint8_t *mac, size_t mac_len);

/* Verify gegen das laufende Token, mit Pruefung auf in_use. */
bool osk_pin_verify_token(uint64_t protocol, const uint8_t *msg, size_t msg_len, const uint8_t *mac,
			  size_t mac_len);

/* Zustandspflege nach Abschnitt 6.5.3.2. */
void osk_pin_begin_using(bool user_is_present);
void osk_pin_stop_using(void);
void osk_pin_clear_up(void);
void osk_pin_clear_uv(void);
void osk_pin_clear_perms_except_lbw(void);
bool osk_pin_get_up_flag(void);
bool osk_pin_get_uv_flag(void);
bool osk_pin_has_permission(uint8_t perm);

/* Den eigenen oeffentlichen Schluesseltauschschluessel als COSE_Key schreiben. */
osk_result_t osk_pin_put_public_key(osk_cbor_writer_t *w);

/* Der ganze Befehl authenticatorClientPIN. */
uint8_t osk_pin_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w);

#endif
