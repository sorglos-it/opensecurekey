/* Nachbau des Sicherheitschips, nur so weit wie die Rechteschicht ihn
 * benutzt: HMAC, Zufall, Unterschrift und monotone Zaehler.
 *
 * ER IST KEIN SE050 UND SOLL KEINER SEIN. Was er prueft, ist die
 * Rechteschicht; was er nicht prueft, ist der Chip. Dass ein echter SE051
 * dieselben Antworten gibt, ist Sache von Strang A und am Geraet zu messen.
 * Diese Grenze steht hier, damit ein gruener Lauf nicht fuer mehr genommen
 * wird, als er ist.
 *
 * Der Zufall ist ABSICHTLICH nicht zufaellig, sondern ein Zaehler: ein
 * durchgefallener Prueffall soll beim naechsten Lauf wieder durchfallen.
 * Genau deshalb darf dieser Nachbau nie ins Geraet - er steht nur unter
 * apps/firmware/test/ und wird von keinem Bau des Abbilds erfasst. */
#include <string.h>
#include "osk_se050.h"
#include "osk_crypto.h"

#define FAKE_KEYS 8

static struct {
	bool used;
	osk_key_id_t id;
	uint8_t secret[32];
} g_keys[FAKE_KEYS];

static struct {
	bool used;
	osk_key_id_t id;
	uint64_t value;
} g_counters[FAKE_KEYS];

static uint8_t g_rand_next;

/* ------------------------------------------- Steuerung durch die Tests */

void fake_reset(void)
{
	memset(g_keys, 0, sizeof(g_keys));
	memset(g_counters, 0, sizeof(g_counters));
	g_rand_next = 1u;
}

/* Ein Geheimnis eintragen. Der Inhalt ist die Kennung, auf 32 Byte
 * ausgebreitet - so ist jeder Schluessel anders und doch vorhersehbar. */
void fake_add_key(osk_key_id_t id)
{
	for (int i = 0; i < FAKE_KEYS; i++) {
		if (!g_keys[i].used) {
			g_keys[i].used = true;
			g_keys[i].id = id;
			for (size_t k = 0u; k < sizeof(g_keys[i].secret); k++) {
				g_keys[i].secret[k] = (uint8_t)(id + (uint32_t)k);
			}
			return;
		}
	}
}

/* Dasselbe, was die Rechteschicht im Chip rechnen laesst - aber hier, im
 * Prueffall, damit der Test den erwarteten Beweis selbst bilden kann,
 * statt ihn von der geprueften Seite zu holen. */
int fake_hmac_of(osk_key_id_t id, const uint8_t *msg, size_t len, uint8_t out[32])
{
	for (int i = 0; i < FAKE_KEYS; i++) {
		if (g_keys[i].used && g_keys[i].id == id) {
			osk_hmac_sha256(g_keys[i].secret, sizeof(g_keys[i].secret), msg, len, out);
			return 0;
		}
	}
	return -1;
}

/* ----------------------------------------------- Vertrag aus osk_se050.h */

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	if (cap < 32u) {
		return OSK_ERR_ARG;
	}
	if (fake_hmac_of(id, msg, msg_len, mac_out) != 0) {
		return OSK_ERR_NOT_FOUND;
	}
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	for (size_t i = 0u; i < len; i++) {
		out[i] = g_rand_next++;
		if (g_rand_next == 0u) {
			g_rand_next = 1u;
		}
	}
	return OSK_OK;
}

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	uint8_t msg[64];

	if (cap < 32u || hash_len > 32u) {
		return OSK_ERR_ARG;
	}
	memcpy(msg, hash, hash_len);
	for (size_t i = 0u; i < 4u; i++) {
		msg[hash_len + i] = (uint8_t)(id >> (8u * (3u - i)));
	}
	osk_sha256(msg, hash_len + 4u, sig_out);
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_counter_create(osk_key_id_t id)
{
	for (int i = 0; i < FAKE_KEYS; i++) {
		if (!g_counters[i].used) {
			g_counters[i].used = true;
			g_counters[i].id = id;
			g_counters[i].value = 0u;
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_se050_counter_read(osk_key_id_t id, uint64_t *value_out)
{
	for (int i = 0; i < FAKE_KEYS; i++) {
		if (g_counters[i].used && g_counters[i].id == id) {
			*value_out = g_counters[i].value;
			return OSK_OK;
		}
	}
	return OSK_ERR_NOT_FOUND;
}

/* Nur vorwaerts - auch der Nachbau. Ein Nachbau, der zurueckdrehen laesst,
 * wuerde einen Prueffall gruen machen, den der echte Chip rot macht. */
osk_result_t osk_se050_counter_raise(osk_key_id_t id, uint64_t at_least)
{
	for (int i = 0; i < FAKE_KEYS; i++) {
		if (g_counters[i].used && g_counters[i].id == id) {
			if (at_least > g_counters[i].value) {
				g_counters[i].value = at_least;
			}
			return OSK_OK;
		}
	}
	return OSK_ERR_NOT_FOUND;
}
