/* Anmeldung und Verwaltung der Zugangsschluessel.
 *
 * Zwei Dinge stehen hier im Mittelpunkt, und beide sind Wege, auf denen
 * eine Rechtetrennung von innen aufgeht:
 *   - ein geratener Beweis darf nicht beliebig oft wiederholbar sein,
 *   - wer Zugangsschluessel anlegen darf, darf sich damit nicht mehr
 *     Rechte verschaffen, als er selbst hat. */
#include <string.h>
#include "authz_t.h"

void test_keys(void)
{
	TT_CASE("gegenseitig: der Beweis des Geraets ist nicht der des Rechners");
	{
		t_setup();
		uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t dev_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t msg[64];
		uint8_t host_proof[32];
		uint8_t dev_proof[OSK_AUTHZ_PROOF_LEN];
		uint8_t want_dev[32];
		osk_authz_session_t s = OSK_AUTHZ_NO_SESSION;

		memset(host_chal, 0x5A, sizeof(host_chal));
		TT_EQ_INT(osk_authz_session_begin(T_KEY_ADMIN, host_chal, dev_chal, &s), OSK_OK,
			  "beginnen");

		size_t n = strlen("osk-authz-host-1");
		memcpy(msg, "osk-authz-host-1", n);
		memcpy(msg + n, dev_chal, OSK_AUTHZ_CHALLENGE_LEN);
		memcpy(msg + n + OSK_AUTHZ_CHALLENGE_LEN, host_chal, OSK_AUTHZ_CHALLENGE_LEN);
		TT_EQ_INT(fake_hmac_of(T_SECRET_ADMIN, msg, n + 32u, host_proof), 0, "HMAC rechnen");

		TT_EQ_INT(osk_authz_session_authenticate(s, host_proof, dev_proof), OSK_OK,
			  "anmelden");

		/* Der Beweis des Geraets muss aus einer anderen
		 * Trennzeichenkette kommen. Waeren beide gleich, koennte ein
		 * Angreifer den Beweis des Rechners abhoeren und ihn als
		 * Beweis des Geraets wieder einspielen - und ein
		 * untergeschobenes Geraet braeuchte das Geheimnis nicht. */
		TT_TRUE(memcmp(dev_proof, host_proof, OSK_AUTHZ_PROOF_LEN) != 0,
			"die beiden Beweise sind verschieden");

		memcpy(msg, "osk-authz-dev-1", strlen("osk-authz-dev-1"));
		n = strlen("osk-authz-dev-1");
		memcpy(msg + n, dev_chal, OSK_AUTHZ_CHALLENGE_LEN);
		memcpy(msg + n + OSK_AUTHZ_CHALLENGE_LEN, host_chal, OSK_AUTHZ_CHALLENGE_LEN);
		TT_EQ_INT(fake_hmac_of(T_SECRET_ADMIN, msg, n + 32u, want_dev), 0, "HMAC rechnen");
		TT_EQ_MEM(dev_proof, want_dev, OSK_AUTHZ_PROOF_LEN,
			  "Beweis des Geraets ist nachrechenbar");

		/* Zweimal anmelden gilt nicht - sonst waere an einer
		 * angemeldeten Sitzung beliebig oft zu raten, ohne dass ein
		 * Fehlversuch gezaehlt wird. */
		TT_EQ_INT(osk_authz_session_authenticate(s, host_proof, dev_proof), OSK_ERR_EXISTS,
			  "zweite Anmeldung derselben Sitzung");
		osk_authz_session_end(s);
	}

	TT_CASE("die Herausforderung des Geraets ist jedes Mal eine andere");
	{
		t_setup();
		uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t a[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t b[OSK_AUTHZ_CHALLENGE_LEN];
		osk_authz_session_t s1 = OSK_AUTHZ_NO_SESSION;
		osk_authz_session_t s2 = OSK_AUTHZ_NO_SESSION;

		memset(host_chal, 0x01, sizeof(host_chal));
		(void)osk_authz_session_begin(T_KEY_ADMIN, host_chal, a, &s1);
		(void)osk_authz_session_begin(T_KEY_ADMIN, host_chal, b, &s2);
		/* Gleiche Herausforderung des Rechners, verschiedene des
		 * Geraets: nur so ist ein abgehoerter Beweis nicht noch
		 * einmal brauchbar. */
		TT_TRUE(memcmp(a, b, sizeof(a)) != 0, "zwei Sitzungen, zwei Herausforderungen");
		osk_authz_session_end(s1);
		osk_authz_session_end(s2);
	}

	TT_CASE("ein abgehoerter Beweis gilt nicht noch einmal");
	{
		t_setup();
		osk_authz_session_t s = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		TT_TRUE(s != OSK_AUTHZ_NO_SESSION, "erste Anmeldung");

		/* t_login() rechnet den Beweis gegen die Herausforderung der
		 * EIGENEN Sitzung. Derselbe Beweis an einer neuen Sitzung
		 * trifft eine andere Herausforderung und muss scheitern. */
		uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t dev_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t msg[64];
		uint8_t old_proof[32];
		osk_authz_session_t s2 = OSK_AUTHZ_NO_SESSION;

		memset(host_chal, 0xC3, sizeof(host_chal));
		(void)osk_authz_session_begin(T_KEY_ADMIN, host_chal, dev_chal, &s2);
		size_t n = strlen("osk-authz-host-1");
		memcpy(msg, "osk-authz-host-1", n);
		memcpy(msg + n, dev_chal, OSK_AUTHZ_CHALLENGE_LEN);
		memcpy(msg + n + OSK_AUTHZ_CHALLENGE_LEN, host_chal, OSK_AUTHZ_CHALLENGE_LEN);
		(void)fake_hmac_of(T_SECRET_ADMIN, msg, n + 32u, old_proof);
		osk_authz_session_end(s2);

		/* Dieser Beweis war fuer s2 richtig. An einer neuen Sitzung
		 * ist er falsch. */
		TT_EQ_INT(t_login_with(T_KEY_ADMIN, old_proof, sizeof(old_proof)), OSK_ERR_DENIED,
			  "Beweis einer anderen Sitzung");
	}

	TT_CASE("Fehlversuche sperren, und gesperrt ist nicht nicht-vorhanden");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;
		memset(&k, 0, sizeof(k));
		k.id = T_KEY_WEB;
		T_LABEL(k.label, "web");
		k.secret = T_SECRET_WEB;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.max_fail = 3u;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "anlegen");

		uint8_t junk[32];
		memset(junk, 0xEE, sizeof(junk));
		osk_authz_key_info_t info;

		for (int i = 1; i <= 2; i++) {
			TT_EQ_INT(t_login_with(T_KEY_WEB, junk, sizeof(junk)), OSK_ERR_DENIED,
				  "falscher Beweis");
			(void)osk_authz_key_info(T_KEY_WEB, &info);
			TT_EQ_INT(info.fails, i, "Fehlversuche gezaehlt");
			TT_EQ_INT(info.state, OSK_AUTHZ_ACTIVE, "noch nicht gesperrt");
		}
		TT_EQ_INT(t_login_with(T_KEY_WEB, junk, sizeof(junk)), OSK_ERR_DENIED,
			  "dritter Fehlversuch");
		(void)osk_authz_key_info(T_KEY_WEB, &info);
		TT_EQ_INT(info.state, OSK_AUTHZ_LOCKED, "jetzt gesperrt");

		/* LOCKED und NOT_FOUND sind verschiedene Befunde. Wer sie
		 * zusammenwirft, kann einen gesperrten Schluessel nicht von
		 * einem geloeschten unterscheiden - und der Verwalter weiss
		 * nicht, ob er freigeben oder neu anlegen muss. */
		uint8_t hc[OSK_AUTHZ_CHALLENGE_LEN], dc[OSK_AUTHZ_CHALLENGE_LEN];
		osk_authz_session_t s = OSK_AUTHZ_NO_SESSION;
		memset(hc, 0, sizeof(hc));
		TT_EQ_INT(osk_authz_session_begin(T_KEY_WEB, hc, dc, &s), OSK_ERR_LOCKED,
			  "gesperrt meldet LOCKED");
		TT_EQ_INT(osk_authz_session_begin(999u, hc, dc, &s), OSK_ERR_NOT_FOUND,
			  "unbekannt meldet NOT_FOUND");

		/* Richtiger Beweis hilft einem gesperrten nicht. */
		TT_EQ_INT(t_login(T_KEY_WEB, T_SECRET_WEB), OSK_AUTHZ_NO_SESSION,
			  "gesperrt bleibt gesperrt");

		TT_EQ_INT(osk_authz_key_unlock(admin, T_KEY_WEB), OSK_OK, "Admin gibt frei");
		(void)osk_authz_key_info(T_KEY_WEB, &info);
		TT_EQ_INT(info.state, OSK_AUTHZ_ACTIVE, "wieder benutzbar");
		TT_EQ_INT(info.fails, 0, "Zaehler zurueckgesetzt");
		TT_TRUE(t_login(T_KEY_WEB, T_SECRET_WEB) != OSK_AUTHZ_NO_SESSION,
			"Anmeldung geht wieder");
		osk_authz_session_end(admin);
	}

	TT_CASE("ein erfolgreicher Beweis setzt die Fehlversuche zurueck");
	{
		t_setup();
		uint8_t junk[32];
		memset(junk, 0x77, sizeof(junk));
		osk_authz_key_info_t info;

		TT_EQ_INT(t_login_with(T_KEY_ADMIN, junk, sizeof(junk)), OSK_ERR_DENIED, "daneben");
		(void)osk_authz_key_info(T_KEY_ADMIN, &info);
		TT_EQ_INT(info.fails, 1, "einer gezaehlt");

		osk_authz_session_t s = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		TT_TRUE(s != OSK_AUTHZ_NO_SESSION, "richtig angemeldet");
		(void)osk_authz_key_info(T_KEY_ADMIN, &info);
		TT_EQ_INT(info.fails, 0, "zurueckgesetzt");
		TT_EQ_INT(info.uses, 1, "Benutzung gezaehlt");
		osk_authz_session_end(s);
	}

	TT_CASE("DIE SCHRANKE: weitergeben, was man selbst nicht hat, geht nicht");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;

		/* Ein Verwalter, der verwalten darf - aber nur SIGN
		 * weitergeben. */
		memset(&k, 0, sizeof(k));
		k.id = T_KEY_WEB;
		T_LABEL(k.label, "teilverwalter");
		k.secret = T_SECRET_WEB;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN | OSK_CAP_AUTHZ_MANAGE;
		k.delegated = OSK_CAP_SIGN;
		k.max_fail = 5u;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "Teilverwalter anlegen");
		osk_authz_session_end(admin);

		osk_authz_session_t sub = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_TRUE(sub != OSK_AUTHZ_NO_SESSION, "Teilverwalter angemeldet");

		/* Der Aufstieg in einem Schritt: er gibt sich selbst mehr. */
		memset(&k, 0, sizeof(k));
		k.id = 20u;
		T_LABEL(k.label, "mehr-rechte");
		k.secret = T_SECRET_WALLET;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN | OSK_CAP_DECRYPT;
		k.delegated = OSK_CAP_NONE;
		TT_EQ_INT(osk_authz_key_add(sub, &k), OSK_ERR_DENIED,
			  "DECRYPT steht nicht in seiner delegated-Maske");

		/* Der Aufstieg in zwei Schritten: er gibt dem Neuen das Recht
		 * weiterzugeben, was er selbst nicht weitergeben darf.
		 * Derselbe Aufstieg, nur ueber eine Zwischenstation. */
		memset(&k, 0, sizeof(k));
		k.id = 21u;
		T_LABEL(k.label, "umweg");
		k.secret = T_SECRET_WALLET;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN | OSK_CAP_AUTHZ_MANAGE;
		k.delegated = OSK_CAP_SIGN;
		TT_EQ_INT(osk_authz_key_add(sub, &k), OSK_ERR_DENIED,
			  "AUTHZ_MANAGE steht nicht in seiner delegated-Maske");

		/* Und im fremden Bereich kann er gar nichts einrichten. */
		memset(&k, 0, sizeof(k));
		k.id = 22u;
		T_LABEL(k.label, "fremder-bereich");
		k.secret = T_SECRET_WALLET;
		k.realms = T_REALM_WALLET;
		k.caps = OSK_CAP_SIGN;
		TT_EQ_INT(osk_authz_key_add(sub, &k), OSK_ERR_DENIED,
			  "Zugang in einem Bereich, den er nicht hat");

		/* Was er darf, darf er. */
		memset(&k, 0, sizeof(k));
		k.id = 23u;
		T_LABEL(k.label, "erlaubt");
		k.secret = T_SECRET_WALLET;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.delegated = OSK_CAP_NONE;
		TT_EQ_INT(osk_authz_key_add(sub, &k), OSK_OK, "SIGN im eigenen Bereich");

		/* Der Neue kann nicht verwalten, also auch nichts anlegen. */
		osk_authz_session_end(sub);
		osk_authz_session_t leaf = t_login(23u, T_SECRET_WALLET);
		memset(&k, 0, sizeof(k));
		k.id = 24u;
		T_LABEL(k.label, "blatt");
		k.secret = T_SECRET_CRED;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		TT_EQ_INT(osk_authz_key_add(leaf, &k), OSK_ERR_DENIED,
			  "ohne AUTHZ_MANAGE geht kein Anlegen");
		osk_authz_session_end(leaf);
	}

	TT_CASE("delegated grosser als caps wird beim Eintragen abgewiesen");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;
		memset(&k, 0, sizeof(k));
		k.id = 30u;
		T_LABEL(k.label, "widerspruch");
		k.secret = T_SECRET_WEB;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.delegated = OSK_CAP_SIGN | OSK_CAP_DECRYPT;
		/* Das ist kein Angriff, sondern ein Widerspruch in sich - und
		 * er wird als Fehler des Aufrufers gemeldet, nicht als
		 * Absage. */
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "delegated ohne eigene caps");
		osk_authz_session_end(admin);
	}

	TT_CASE("Namen und Kennungen werden geprueft");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;

		memset(&k, 0, sizeof(k));
		k.id = 0u;
		T_LABEL(k.label, "null");
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "Kennung 0 ist frei");

		k.id = 40u;
		k.label[0] = '\0';
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "leerer Name");

		/* Ein Steuerzeichen im Namen kann eine Protokollzeile so
		 * aussehen lassen, als stuende etwas anderes darin. */
		T_LABEL(k.label, "zeile\neins");
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "Zeilenumbruch im Namen");

		T_LABEL(k.label, "gut");
		k.realms = OSK_REALM_NONE;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "ohne Bereich");

		k.realms = T_REALM_WEB;
		k.caps = (osk_cap_mask_t)(1u << 25);
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_ARG, "unbelegtes Faehigkeitsbit");

		k.caps = OSK_CAP_SIGN;
		k.secret = T_SECRET_WEB;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "jetzt geht es");
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_ERR_EXISTS, "Kennung schon vergeben");
		osk_authz_session_end(admin);
	}

	TT_CASE("Entfernen: nicht sich selbst, und die Sitzungen gehen mit");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;
		memset(&k, 0, sizeof(k));
		k.id = T_KEY_WEB;
		T_LABEL(k.label, "web");
		k.secret = T_SECRET_WEB;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.max_fail = 3u;
		TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "anlegen");

		osk_authz_session_t web = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_EQ_INT(osk_authz_check(web, OSK_CAP_SIGN, T_REALM_WEB), OSK_OK, "arbeitet");

		TT_EQ_INT(osk_authz_key_remove(admin, T_KEY_ADMIN), OSK_ERR_DENIED,
			  "sich selbst entfernen");
		TT_EQ_INT(osk_authz_key_remove(admin, T_KEY_WEB), OSK_OK, "den anderen entfernen");

		/* Eine Sitzung, die einen entfernten Zugangsschluessel
		 * ueberlebt, waere ein Zugang ohne Zugangsschluessel. */
		TT_EQ_INT(osk_authz_check(web, OSK_CAP_SIGN, T_REALM_WEB), OSK_ERR_DENIED,
			  "seine Sitzung ist mit ihm gegangen");
		TT_EQ_INT(osk_authz_key_remove(admin, T_KEY_WEB), OSK_ERR_NOT_FOUND,
			  "zweimal entfernen");
		osk_authz_session_end(admin);
	}

	TT_CASE("Verwalten braucht das Recht dazu");
	{
		t_setup();
		osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		osk_authz_key_t k;
		memset(&k, 0, sizeof(k));
		k.id = T_KEY_WEB;
		T_LABEL(k.label, "nur-signieren");
		k.secret = T_SECRET_WEB;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.max_fail = 3u;
		(void)osk_authz_key_add(admin, &k);

		memset(&k, 0, sizeof(k));
		k.id = T_KEY_WALLET;
		T_LABEL(k.label, "opfer");
		k.secret = T_SECRET_WALLET;
		k.realms = T_REALM_WEB;
		k.caps = OSK_CAP_SIGN;
		k.max_fail = 3u;
		(void)osk_authz_key_add(admin, &k);
		osk_authz_session_end(admin);

		osk_authz_session_t web = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_EQ_INT(osk_authz_key_remove(web, T_KEY_WALLET), OSK_ERR_DENIED,
			  "entfernen ohne AUTHZ_MANAGE");
		TT_EQ_INT(osk_authz_key_unlock(web, T_KEY_WALLET), OSK_ERR_DENIED,
			  "freigeben ohne AUTHZ_MANAGE");
		osk_authz_session_end(web);
	}

	TT_CASE("Sitzungsplaetze sind endlich und werden wieder frei");
	{
		t_setup();
		osk_authz_session_t s[OSK_AUTHZ_SESSION_MAX];
		uint8_t hc[OSK_AUTHZ_CHALLENGE_LEN], dc[OSK_AUTHZ_CHALLENGE_LEN];
		memset(hc, 0x22, sizeof(hc));

		for (size_t i = 0u; i < OSK_AUTHZ_SESSION_MAX; i++) {
			TT_EQ_INT(osk_authz_session_begin(T_KEY_ADMIN, hc, dc, &s[i]), OSK_OK,
				  "Platz belegen");
		}
		osk_authz_session_t extra = OSK_AUTHZ_NO_SESSION;
		TT_EQ_INT(osk_authz_session_begin(T_KEY_ADMIN, hc, dc, &extra), OSK_ERR_FULL,
			  "einer zu viel");
		osk_authz_session_end(s[0]);
		TT_EQ_INT(osk_authz_session_begin(T_KEY_ADMIN, hc, dc, &extra), OSK_OK,
			  "nach dem Beenden wieder frei");
		osk_authz_session_end_all();
	}

	TT_CASE("Aufzaehlen");
	{
		t_setup();
		uint16_t ids[OSK_AUTHZ_KEY_MAX];
		size_t n = 0u;
		TT_EQ_INT(osk_authz_key_list(ids, OSK_AUTHZ_KEY_MAX, &n), OSK_OK, "aufzaehlen");
		TT_EQ_INT(n, 1, "nur der Admin ist da");
		TT_EQ_INT(ids[0], T_KEY_ADMIN, "und es ist der Admin");
		TT_EQ_INT(osk_authz_key_list(ids, 0u, &n), OSK_ERR_FULL, "zu kleiner Puffer");
		TT_EQ_INT(osk_authz_key_list(NULL, 0u, &n), OSK_OK, "nur zaehlen");
		TT_EQ_INT(n, 1, "Zahl stimmt");
	}
}
