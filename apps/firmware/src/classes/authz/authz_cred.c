/* Zugangsdaten im Sicherheitschip statt auf der Platte.
 *
 * WOFUER: Ein Server (NGC) soll seinen Schluessel nicht im Dateisystem
 * liegen lassen - `docs/plan.md`, Abschnitt "Dongle und Server (NGC)":
 * "Knotenschluessel im Chip statt auf der Platte". Der Server haelt nur ein
 * Passwort; das Langzeitgeheimnis liegt im SE05x und kommt nicht heraus.
 * Heraus kommen Sitzungsschluessel, und die sind nach dem Abziehen wertlos.
 *
 * Was den Chip verlaesst, ausdruecklich und ohne Beschoenigung:
 *   - Langzeitgeheimnis: NIE.
 *   - Sitzungsschluessel: JA. Das ist ihr Zweck, denn der Server
 *     verschluesselt damit. Wer sie abhoert, hat sie bis zum Ende der
 *     Sitzung. Der Gewinn liegt nicht darin, dass nichts herauskommt,
 *     sondern darin, dass das Langzeitgeheimnis nicht herauskommt: ein
 *     gestohlener Server gibt nichts her, was ueber seine laufende Sitzung
 *     hinausreicht.
 *
 * Ableitung: SP 800-108 Zaehlermodus, PRF ist HMAC-SHA256 aus dem Chip.
 * Warum nicht AES-CMAC wie die Vorlage am Markt: der Vertrag osk_se050.h
 * fuehrt HMAC, aber kein CMAC, und diese Schicht soll kein eigenes
 * Krypto-Grundverfahren mitbringen. Folge ist fehlende Vertraeglichkeit
 * mit fremden Programmen - gemeldet in osk_authz.h, dort steht die
 * Abwaegung. */
#include <string.h>
#include "authz_state.h"
#include "osk_mem.h"

#define CRED_VERIFIER_LEN 16u

static osk_result_t pw_verifier(osk_key_id_t secret, const uint8_t *password, size_t pw_len,
				uint8_t out[CRED_VERIFIER_LEN])
{
	uint8_t msg[sizeof(AUTHZ_CTX_PW) - 1u + OSK_AUTHZ_CRED_PW_MAX];
	uint8_t mac[OSK_AUTHZ_PROOF_LEN];
	const size_t ctx_len = sizeof(AUTHZ_CTX_PW) - 1u;

	if (pw_len < OSK_AUTHZ_CRED_PW_MIN || pw_len > OSK_AUTHZ_CRED_PW_MAX) {
		return OSK_ERR_ARG;
	}

	memcpy(msg, AUTHZ_CTX_PW, ctx_len);
	memcpy(msg + ctx_len, password, pw_len);

	size_t got = 0u;
	osk_result_t r = osk_se050_hmac(secret, msg, ctx_len + pw_len, mac, sizeof(mac), &got);
	osk_wipe(msg, sizeof(msg));
	if (r != OSK_OK) {
		osk_wipe(mac, sizeof(mac));
		return r;
	}
	if (got < CRED_VERIFIER_LEN) {
		osk_wipe(mac, sizeof(mac));
		return OSK_ERR_INTERNAL;
	}
	memcpy(out, mac, CRED_VERIFIER_LEN);
	osk_wipe(mac, sizeof(mac));
	return OSK_OK;
}

/* Ein Block der Ableitung nach SP 800-108, Zaehlermodus:
 *
 *   K(i) = PRF(K, [i]_32 || Label || 0x00 || Context || [L]_32)
 *
 * Das Nullbyte zwischen Label und Context steht nicht zur Zierde da: ohne
 * es liesse sich ein Teil des Labels als Anfang des Context lesen, und
 * zwei verschiedene Eingaben ergaeben denselben Schluessel. `L` ist die
 * Gesamtlaenge in Bit und gehoert dazu, damit eine Ableitung ueber 128 Bit
 * nicht der Anfang einer Ableitung ueber 256 Bit ist. */
static osk_result_t kdf_block(osk_key_id_t secret, uint32_t counter, const uint8_t *context,
			      size_t ctx_len, uint32_t total_bits, uint8_t out[OSK_AUTHZ_PROOF_LEN])
{
	uint8_t msg[4u + (sizeof(AUTHZ_CTX_KDF) - 1u) + 1u + 2u * OSK_AUTHZ_CHALLENGE_LEN + 4u];
	const size_t label_len = sizeof(AUTHZ_CTX_KDF) - 1u;
	size_t n = 0u;

	if (ctx_len > 2u * OSK_AUTHZ_CHALLENGE_LEN) {
		return OSK_ERR_ARG;
	}

	msg[n++] = (uint8_t)(counter >> 24);
	msg[n++] = (uint8_t)(counter >> 16);
	msg[n++] = (uint8_t)(counter >> 8);
	msg[n++] = (uint8_t)counter;
	memcpy(msg + n, AUTHZ_CTX_KDF, label_len);
	n += label_len;
	msg[n++] = 0x00u;
	memcpy(msg + n, context, ctx_len);
	n += ctx_len;
	msg[n++] = (uint8_t)(total_bits >> 24);
	msg[n++] = (uint8_t)(total_bits >> 16);
	msg[n++] = (uint8_t)(total_bits >> 8);
	msg[n++] = (uint8_t)total_bits;

	size_t got = 0u;
	osk_result_t r = osk_se050_hmac(secret, msg, n, out, OSK_AUTHZ_PROOF_LEN, &got);
	osk_wipe(msg, sizeof(msg));
	if (r != OSK_OK) {
		return r;
	}
	if (got != OSK_AUTHZ_PROOF_LEN) {
		osk_wipe(out, OSK_AUTHZ_PROOF_LEN);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

static size_t cred_slot(uint16_t id)
{
	authz_state_t *st = authz_state();

	for (size_t i = 0u; i < OSK_AUTHZ_CRED_MAX; i++) {
		if (st->creds[i].used && st->creds[i].cred.id == id) {
			return i;
		}
	}
	return OSK_AUTHZ_CRED_MAX;
}

osk_result_t osk_authz_cred_add(osk_authz_session_t session, const osk_authz_cred_t *cred,
				const uint8_t *password, size_t pw_len)
{
	if (cred == NULL || password == NULL) {
		return OSK_ERR_ARG;
	}
	if (cred->id == 0u || cred->realms == OSK_REALM_NONE) {
		return OSK_ERR_ARG;
	}
	if (pw_len < OSK_AUTHZ_CRED_PW_MIN || pw_len > OSK_AUTHZ_CRED_PW_MAX) {
		return OSK_ERR_ARG;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_CRED_MANAGE, cred->realms);
	if (r != OSK_OK) {
		return r;
	}
	if (cred_slot(cred->id) != OSK_AUTHZ_CRED_MAX) {
		return OSK_ERR_EXISTS;
	}

	authz_state_t *st = authz_state();
	for (size_t i = 0u; i < OSK_AUTHZ_CRED_MAX; i++) {
		if (st->creds[i].used) {
			continue;
		}
		uint8_t v[CRED_VERIFIER_LEN];
		r = pw_verifier(cred->secret, password, pw_len, v);
		if (r != OSK_OK) {
			osk_wipe(v, sizeof(v));
			return r;
		}
		memset(&st->creds[i], 0, sizeof(st->creds[i]));
		st->creds[i].cred = *cred;
		st->creds[i].cred.label[OSK_AUTHZ_LABEL_MAX] = '\0';
		memcpy(st->creds[i].verifier, v, CRED_VERIFIER_LEN);
		st->creds[i].used = true;
		osk_wipe(v, sizeof(v));
		return OSK_OK;
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_authz_cred_remove(osk_authz_session_t session, uint16_t cred_id)
{
	size_t i = cred_slot(cred_id);
	if (i == OSK_AUTHZ_CRED_MAX) {
		return OSK_ERR_NOT_FOUND;
	}
	authz_state_t *st = authz_state();

	osk_result_t r = osk_authz_check(session, OSK_CAP_CRED_MANAGE, st->creds[i].cred.realms);
	if (r != OSK_OK) {
		return r;
	}
	osk_wipe(&st->creds[i], sizeof(st->creds[i]));
	return OSK_OK;
}

osk_result_t osk_authz_cred_list(uint16_t *ids_out, size_t cap, size_t *count_out)
{
	if (count_out == NULL) {
		return OSK_ERR_ARG;
	}
	authz_state_t *st = authz_state();
	size_t n = 0u;

	for (size_t i = 0u; i < OSK_AUTHZ_CRED_MAX; i++) {
		if (!st->creds[i].used) {
			continue;
		}
		if (ids_out != NULL) {
			if (n >= cap) {
				return OSK_ERR_FULL;
			}
			ids_out[n] = st->creds[i].cred.id;
		}
		n++;
	}
	*count_out = n;
	return OSK_OK;
}

osk_result_t osk_authz_cred_tries_left(uint16_t cred_id, uint8_t *tries_out)
{
	if (tries_out == NULL) {
		return OSK_ERR_ARG;
	}
	size_t i = cred_slot(cred_id);
	if (i == OSK_AUTHZ_CRED_MAX) {
		return OSK_ERR_NOT_FOUND;
	}
	authz_state_t *st = authz_state();

	if (st->creds[i].locked) {
		*tries_out = 0u;
		return OSK_OK;
	}
	if (st->creds[i].cred.max_fail == 0u) {
		*tries_out = 0xFFu; /* nie sperren */
		return OSK_OK;
	}
	*tries_out = (uint8_t)(st->creds[i].cred.max_fail - st->creds[i].fails);
	return OSK_OK;
}

osk_result_t osk_authz_cred_session(osk_authz_session_t session, uint16_t cred_id,
				    const uint8_t *password, size_t pw_len,
				    const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				    const uint8_t device_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				    osk_authz_session_keys_t *keys_out)
{
	if (password == NULL || host_challenge == NULL || device_challenge == NULL ||
	    keys_out == NULL) {
		return OSK_ERR_ARG;
	}
	size_t i = cred_slot(cred_id);
	if (i == OSK_AUTHZ_CRED_MAX) {
		return OSK_ERR_NOT_FOUND;
	}
	authz_state_t *st = authz_state();
	if (st->creds[i].locked) {
		return OSK_ERR_LOCKED;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_CRED_USE, st->creds[i].cred.realms);
	if (r != OSK_OK) {
		return r;
	}

	/* Das Passwort wird GEPRUEFT, nicht nur in die Ableitung gegeben.
	 * Gaebe es nur die Ableitung, kaeme bei falschem Passwort ein
	 * falscher Sitzungsschluessel heraus, der Versuch waere unbegrenzt
	 * wiederholbar und kein Fehlversuchszaehler griffe. */
	uint8_t v[CRED_VERIFIER_LEN];
	r = pw_verifier(st->creds[i].cred.secret, password, pw_len, v);
	if (r != OSK_OK) {
		osk_wipe(v, sizeof(v));
		return r;
	}
	int bad = authz_equal_ct(v, st->creds[i].verifier, CRED_VERIFIER_LEN);
	osk_wipe(v, sizeof(v));

	if (bad != 0) {
		if (st->creds[i].cred.max_fail != 0u) {
			if (st->creds[i].fails < 0xFFu) {
				st->creds[i].fails++;
			}
			if (st->creds[i].fails >= st->creds[i].cred.max_fail) {
				st->creds[i].locked = true;
			}
		}
		return OSK_ERR_DENIED;
	}

	/* Beide Herausforderungen gehen in den Context: so haengt das Paar
	 * an dieser einen Sitzung und nicht am Passwort allein. */
	uint8_t ctx[2u * OSK_AUTHZ_CHALLENGE_LEN];
	memcpy(ctx, device_challenge, OSK_AUTHZ_CHALLENGE_LEN);
	memcpy(ctx + OSK_AUTHZ_CHALLENGE_LEN, host_challenge, OSK_AUTHZ_CHALLENGE_LEN);

	/* Zwei Schluessel, 128 Bit je, aus zwei Blocken mit verschiedenem
	 * Zaehler. Beide aus demselben Block zu schneiden waere kuerzer und
	 * falsch: dann waere der eine aus dem anderen zu errechnen, sobald
	 * ein Teil des Blocks bekannt ist. */
	const uint32_t total_bits = 2u * OSK_AUTHZ_SESSION_KEY_LEN * 8u;
	uint8_t b[OSK_AUTHZ_PROOF_LEN];

	r = kdf_block(st->creds[i].cred.secret, 1u, ctx, sizeof(ctx), total_bits, b);
	if (r == OSK_OK) {
		memcpy(keys_out->enc, b, OSK_AUTHZ_SESSION_KEY_LEN);
		osk_wipe(b, sizeof(b));
		r = kdf_block(st->creds[i].cred.secret, 2u, ctx, sizeof(ctx), total_bits, b);
	}
	if (r == OSK_OK) {
		memcpy(keys_out->mac, b, OSK_AUTHZ_SESSION_KEY_LEN);
	} else {
		osk_wipe(keys_out, sizeof(*keys_out));
	}
	osk_wipe(b, sizeof(b));
	osk_wipe(ctx, sizeof(ctx));

	if (r == OSK_OK) {
		st->creds[i].fails = 0u;
	}
	return r;
}
