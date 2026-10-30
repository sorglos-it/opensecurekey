/* Die Empfaengerzeilen - die EINZIGE Stelle, an der sich die drei
 * Betriebsarten wirklich unterscheiden. Modell von age: ein Datenschluessel,
 * mehrfach eingewickelt, je Empfaenger eine Zeile.
 *
 *   geraet   Chipschluessel dieses Sticks       Modus 1 und 2
 *   wort     aus dem Kartenwort abgeleitet      Modus 2
 *   stick    X25519 an einen anderen Stick      Modus 2 - NICHT GEBAUT
 *   (keine)                                     Modus 3
 *
 * Aufbau einer Zeile, feste Laengen:
 *   0   1   Art
 *   1   3   Reserve
 *   4  16   Salz (nur bei "wort")
 *  20  32   fluechtiger oeffentlicher Teil (nur bei "stick")
 *  52  48   eingewickelter K_daten (32 + 16 Stempel) */

#include <string.h>
#include "osk_volume.h"
#include "osk_volume_crypto.h"
#include "volume_internal.h"

#define L_KIND 0u
#define L_SALT 4u
#define L_EPHEM 20u
#define L_WRAP 52u

/* Argon2id auf dem GERAET. Dieselben Werte wie die Sicherungsdatei
 * (osk_backup.h): 64 KiB, 64 Durchgaenge - mehr hat der Rechenkern nicht.
 *
 * Das ist GEGEN DURCHPROBIEREN ZU SCHWACH, und darum ist der Regelfall ein
 * vom Stick erzeugtes Kartenwort mit 80 Bit echter Entropie: dann ist es
 * gleichgueltig, wie langsam die Ableitung ist. Wer ein selbst erdachtes
 * Passwort eingibt, hat den Schutz seines Passworts und nicht mehr - das muss
 * die Anzeige sagen, und diese Datei kann es nicht erzwingen. */
static const osk_argon2_params_t word_params = {
    .m_cost_kib = 64u,
    .t_cost = 64u,
    .lanes = 1u,
};

/* Einwickelschluessel der Zeile beschaffen. */
static osk_result_t wrap_key(const osk_volume_config_t *cfg, osk_vol_recv_kind_t kind,
			     const uint8_t *salt, const osk_vol_recipient_t *secret, osk_key_t *out)
{
	switch (kind) {
	case OSK_VOL_RECV_DEVICE:
		/* K_geraet. Entsteht im Chip und verlaesst ihn als
		 * abgeleiteter Wert - das Grundgeheimnis nie. Ein fremder
		 * Stick bekommt hier einen anderen Wert, und genau daran
		 * scheitert er. */
		return osk_se050_hkdf(cfg->root_key, (const uint8_t *)"osk/volume/device", 17u,
				      out->b, OSK_KEY_LEN);
	case OSK_VOL_RECV_WORD:
		if (secret == NULL || secret->word == NULL || secret->word_len == 0u) {
			return OSK_ERR_DENIED;
		}
		return osk_crypto_argon2id(&word_params, secret->word, secret->word_len, salt,
					   out->b, OSK_KEY_LEN);
	case OSK_VOL_RECV_STICK:
		/* NICHT GEBAUT, und das ist eine Meldung, keine Nachlaessigkeit:
		 * ob der SE051C2 Curve25519-ECDH kann, ist nicht bestaetigt
		 * (offener Punkt 6 des Konzepts). Eine Attrappe, die hier ein
		 * gemeinsames Geheimnis aus zwei oeffentlichen Teilen
		 * zusammenrechnet, wuerde die EIGENE Seite des Vertrags
		 * pruefen und nicht die des Chips - also lieber nichts. */
		return OSK_ERR_UNSUPPORTED;
	default:
		return OSK_ERR_ARG;
	}
}

osk_result_t vol_recv_wrap(const osk_volume_config_t *cfg, const osk_vol_recipient_t *recv,
			   const uint8_t id[OSK_VOL_ID_LEN], const osk_key_t *k_data,
			   uint8_t line[OSK_VOL_RECV_LINE_LEN])
{
	osk_key_t w;
	uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN];
	size_t ct_len = 0u;
	osk_result_t r;

	if (cfg == NULL || recv == NULL || k_data == NULL || line == NULL) {
		return OSK_ERR_ARG;
	}
	memset(line, 0, OSK_VOL_RECV_LINE_LEN);
	line[L_KIND] = (uint8_t)recv->kind;

	if (recv->kind == OSK_VOL_RECV_WORD) {
		r = osk_crypto_random(line + L_SALT, OSK_VOL_RECV_SALT_LEN);
		if (r != OSK_OK) {
			return r;
		}
	}
	r = wrap_key(cfg, recv->kind, line + L_SALT, recv, &w);
	if (r != OSK_OK) {
		return r;
	}
	/* Die Nonce der Zeile: Zweck WRAP und die Art als "Blocknummer". Je
	 * Art gibt es nur eine Zeile, und der Einwickelschluessel ist je Zeile
	 * ein anderer (eigenes Salz, eigener Chipschluessel) - damit
	 * wiederholt sich kein Paar aus Schluessel und Nonce. */
	memset(nonce, 0, sizeof(nonce));
	nonce[4] = (uint8_t)recv->kind;
	nonce[8] = (uint8_t)(OSK_VOL_PURPOSE_WRAP & 0xffu);

	r = osk_vol_aead_seal(&w, nonce, id, OSK_VOL_ID_LEN, k_data->b, OSK_KEY_LEN, line + L_WRAP,
			      OSK_VOL_RECV_WRAP_LEN, &ct_len);
	osk_crypto_wipe(&w, sizeof(w));
	if (r != OSK_OK) {
		return r;
	}
	return (ct_len == OSK_VOL_RECV_WRAP_LEN) ? OSK_OK : OSK_ERR_INTERNAL;
}

osk_result_t vol_recv_unwrap(const osk_volume_config_t *cfg,
			     const uint8_t line[OSK_VOL_RECV_LINE_LEN],
			     const uint8_t id[OSK_VOL_ID_LEN], const osk_vol_recipient_t *unlock,
			     osk_key_t *k_data_out)
{
	osk_vol_recv_kind_t kind;
	osk_key_t w;
	uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN];
	size_t pt_len = 0u;
	osk_result_t r;

	if (cfg == NULL || line == NULL || k_data_out == NULL) {
		return OSK_ERR_ARG;
	}
	kind = (osk_vol_recv_kind_t)line[L_KIND];
	if (kind == OSK_VOL_RECV_WORD && unlock == NULL) {
		/* Diese Zeile braucht das Kartenwort, und es wurde keines
		 * mitgegeben. Kein Fehler - die naechste Zeile kann passen. */
		return OSK_ERR_DENIED;
	}
	r = wrap_key(cfg, kind, line + L_SALT, unlock, &w);
	if (r != OSK_OK) {
		return r == OSK_ERR_UNSUPPORTED ? r : OSK_ERR_DENIED;
	}
	memset(nonce, 0, sizeof(nonce));
	nonce[4] = (uint8_t)kind;
	nonce[8] = (uint8_t)(OSK_VOL_PURPOSE_WRAP & 0xffu);

	r = osk_vol_aead_open(&w, nonce, id, OSK_VOL_ID_LEN, line + L_WRAP, OSK_VOL_RECV_WRAP_LEN,
			      k_data_out->b, OSK_KEY_LEN, &pt_len);
	osk_crypto_wipe(&w, sizeof(w));
	if (r != OSK_OK) {
		/* Falscher Stick oder falsches Wort. Der Stempel sagt nicht,
		 * welches von beiden - und das soll er auch nicht. */
		osk_crypto_wipe(k_data_out, sizeof(*k_data_out));
		return OSK_ERR_DENIED;
	}
	if (pt_len != OSK_KEY_LEN) {
		osk_crypto_wipe(k_data_out, sizeof(*k_data_out));
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}
