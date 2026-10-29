/* Rechteschicht: Zustand, Anmeldung, und die Pruefung selbst.
 *
 * Die Pruefung ist der Kern der ganzen Schicht und steht darum in einer
 * einzigen Funktion, osk_authz_check(). Jeder Vorgang geht durch sie. Eine
 * zweite Stelle, an der ueber eine Freigabe entschieden wird, waere der
 * Anfang vom Ende - dann gibt es zwei Antworten auf dieselbe Frage. */
#include <string.h>
#include "authz_state.h"
#include "osk_mem.h"

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#endif

static authz_state_t g_state;

authz_state_t *authz_state(void)
{
	return &g_state;
}

uint32_t authz_device_ms(void)
{
#ifdef __ZEPHYR__
	return k_uptime_get_32();
#else
	/* Am PC gibt es keine Geraetezeit. Null ist hier die ehrliche
	 * Antwort: der Pruefrahmen soll nicht behaupten, er haette eine. */
	return 0u;
#endif
}

int authz_equal_ct(const void *a, const void *b, size_t len)
{
	const uint8_t *x = (const uint8_t *)a;
	const uint8_t *y = (const uint8_t *)b;
	uint8_t diff = 0u;

	for (size_t i = 0u; i < len; i++) {
		diff |= (uint8_t)(x[i] ^ y[i]);
	}
	return diff == 0u ? 0 : 1;
}

osk_authz_key_info_t *authz_find_key(uint16_t key_id)
{
	if (key_id == 0u) {
		return NULL;
	}
	for (size_t i = 0u; i < OSK_AUTHZ_KEY_MAX; i++) {
		if (g_state.keys[i].state != OSK_AUTHZ_FREE &&
		    g_state.keys[i].key.id == key_id) {
			return &g_state.keys[i];
		}
	}
	return NULL;
}

authz_session_slot_t *authz_session_live(osk_authz_session_t s)
{
	if (s >= OSK_AUTHZ_SESSION_MAX) {
		return NULL;
	}
	authz_session_slot_t *sl = &g_state.sessions[s];
	if (!sl->used || !sl->authenticated) {
		return NULL;
	}
	return sl;
}

/* ------------------------------------------------------- Beweis rechnen */

/* HMAC(K, ctx || device_challenge || host_challenge).
 *
 * Die Reihenfolge ist fest und beide Herausforderungen sind drin: nur die
 * des Geraets verhindert, dass ein alter Beweis noch einmal gilt, und nur
 * die des Rechners verhindert, dass ein untergeschobenes Geraet den
 * Rechner auf eine Herausforderung festnagelt, zu der es den Beweis schon
 * kennt. Eine von beiden allein reicht nicht. */
osk_result_t authz_proof(osk_key_id_t secret, const char *ctx,
			 const uint8_t device_challenge[OSK_AUTHZ_CHALLENGE_LEN],
			 const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
			 uint8_t out[OSK_AUTHZ_PROOF_LEN])
{
	uint8_t msg[32u + 2u * OSK_AUTHZ_CHALLENGE_LEN];
	size_t ctx_len = strlen(ctx);

	if (ctx_len > 32u) {
		return OSK_ERR_INTERNAL;
	}

	memcpy(msg, ctx, ctx_len);
	memcpy(msg + ctx_len, device_challenge, OSK_AUTHZ_CHALLENGE_LEN);
	memcpy(msg + ctx_len + OSK_AUTHZ_CHALLENGE_LEN, host_challenge, OSK_AUTHZ_CHALLENGE_LEN);

	size_t got = 0u;
	osk_result_t r = osk_se050_hmac(secret, msg, ctx_len + 2u * OSK_AUTHZ_CHALLENGE_LEN, out,
					OSK_AUTHZ_PROOF_LEN, &got);
	osk_wipe(msg, sizeof(msg));
	if (r != OSK_OK) {
		return r;
	}
	/* Eine kuerzere Antwort ist kein brauchbarer Beweis, und sie
	 * stillschweigend zu nehmen hiesse, den Rest des Puffers als Beweis
	 * mitzuzaehlen. */
	if (got != OSK_AUTHZ_PROOF_LEN) {
		osk_wipe(out, OSK_AUTHZ_PROOF_LEN);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

/* ------------------------------------------------------------- Anmelden */

osk_result_t osk_authz_init(osk_key_id_t admin_secret)
{
	memset(&g_state, 0, sizeof(g_state));

	/* Der erste Zugangsschluessel. Er hat alle Bereiche und alle
	 * Faehigkeiten und darf alles weitergeben - sonst liesse sich kein
	 * zweiter eintragen. Sein Geheimnis liegt im Chip; eine fest
	 * eingebaute Kennung waere auf jedem Geraet dieselbe. */
	osk_authz_key_info_t *a = &g_state.keys[0];
	a->state = OSK_AUTHZ_ACTIVE;
	a->key.id = 1u;
	memcpy(a->key.label, "admin", sizeof("admin"));
	a->key.secret = admin_secret;
	a->key.realms = OSK_REALM_ALL;
	a->key.caps = OSK_CAP_DEFINED;
	a->key.delegated = OSK_CAP_DEFINED;
	a->key.max_fail = 8u;
	a->key.needs_touch = true;

	g_state.ready = true;
	return OSK_OK;
}

void osk_authz_shutdown(void)
{
	osk_wipe(&g_state, sizeof(g_state));
}

osk_result_t osk_authz_session_begin(uint16_t key_id,
				     const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				     uint8_t device_challenge_out[OSK_AUTHZ_CHALLENGE_LEN],
				     osk_authz_session_t *session_out)
{
	if (host_challenge == NULL || device_challenge_out == NULL || session_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!g_state.ready) {
		return OSK_ERR_INTERNAL;
	}
	*session_out = OSK_AUTHZ_NO_SESSION;

	osk_authz_key_info_t *info = authz_find_key(key_id);
	if (info == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	/* Gesperrt ist nicht dasselbe wie nicht vorhanden, und es wird auch
	 * nicht dasselbe gemeldet. Wer das zusammenwirft, kann einen
	 * gesperrten Schluessel nicht von einem geloeschten unterscheiden. */
	if (info->state == OSK_AUTHZ_LOCKED) {
		return OSK_ERR_LOCKED;
	}

	size_t slot = OSK_AUTHZ_SESSION_MAX;
	for (size_t i = 0u; i < OSK_AUTHZ_SESSION_MAX; i++) {
		if (!g_state.sessions[i].used) {
			slot = i;
			break;
		}
	}
	if (slot == OSK_AUTHZ_SESSION_MAX) {
		return OSK_ERR_FULL;
	}

	authz_session_slot_t *sl = &g_state.sessions[slot];
	memset(sl, 0, sizeof(*sl));

	osk_result_t r = osk_se050_random(sl->device_challenge, OSK_AUTHZ_CHALLENGE_LEN);
	if (r != OSK_OK) {
		/* Ohne eigenen Zufall keine Anmeldung. Ein ersatzweise
		 * ausgedachter Wert waere vorhersehbar, und damit waere die
		 * Wiederverwendung eines alten Beweises wieder moeglich -
		 * genau das, was die Herausforderung verhindern soll. */
		osk_wipe(sl, sizeof(*sl));
		return r;
	}

	memcpy(sl->host_challenge, host_challenge, OSK_AUTHZ_CHALLENGE_LEN);
	memcpy(device_challenge_out, sl->device_challenge, OSK_AUTHZ_CHALLENGE_LEN);
	sl->used = true;
	sl->authenticated = false;
	sl->key_id = key_id;
	*session_out = (osk_authz_session_t)slot;
	return OSK_OK;
}

osk_result_t osk_authz_session_authenticate(osk_authz_session_t session,
					    const uint8_t host_proof[OSK_AUTHZ_PROOF_LEN],
					    uint8_t device_proof_out[OSK_AUTHZ_PROOF_LEN])
{
	if (host_proof == NULL || device_proof_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (session >= OSK_AUTHZ_SESSION_MAX) {
		return OSK_ERR_ARG;
	}
	authz_session_slot_t *sl = &g_state.sessions[session];
	if (!sl->used) {
		return OSK_ERR_NOT_FOUND;
	}
	/* Zweimal anmelden gilt nicht: sonst liesse sich an einer
	 * angemeldeten Sitzung beliebig oft raten, ohne dass die
	 * Fehlversuche je zu einer Sperre fuehren. */
	if (sl->authenticated) {
		return OSK_ERR_EXISTS;
	}

	osk_authz_key_info_t *info = authz_find_key(sl->key_id);
	if (info == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (info->state == OSK_AUTHZ_LOCKED) {
		return OSK_ERR_LOCKED;
	}

	uint8_t want[OSK_AUTHZ_PROOF_LEN];
	osk_result_t r = authz_proof(info->key.secret, AUTHZ_CTX_HOST, sl->device_challenge,
				     sl->host_challenge, want);
	if (r != OSK_OK) {
		return r;
	}

	int bad = authz_equal_ct(host_proof, want, OSK_AUTHZ_PROOF_LEN);
	osk_wipe(want, sizeof(want));

	if (bad != 0) {
		if (info->key.max_fail != 0u) {
			if (info->fails < 0xFFu) {
				info->fails++;
			}
			if (info->fails >= info->key.max_fail) {
				info->state = OSK_AUTHZ_LOCKED;
			}
		}
		/* Die Sitzung ist verbraucht. Sonst waere die Herausforderung
		 * des Geraets fuer einen zweiten Versuch dieselbe, und damit
		 * waere sie keine. */
		osk_wipe(sl, sizeof(*sl));
		return OSK_ERR_DENIED;
	}

	r = authz_proof(info->key.secret, AUTHZ_CTX_DEVICE, sl->device_challenge,
			sl->host_challenge, device_proof_out);
	if (r != OSK_OK) {
		osk_wipe(sl, sizeof(*sl));
		return r;
	}

	info->fails = 0u;
	if (info->uses < 0xFFFFFFFFu) {
		info->uses++;
	}
	sl->authenticated = true;
	return OSK_OK;
}

void osk_authz_session_end(osk_authz_session_t session)
{
	if (session >= OSK_AUTHZ_SESSION_MAX) {
		return;
	}
	osk_wipe(&g_state.sessions[session], sizeof(g_state.sessions[session]));
}

void osk_authz_session_end_all(void)
{
	osk_wipe(g_state.sessions, sizeof(g_state.sessions));
}

/* ----------------------------------------------------------- Die Pruefung */

osk_result_t osk_authz_check(osk_authz_session_t session, osk_cap_mask_t need,
			     osk_realm_mask_t object_realms)
{
	/* Eine Pruefung ohne verlangtes Recht waere eine Pruefung, die
	 * immer ja sagt. Das ist ein Fehler im Aufrufer, nicht eine
	 * Freigabe. */
	if (need == OSK_CAP_NONE) {
		return OSK_ERR_ARG;
	}
	if ((need & ~OSK_CAP_DEFINED) != 0u) {
		return OSK_ERR_ARG;
	}

	authz_session_slot_t *sl = authz_session_live(session);
	if (sl == NULL) {
		return OSK_ERR_DENIED;
	}
	osk_authz_key_info_t *info = authz_find_key(sl->key_id);
	if (info == NULL || info->state != OSK_AUTHZ_ACTIVE) {
		return OSK_ERR_DENIED;
	}

	/* Erste Haelfte: darf er diese Art von Vorgang ueberhaupt?
	 * ALLE verlangten Bits muessen da sein, nicht eines davon. */
	if ((info->key.caps & need) != need) {
		return OSK_ERR_DENIED;
	}

	/* Zweite Haelfte, und das ist die Trennung: ein Gegenstand ohne
	 * Bereich gehoert niemandem und ist fuer niemanden erreichbar. Eine
	 * leere Maske als "ueberall" zu lesen waere der stille
	 * Rechteverlust, vor dem der Vertrag warnt. */
	if (object_realms == OSK_REALM_NONE) {
		return OSK_ERR_DENIED;
	}
	if ((info->key.realms & object_realms) == 0u) {
		return OSK_ERR_DENIED;
	}

	return OSK_OK;
}

osk_result_t osk_authz_session_identity(osk_authz_session_t session, uint16_t *key_id_out,
					const char **label_out)
{
	authz_session_slot_t *sl = authz_session_live(session);
	if (sl == NULL) {
		return OSK_ERR_DENIED;
	}
	osk_authz_key_info_t *info = authz_find_key(sl->key_id);
	if (info == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (key_id_out != NULL) {
		*key_id_out = info->key.id;
	}
	if (label_out != NULL) {
		*label_out = info->key.label;
	}
	return OSK_OK;
}
