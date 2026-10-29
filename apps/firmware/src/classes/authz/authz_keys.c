/* Zugangsschluessel eintragen, entfernen, nachsehen.
 *
 * Der ganze Sinn dieser Datei steht in einer Regel: wer Zugangsschluessel
 * anlegen darf, darf dabei nicht mehr weitergeben, als in seiner
 * `delegated`-Maske steht. Ohne sie legt sich jeder Verwalter einen
 * zweiten Schluessel mit mehr Rechten an und die Rechtetrennung ist eine
 * Hoeflichkeit. Siehe osk_authz.h. */
#include <string.h>
#include "authz_state.h"
#include "osk_mem.h"

/* Ein Name wird angezeigt und protokolliert. Er darf darum nur
 * Druckzeichen enthalten: ein Steuerzeichen im Protokoll kann eine Zeile
 * so aussehen lassen, als stuende etwas anderes darin. */
static bool label_ok(const char *s)
{
	size_t n = 0u;

	if (s == NULL || s[0] == '\0') {
		return false;
	}
	while (s[n] != '\0') {
		unsigned char c = (unsigned char)s[n];
		if (c < 0x20u || c > 0x7Eu) {
			return false;
		}
		n++;
		if (n > OSK_AUTHZ_LABEL_MAX) {
			return false;
		}
	}
	return true;
}

osk_result_t osk_authz_key_add(osk_authz_session_t session, const osk_authz_key_t *key)
{
	if (key == NULL) {
		return OSK_ERR_ARG;
	}
	if (key->id == 0u || !label_ok(key->label)) {
		return OSK_ERR_ARG;
	}
	/* Bits, die keine Faehigkeit bezeichnen, werden abgewiesen statt
	 * mitgefuehrt - sonst bekommt dieser Schluessel das naechste neue
	 * Recht geschenkt, sobald es vergeben wird. */
	if ((key->caps & ~OSK_CAP_DEFINED) != 0u || (key->delegated & ~OSK_CAP_DEFINED) != 0u) {
		return OSK_ERR_ARG;
	}
	/* Weitergeben, was man selbst nicht hat, ist kein Sonderfall,
	 * sondern derselbe Aufstieg nur ueber zwei Schritte. */
	if ((key->delegated & ~key->caps) != 0u) {
		return OSK_ERR_ARG;
	}
	if (key->realms == OSK_REALM_NONE) {
		return OSK_ERR_ARG;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_AUTHZ_MANAGE, key->realms);
	if (r != OSK_OK) {
		return r;
	}

	authz_state_t *st = authz_state();
	authz_session_slot_t *sl = authz_session_live(session);
	osk_authz_key_info_t *me = authz_find_key(sl->key_id);
	if (me == NULL) {
		return OSK_ERR_DENIED;
	}

	/* Die eigentliche Schranke, zweimal:
	 *   - der Neue bekommt hoechstens die `delegated` seines Erzeugers,
	 *   - und seine eigene `delegated` kann nicht groesser sein.
	 * Damit schrumpft die Rechtemenge ueber jede Kette hinweg und
	 * waechst nie. */
	if ((key->caps & ~me->key.delegated) != 0u) {
		return OSK_ERR_DENIED;
	}
	/* Bereiche genauso: in einem Bereich, den der Erzeuger nicht hat,
	 * kann er keinen Zugang einrichten. */
	if ((key->realms & ~me->key.realms) != 0u) {
		return OSK_ERR_DENIED;
	}

	if (authz_find_key(key->id) != NULL) {
		return OSK_ERR_EXISTS;
	}

	for (size_t i = 0u; i < OSK_AUTHZ_KEY_MAX; i++) {
		if (st->keys[i].state == OSK_AUTHZ_FREE) {
			memset(&st->keys[i], 0, sizeof(st->keys[i]));
			st->keys[i].key = *key;
			st->keys[i].key.label[OSK_AUTHZ_LABEL_MAX] = '\0';
			st->keys[i].state = OSK_AUTHZ_ACTIVE;
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_authz_key_remove(osk_authz_session_t session, uint16_t key_id)
{
	osk_authz_key_info_t *victim = authz_find_key(key_id);
	if (victim == NULL) {
		return OSK_ERR_NOT_FOUND;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_AUTHZ_MANAGE, victim->key.realms);
	if (r != OSK_OK) {
		return r;
	}

	authz_session_slot_t *sl = authz_session_live(session);
	/* Sich selbst zu entfernen, waehrend man angemeldet ist, laesst eine
	 * Sitzung ohne Identitaet zurueck. Die Pruefung findet den Eintrag
	 * dann nicht mehr und weist alles ab - aber dieser Zustand soll
	 * nicht erst entstehen, denn er sieht aus wie ein Fehler. */
	if (sl->key_id == key_id) {
		return OSK_ERR_DENIED;
	}

	authz_state_t *st = authz_state();
	/* Die Sitzungen des Entfernten enden mit ihm. Eine, die
	 * weiterlaeuft, waere ein Zugang ohne Zugangsschluessel. */
	for (size_t i = 0u; i < OSK_AUTHZ_SESSION_MAX; i++) {
		if (st->sessions[i].used && st->sessions[i].key_id == key_id) {
			osk_wipe(&st->sessions[i], sizeof(st->sessions[i]));
		}
	}
	osk_wipe(victim, sizeof(*victim));
	return OSK_OK;
}

osk_result_t osk_authz_key_unlock(osk_authz_session_t session, uint16_t key_id)
{
	osk_authz_key_info_t *info = authz_find_key(key_id);
	if (info == NULL) {
		return OSK_ERR_NOT_FOUND;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_AUTHZ_MANAGE, info->key.realms);
	if (r != OSK_OK) {
		return r;
	}
	if (info->state != OSK_AUTHZ_LOCKED) {
		return OSK_ERR_ARG;
	}
	info->state = OSK_AUTHZ_ACTIVE;
	info->fails = 0u;
	return OSK_OK;
}

/* Nachsehen braucht keine Sitzung: hier steht kein Geheimnis. Was
 * herauskommt, ist Name, Rechte und Zustand - also genau das, was der
 * Verwalter sehen muss, um zu entscheiden, bei wem er sich anmeldet. */
osk_result_t osk_authz_key_info(uint16_t key_id, osk_authz_key_info_t *info_out)
{
	if (info_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_authz_key_info_t *info = authz_find_key(key_id);
	if (info == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	*info_out = *info;
	return OSK_OK;
}

osk_result_t osk_authz_key_list(uint16_t *ids_out, size_t cap, size_t *count_out)
{
	if (count_out == NULL) {
		return OSK_ERR_ARG;
	}
	authz_state_t *st = authz_state();
	size_t n = 0u;

	for (size_t i = 0u; i < OSK_AUTHZ_KEY_MAX; i++) {
		if (st->keys[i].state == OSK_AUTHZ_FREE) {
			continue;
		}
		if (ids_out != NULL) {
			if (n >= cap) {
				return OSK_ERR_FULL;
			}
			ids_out[n] = st->keys[i].key.id;
		}
		n++;
	}
	*count_out = n;
	return OSK_OK;
}
