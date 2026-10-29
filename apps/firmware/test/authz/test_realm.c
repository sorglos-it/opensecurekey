/* Die Rechtetrennung dort pruefen, wo sie zaehlt.
 *
 * Ein Prueffall, der nur die erlaubten Wege geht, prueft die Trennung
 * NICHT - er prueft, dass das Geraet benutzbar ist. Darum steht in dieser
 * Datei der umgekehrte Fall zuerst: ein Zugangsschluessel, der einen
 * Vorgang in einem FREMDEN Bereich versucht, muss abgewiesen werden.
 *
 * Das Bild dazu aus dem Auftrag: der Dienst, der Server-Zertifikate
 * ausstellt, kommt nicht an die privaten Schluessel der Geldboerse. Beide
 * duerfen unterschreiben - die Faehigkeit allein trennt also nicht. Es
 * trennt der Bereich. */
#include <string.h>
#include "authz_t.h"

void t_setup(void)
{
	fake_reset();
	fake_add_key(T_SECRET_ADMIN);
	fake_add_key(T_SECRET_WEB);
	fake_add_key(T_SECRET_WALLET);
	fake_add_key(T_SECRET_CRED);
	fake_add_key(T_SIGNING_KEY);
	(void)osk_authz_init(T_SECRET_ADMIN);
	(void)osk_se050_counter_create(T_COUNTER);
}

osk_result_t t_login_with(uint16_t key_id, const uint8_t *proof, size_t proof_len)
{
	uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t dev_chal[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t dev_proof[OSK_AUTHZ_PROOF_LEN];
	osk_authz_session_t s = OSK_AUTHZ_NO_SESSION;

	memset(host_chal, 0xC3, sizeof(host_chal));
	osk_result_t r = osk_authz_session_begin(key_id, host_chal, dev_chal, &s);
	if (r != OSK_OK) {
		return r;
	}
	uint8_t buf[OSK_AUTHZ_PROOF_LEN];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, proof, proof_len < sizeof(buf) ? proof_len : sizeof(buf));
	return osk_authz_session_authenticate(s, buf, dev_proof);
}

osk_authz_session_t t_login(uint16_t key_id, osk_key_id_t secret)
{
	uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t dev_chal[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t msg[64];
	uint8_t proof[32];
	uint8_t dev_proof[OSK_AUTHZ_PROOF_LEN];
	osk_authz_session_t s = OSK_AUTHZ_NO_SESSION;

	memset(host_chal, 0xC3, sizeof(host_chal));
	if (osk_authz_session_begin(key_id, host_chal, dev_chal, &s) != OSK_OK) {
		return OSK_AUTHZ_NO_SESSION;
	}

	/* Der Beweis wird hier nachgerechnet, nicht von der geprueften Seite
	 * geholt: sonst prueft der Fall nur, dass zwei Aufrufe derselben
	 * Funktion gleich antworten. */
	const char *ctx = "osk-authz-host-1";
	size_t n = strlen(ctx);
	memcpy(msg, ctx, n);
	memcpy(msg + n, dev_chal, OSK_AUTHZ_CHALLENGE_LEN);
	memcpy(msg + n + OSK_AUTHZ_CHALLENGE_LEN, host_chal, OSK_AUTHZ_CHALLENGE_LEN);
	if (fake_hmac_of(secret, msg, n + 2u * OSK_AUTHZ_CHALLENGE_LEN, proof) != 0) {
		return OSK_AUTHZ_NO_SESSION;
	}
	if (osk_authz_session_authenticate(s, proof, dev_proof) != OSK_OK) {
		return OSK_AUTHZ_NO_SESSION;
	}
	return s;
}

/* Zwei Zugangsschluessel in zwei Bereichen anlegen, beide mit SIGN. */
static void add_two(void)
{
	osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
	osk_authz_key_t k;

	memset(&k, 0, sizeof(k));
	k.id = T_KEY_WEB;
	T_LABEL(k.label, "zertifikatsdienst");
	k.secret = T_SECRET_WEB;
	k.realms = T_REALM_WEB;
	k.caps = OSK_CAP_SIGN | OSK_CAP_PUBLIC_READ;
	k.delegated = OSK_CAP_NONE;
	k.max_fail = 3u;
	TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "Zertifikatsdienst anlegen");

	memset(&k, 0, sizeof(k));
	k.id = T_KEY_WALLET;
	T_LABEL(k.label, "geldboerse");
	k.secret = T_SECRET_WALLET;
	k.realms = T_REALM_WALLET;
	k.caps = OSK_CAP_SIGN | OSK_CAP_DECRYPT;
	k.delegated = OSK_CAP_NONE;
	k.max_fail = 3u;
	TT_EQ_INT(osk_authz_key_add(admin, &k), OSK_OK, "Geldboerse anlegen");

	osk_authz_session_end(admin);
}

void test_realm(void)
{
	t_setup();
	add_two();

	TT_CASE("im eigenen Bereich ist der Vorgang erlaubt");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_TRUE(s != OSK_AUTHZ_NO_SESSION, "Anmeldung des Zertifikatsdienstes");
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WEB), OSK_OK,
			  "signieren im eigenen Bereich");
		osk_authz_session_end(s);
	}

	TT_CASE("DER PRUEFFALL: im fremden Bereich wird abgewiesen");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		/* Derselbe Schluessel, dieselbe Faehigkeit, anderer Bereich.
		 * Wuerde das durchgehen, kaeme der Zertifikatsdienst an die
		 * Schluessel der Geldboerse. */
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WALLET), OSK_ERR_DENIED,
			  "signieren im Bereich der Geldboerse");
		osk_authz_session_end(s);
	}

	TT_CASE("und umgekehrt genauso - die Trennung geht in beide Richtungen");
	{
		osk_authz_session_t s = t_login(T_KEY_WALLET, T_SECRET_WALLET);
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WALLET), OSK_OK,
			  "Geldboerse im eigenen Bereich");
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WEB), OSK_ERR_DENIED,
			  "Geldboerse im Bereich des Zertifikatsdienstes");
		osk_authz_session_end(s);
	}

	TT_CASE("ein Gegenstand in beiden Bereichen ist fuer beide erreichbar");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WEB | T_REALM_WALLET), OSK_OK,
			  "Ueberschneidung genuegt");
		osk_authz_session_end(s);
	}

	TT_CASE("Faehigkeit fehlt - der Bereich allein hilft nicht");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_DECRYPT, T_REALM_WEB), OSK_ERR_DENIED,
			  "entschluesseln ohne das Recht dazu");
		/* Mehrere verlangte Rechte: ALLE muessen da sein. Eines davon
		 * genuegen zu lassen waere der haeufigste Fehler in einer
		 * solchen Pruefung. */
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN | OSK_CAP_DECRYPT, T_REALM_WEB),
			  OSK_ERR_DENIED, "SIGN hat er, DECRYPT nicht - zusammen also nein");
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN | OSK_CAP_PUBLIC_READ, T_REALM_WEB),
			  OSK_OK, "beide hat er");
		osk_authz_session_end(s);
	}

	TT_CASE("ein Gegenstand ohne Bereich gehoert niemandem");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		/* Leere Maske als "ueberall" zu lesen waere der stille
		 * Rechteverlust: ein neu angelegter Gegenstand, dessen
		 * Bereich noch nicht gesetzt ist, waere fuer jeden offen. */
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, OSK_REALM_NONE), OSK_ERR_DENIED,
			  "leere Bereichsmaske");
		osk_authz_session_end(s);
	}

	TT_CASE("Admin kommt ueberall hin - aber weil er alle Bereiche hat");
	{
		osk_authz_session_t s = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WEB), OSK_OK, "Admin, Bereich 1");
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WALLET), OSK_OK,
			  "Admin, Bereich 5");
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, OSK_REALM(15)), OSK_OK,
			  "Admin, Bereich 15");
		/* Auch der Admin kommt nicht in einen Gegenstand ohne
		 * Bereich: es gibt keine Abkuerzung um die Regel herum. */
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, OSK_REALM_NONE), OSK_ERR_DENIED,
			  "Admin, leere Maske");
		osk_authz_session_end(s);
	}

	TT_CASE("alle sechzehn Bereiche sind einzeln erreichbar");
	{
		osk_authz_session_t s = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		/* Gezaehlt wird, wie viele wirklich antworten - nicht
		 * geprueft, ob die Zahl 16 irgendwo steht. */
		int ok = 0;
		for (unsigned n = 0u; n < OSK_AUTHZ_REALM_COUNT; n++) {
			if (osk_authz_check(s, OSK_CAP_SIGN, OSK_REALM(n)) == OSK_OK) {
				ok++;
			}
		}
		TT_EQ_INT(ok, 16, "Zahl der erreichbaren Bereiche");
		/* Und ueber die sechzehn hinaus gibt es nichts: Bit 16 passt
		 * nicht mehr in osk_realm_mask_t und ist damit dieselbe
		 * leere Maske wie oben. */
		TT_EQ_INT(sizeof(osk_realm_mask_t), 2u, "Breite der Bereichsmaske");
		osk_authz_session_end(s);
	}

	TT_CASE("ohne angemeldete Sitzung ist nichts erlaubt");
	{
		uint8_t host_chal[OSK_AUTHZ_CHALLENGE_LEN];
		uint8_t dev_chal[OSK_AUTHZ_CHALLENGE_LEN];
		osk_authz_session_t s = OSK_AUTHZ_NO_SESSION;

		memset(host_chal, 0x11, sizeof(host_chal));
		TT_EQ_INT(osk_authz_session_begin(T_KEY_WEB, host_chal, dev_chal, &s), OSK_OK,
			  "Sitzung beginnen");
		/* Begonnen, aber nicht bewiesen. Genau hier wird aus einer
		 * Rechtepruefung eine Hoeflichkeit, wenn man es durchlaesst. */
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_SIGN, T_REALM_WEB), OSK_ERR_DENIED,
			  "begonnene, nicht angemeldete Sitzung");
		osk_authz_session_end(s);

		TT_EQ_INT(osk_authz_check(OSK_AUTHZ_NO_SESSION, OSK_CAP_SIGN, T_REALM_WEB),
			  OSK_ERR_DENIED, "Sitzungsnummer, die es nicht gibt");
		TT_EQ_INT(osk_authz_check(0u, OSK_CAP_SIGN, T_REALM_WEB), OSK_ERR_DENIED,
			  "beendete Sitzung");
	}

	TT_CASE("eine Pruefung ohne verlangtes Recht ist ein Fehler, keine Freigabe");
	{
		osk_authz_session_t s = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
		TT_EQ_INT(osk_authz_check(s, OSK_CAP_NONE, T_REALM_WEB), OSK_ERR_ARG,
			  "leere Faehigkeitsmaske");
		/* Ein Bit, das keine Faehigkeit bezeichnet: abweisen, nicht
		 * mitfuehren. Sonst bekommt der Schluessel das naechste neue
		 * Recht geschenkt, sobald es vergeben wird. */
		TT_EQ_INT(osk_authz_check(s, (osk_cap_mask_t)(1u << 20), T_REALM_WEB), OSK_ERR_ARG,
			  "unbelegtes Faehigkeitsbit");
		osk_authz_session_end(s);
	}

	TT_CASE("die Sitzung nennt, WER angemeldet ist - darum geht es bei FIPS");
	{
		osk_authz_session_t s = t_login(T_KEY_WEB, T_SECRET_WEB);
		uint16_t who = 0u;
		const char *label = NULL;
		TT_EQ_INT(osk_authz_session_identity(s, &who, &label), OSK_OK, "Identitaet lesen");
		TT_EQ_INT(who, T_KEY_WEB, "Kennung");
		TT_TRUE(label != NULL && strcmp(label, "zertifikatsdienst") == 0,
			"Name steht fuer das Protokoll bereit");
		osk_authz_session_end(s);

		/* Nach dem Ende nennt sie niemanden mehr. */
		TT_EQ_INT(osk_authz_session_identity(s, &who, &label), OSK_ERR_DENIED,
			  "beendete Sitzung hat keine Identitaet");
	}

	TT_CASE("Abziehen beendet alle Sitzungen auf einmal");
	{
		osk_authz_session_t a = t_login(T_KEY_WEB, T_SECRET_WEB);
		osk_authz_session_t b = t_login(T_KEY_WALLET, T_SECRET_WALLET);
		TT_TRUE(a != OSK_AUTHZ_NO_SESSION && b != OSK_AUTHZ_NO_SESSION, "zwei Sitzungen");
		osk_authz_session_end_all();
		TT_EQ_INT(osk_authz_check(a, OSK_CAP_SIGN, T_REALM_WEB), OSK_ERR_DENIED,
			  "erste Sitzung ist weg");
		TT_EQ_INT(osk_authz_check(b, OSK_CAP_SIGN, T_REALM_WALLET), OSK_ERR_DENIED,
			  "zweite Sitzung ist weg");
	}
}
