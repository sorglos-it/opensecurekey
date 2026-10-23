/* Die Credential-ID: alles, was zum Wiederfinden eines abgeleiteten
 * Schluessels noetig ist - verschluesselt und gegen Verfaelschung gesichert.
 *
 * Aufbau (97 Bytes):
 *   [0]      Fassung (0x01)
 *   [1..16]  Zufall, dient als IV
 *   [17..80] AES-256-CBC ueber 64 Bytes Inhalt
 *   [81..96] 16 Bytes HMAC-SHA-256 ueber alles davor (gekuerzt)
 *
 * Inhalt (64 Bytes):
 *   [0]      Merkmale (Bit 0: residenter Schluessel, Bit 1: hmac-secret)
 *   [1]      credProtect (1..3)
 *   [2..33]  rpIdHash
 *   [34..49] Salz der Schluesselableitung
 *   [50..53] Erstellzeit, gross-endisch
 *   [54..63] frei, muss null sein
 *
 * Erst wird der MAC geprueft, dann entschluesselt - nie umgekehrt. Eine ID
 * von einem anderen Geraet oder mit geaenderter rpIdHash fliegt damit auf. */
#ifndef OSK_CREDID_H
#define OSK_CREDID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_ctap_key.h"

#define OSK_CREDID_LEN 97
#define OSK_CREDID_VERSION 0x01

typedef struct {
	bool resident;
	bool hmac_secret;
	uint8_t cred_protect;
	uint8_t rp_id_hash[32];
	uint8_t salt[OSK_KEY_SALT_LEN];
	uint32_t created;
} osk_credid_t;

/* Packt eine neue ID. Das Salz muss der Aufrufer schon gezogen haben. */
osk_result_t osk_credid_pack(const osk_credid_t *in, uint8_t *out, size_t cap, size_t *len_out);

/* Prueft und entpackt. Antwortet OSK_ERR_NOT_FOUND, wenn die ID nicht von
 * diesem Geraet stammt oder angefasst wurde - der Aufrufer darf daraus nicht
 * mehr schliessen als genau das. */
osk_result_t osk_credid_unpack(const uint8_t *in, size_t len, osk_credid_t *out);

#endif
