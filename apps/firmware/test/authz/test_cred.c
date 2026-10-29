/* Zugangsdaten im Chip: Sitzungsschluessel holen, Passwort pruefen.
 *
 * Der wichtigste Prueffall ist der Fehlversuchszaehler. Ohne ihn waere die
 * Ableitung bei falschem Passwort einfach falsch - und damit unbegrenzt
 * oft zu wiederholen, bis das Passwort geraten ist. */
#include <string.h>
#include "authz_t.h"

#define T_CRED_ID 7u

static const uint8_t PW_GOOD[] = "richtig-genug";
static const uint8_t PW_BAD[] = "falsch-genug!";

static osk_authz_session_t setup_cred(osk_cap_mask_t caps, osk_realm_mask_t cred_realms)
{
	t_setup();
	osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
	osk_authz_cred_t c;

	memset(&c, 0, sizeof(c));
	c.id = T_CRED_ID;
	T_LABEL(c.label, "ngc-knoten-1");
	c.secret = T_SECRET_CRED;
	c.realms = cred_realms;
	c.max_fail = 3u;
	TT_EQ_INT(osk_authz_cred_add(admin, &c, PW_GOOD, sizeof(PW_GOOD) - 1u), OSK_OK,
		  "Zugangsdaten anlegen");

	osk_authz_key_t k;
	memset(&k, 0, sizeof(k));
	k.id = T_KEY_WEB;
	T_LABEL(k.label, "ngc-server");
	k.secret = T_SECRET_WEB;
	k.realms = T_REALM_WEB;
	k.caps = caps;
	k.max_fail = 5u;
	(void)osk_authz_key_add(admin, &k);
	osk_authz_session_end(admin);

	return t_login(T_KEY_WEB, T_SECRET_WEB);
}

/* Die Ableitung aus osk_authz.h nachrechnen - SP 800-108 Zaehlermodus,
 * PRF HMAC-SHA256. Nicht aus dem Code gelesen, sondern aus der
 * Beschreibung gebaut: so prueft der Fall, ob der Code tut, was der
 * Vertrag sagt, und nicht, ob er sich selbst gleicht. */
static void kdf_expect(uint32_t counter, const uint8_t *ctx, size_t ctx_len, uint8_t out[32])
{
	const char *label = "osk-authz-session-1";
	uint8_t msg[128];
	size_t n = 0u;
	const uint32_t total_bits = 2u * 16u * 8u;
	const size_t ll = strlen(label);

	msg[n++] = (uint8_t)(counter >> 24);
	msg[n++] = (uint8_t)(counter >> 16);
	msg[n++] = (uint8_t)(counter >> 8);
	msg[n++] = (uint8_t)counter;
	memcpy(msg + n, label, ll);
	n += ll;
	msg[n++] = 0x00u;
	memcpy(msg + n, ctx, ctx_len);
	n += ctx_len;
	msg[n++] = (uint8_t)(total_bits >> 24);
	msg[n++] = (uint8_t)(total_bits >> 16);
	msg[n++] = (uint8_t)(total_bits >> 8);
	msg[n++] = (uint8_t)total_bits;

	(void)fake_hmac_of(T_SECRET_CRED, msg, n, out);
}

void test_cred(void)
{
	uint8_t hc[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t dc[OSK_AUTHZ_CHALLENGE_LEN];

	memset(hc, 0xA1, sizeof(hc));
	memset(dc, 0xB2, sizeof(dc));

	TT_CASE("Sitzungsschluessel holen, und die Ableitung ist die versprochene");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WEB);
		osk_authz_session_keys_t keys;
		memset(&keys, 0, sizeof(keys));

		TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc,
						 dc, &keys),
			  OSK_OK, "Schluesselpaar holen");

		/* Zwei verschiedene Schluessel. Beide aus demselben Block zu
		 * schneiden waere kuerzer und falsch: dann waere der eine aus
		 * dem anderen zu errechnen. */
		TT_TRUE(memcmp(keys.enc, keys.mac, OSK_AUTHZ_SESSION_KEY_LEN) != 0,
			"enc und mac sind verschieden");

		uint8_t ctx[2u * OSK_AUTHZ_CHALLENGE_LEN];
		memcpy(ctx, dc, OSK_AUTHZ_CHALLENGE_LEN);
		memcpy(ctx + OSK_AUTHZ_CHALLENGE_LEN, hc, OSK_AUTHZ_CHALLENGE_LEN);

		uint8_t want[32];
		kdf_expect(1u, ctx, sizeof(ctx), want);
		TT_EQ_MEM(keys.enc, want, OSK_AUTHZ_SESSION_KEY_LEN, "enc nach SP 800-108, i=1");
		kdf_expect(2u, ctx, sizeof(ctx), want);
		TT_EQ_MEM(keys.mac, want, OSK_AUTHZ_SESSION_KEY_LEN, "mac nach SP 800-108, i=2");
		osk_authz_session_end(s);
	}

	TT_CASE("andere Herausforderung, andere Schluessel");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WEB);
		osk_authz_session_keys_t a, b;
		uint8_t dc2[OSK_AUTHZ_CHALLENGE_LEN];
		memset(dc2, 0xB3, sizeof(dc2));

		(void)osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc, dc,
					     &a);
		(void)osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc, dc2,
					     &b);
		/* Haengte das Paar nur am Passwort, waere es bei jeder
		 * Sitzung dasselbe - und ein einmal abgehoerter
		 * Sitzungsschluessel gaelte fuer immer. */
		TT_TRUE(memcmp(a.enc, b.enc, OSK_AUTHZ_SESSION_KEY_LEN) != 0,
			"enc haengt an der Herausforderung");
		TT_TRUE(memcmp(a.mac, b.mac, OSK_AUTHZ_SESSION_KEY_LEN) != 0,
			"mac haengt an der Herausforderung");
		osk_authz_session_end(s);
	}

	TT_CASE("DER PRUEFFALL: falsches Passwort zaehlt und sperrt");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WEB);
		osk_authz_session_keys_t keys;
		uint8_t left = 0u;

		TT_EQ_INT(osk_authz_cred_tries_left(T_CRED_ID, &left), OSK_OK, "Versuche lesen");
		TT_EQ_INT(left, 3, "drei zu Anfang");

		for (int i = 1; i <= 3; i++) {
			TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_BAD, sizeof(PW_BAD) - 1u,
							 hc, dc, &keys),
				  OSK_ERR_DENIED, "falsches Passwort wird ERKANNT");
			(void)osk_authz_cred_tries_left(T_CRED_ID, &left);
			TT_EQ_INT(left, 3 - i, "Versuche gezaehlt");
		}

		/* Und jetzt hilft auch das richtige nicht mehr. Ohne diese
		 * Sperre waere das Passwort in Ruhe zu erraten. */
		TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc,
						 dc, &keys),
			  OSK_ERR_LOCKED, "gesperrt");
		osk_authz_session_end(s);
	}

	TT_CASE("richtiges Passwort setzt die Fehlversuche zurueck");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WEB);
		osk_authz_session_keys_t keys;
		uint8_t left = 0u;

		(void)osk_authz_cred_session(s, T_CRED_ID, PW_BAD, sizeof(PW_BAD) - 1u, hc, dc,
					     &keys);
		(void)osk_authz_cred_tries_left(T_CRED_ID, &left);
		TT_EQ_INT(left, 2, "einer verbraucht");
		TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc,
						 dc, &keys),
			  OSK_OK, "richtig");
		(void)osk_authz_cred_tries_left(T_CRED_ID, &left);
		TT_EQ_INT(left, 3, "wieder drei");
		osk_authz_session_end(s);
	}

	TT_CASE("ohne CRED_USE gibt es keine Schluessel");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_SIGN, T_REALM_WEB);
		osk_authz_session_keys_t keys;
		TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc,
						 dc, &keys),
			  OSK_ERR_DENIED, "Faehigkeit fehlt");
		osk_authz_session_end(s);
	}

	TT_CASE("Zugangsdaten in einem fremden Bereich sind unerreichbar");
	{
		/* Dieselben Zugangsdaten, aber im Bereich der Geldboerse. Der
		 * Server hat Bereich 1 - also kommt er nicht daran, obwohl
		 * er CRED_USE hat und das Passwort kennt. */
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WALLET);
		osk_authz_session_keys_t keys;
		TT_EQ_INT(osk_authz_cred_session(s, T_CRED_ID, PW_GOOD, sizeof(PW_GOOD) - 1u, hc,
						 dc, &keys),
			  OSK_ERR_DENIED, "fremder Bereich, trotz richtigem Passwort");

		/* Und der Fehlversuchszaehler darf davon nicht beruehrt
		 * werden - sonst sperrt ein Unberechtigter die Zugangsdaten
		 * eines anderen, ohne das Passwort zu kennen. Das waere eine
		 * Betriebsstoerung auf Zuruf. */
		uint8_t left = 0u;
		(void)osk_authz_cred_tries_left(T_CRED_ID, &left);
		TT_EQ_INT(left, 3, "Zaehler unberuehrt");
		osk_authz_session_end(s);
	}

	TT_CASE("Anlegen und Entfernen brauchen CRED_MANAGE");
	{
		osk_authz_session_t s = setup_cred(OSK_CAP_CRED_USE, T_REALM_WEB);
		osk_authz_cred_t c;
		memset(&c, 0, sizeof(c));
		c.id = 9u;
		T_LABEL(c.label, "noch-einer");
		c.secret = T_SECRET_CRED;
		c.realms = T_REALM_WEB;
		c.max_fail = 3u;
		TT_EQ_INT(osk_authz_cred_add(s, &c, PW_GOOD, sizeof(PW_GOOD) - 1u), OSK_ERR_DENIED,
			  "anlegen ohne CRED_MANAGE");
		TT_EQ_INT(osk_authz_cred_remove(s, T_CRED_ID), OSK_ERR_DENIED,
			  "entfernen ohne CRED_MANAGE");
		osk_authz_session_end(s);
	}

	TT_CASE("Passwortlaengen und doppelte Kennungen");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_cred_t c;
		memset(&c, 0, sizeof(c));
		c.id = T_CRED_ID;
		T_LABEL(c.label, "x");
		c.secret = T_SECRET_CRED;
		c.realms = T_REALM_WEB;
		c.max_fail = 3u;

		TT_EQ_INT(osk_authz_cred_add(admin, &c, PW_GOOD, 3u), OSK_ERR_ARG, "zu kurz");
		TT_EQ_INT(osk_authz_cred_add(admin, &c, PW_GOOD, OSK_AUTHZ_CRED_PW_MAX + 1u),
			  OSK_ERR_ARG, "zu lang");
		TT_EQ_INT(osk_authz_cred_add(admin, &c, PW_GOOD, sizeof(PW_GOOD) - 1u), OSK_OK,
			  "gerade richtig");
		TT_EQ_INT(osk_authz_cred_add(admin, &c, PW_GOOD, sizeof(PW_GOOD) - 1u),
			  OSK_ERR_EXISTS, "Kennung schon da");

		uint16_t ids[OSK_AUTHZ_CRED_MAX];
		size_t n = 0u;
		TT_EQ_INT(osk_authz_cred_list(ids, OSK_AUTHZ_CRED_MAX, &n), OSK_OK, "aufzaehlen");
		TT_EQ_INT(n, 1, "eine");
		TT_EQ_INT(osk_authz_cred_remove(admin, T_CRED_ID), OSK_OK, "entfernen");
		TT_EQ_INT(osk_authz_cred_list(ids, OSK_AUTHZ_CRED_MAX, &n), OSK_OK, "aufzaehlen");
		TT_EQ_INT(n, 0, "keine");
		uint8_t left = 0u;
		TT_EQ_INT(osk_authz_cred_tries_left(T_CRED_ID, &left), OSK_ERR_NOT_FOUND,
			  "entfernte Zugangsdaten");
		osk_authz_session_end(admin);
	}
}
