/* Die Vertragsfunktionen aus osk_se050.h gegen den Nachbau.
 *
 * Das ist der Test, der die Schichten zusammen laufen laesst: T=1',
 * Applet-Auswahl, SCP03-Aufbau, verschluesselter Befehl, Antwort pruefen -
 * alles ueber osk_se050_open() und die Vertragsfunktionen.
 *
 * Grenze wie in se050_sim.h: der Nachbau liest die Beschreibung so wie der
 * Treiber. Zusammen zu passen heisst hier nicht, zum echten SE050 zu
 * passen. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_sim.h"
#include "osk_se050.h"
#include "se050_platform.h"
#include "se050_cmd.h"

/* Die Platinenhaken fuer den Testlauf. Auf dem Geraet erfuellt sie die
 * Platinen- bzw. Tresorschicht. */
static fake_bus_t *g_bus;

osk_result_t osk_se050_platform_bus(osk_se050_bus_t **bus_out)
{
	if (g_bus == NULL) {
		return OSK_ERR_IO;
	}
	*bus_out = &g_bus->bus;
	return OSK_OK;
}

osk_result_t osk_se050_platform_scp_keys(osk_scp03_static_keys_t *keys_out)
{
	*keys_out = *sim_keys();
	return OSK_OK;
}

/* Bus und Nachbau aufsetzen und den Chip anmelden. */
static int boot(fake_bus_t *fb, se050_sim_t *sim)
{
	fb_init(fb);
	sim_attach(sim, fb);
	g_bus = fb;
	return osk_se050_open() == OSK_OK;
}

void test_contract(void)
{
	TT_CASE("Vertrag: anmelden baut Applet-Auswahl und Kanal auf");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fb_init(&fb);
		sim_attach(&sim, &fb);
		g_bus = &fb;

		TT_EQ_INT(osk_se050_open(), OSK_OK, "angemeldet");
		TT_TRUE(sim.applet_selected, "Applet ausgewaehlt");
		TT_TRUE(sim.channel, "SCP03 steht");
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");

		/* Zweimal anmelden gibt es nicht. */
		TT_EQ_INT(osk_se050_open(), OSK_ERR_EXISTS, "kein zweites Mal");

		osk_se050_close();
		/* Nach dem Schliessen antwortet nichts mehr. */
		uint32_t mask = 0;
		TT_EQ_INT(osk_se050_curves(&mask), OSK_ERR_IO, "zu ist zu");

		/* Schliessen darf zweimal aufgerufen werden. */
		osk_se050_close();
	}

	TT_CASE("Vertrag: ohne Anmeldung antwortet nichts");
	{
		osk_se050_close();

		uint32_t mask = 0;
		uint8_t buf[32];
		size_t n = 0;
		uint8_t tries = 0;

		TT_EQ_INT(osk_se050_curves(&mask), OSK_ERR_IO, "curves");
		TT_EQ_INT(osk_se050_random(buf, sizeof(buf)), OSK_ERR_IO, "random");
		TT_EQ_INT(osk_se050_key_create(1, OSK_CURVE_P256, NULL),
			  OSK_ERR_IO, "key_create");
		TT_EQ_INT(osk_se050_key_delete(1), OSK_ERR_IO, "key_delete");
		TT_EQ_INT(osk_se050_key_public(1, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "key_public");
		TT_EQ_INT(osk_se050_sign(1, buf, 32, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "sign");
		TT_EQ_INT(osk_se050_ecdh(1, buf, 32, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "ecdh");
		TT_EQ_INT(osk_se050_hkdf(1, buf, 8, buf, 32), OSK_ERR_IO, "hkdf");
		TT_EQ_INT(osk_se050_hmac(1, buf, 8, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "hmac");
		TT_EQ_INT(osk_se050_pin_verify(buf, 6), OSK_ERR_IO, "pin_verify");
		TT_EQ_INT(osk_se050_pin_set(NULL, 0, buf, 6), OSK_ERR_IO,
			  "pin_set");
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_IO,
			  "pin_tries_left");
	}

	TT_CASE("Vertrag: Kurven werden abgefragt, nicht angenommen");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* ReadECCurveList liefert je Kurvenkennung unter 0x40 ein
		 * Byte. Hier: P256 und P384 angelegt, secp256k1 nicht. */
		uint8_t list[0x12];
		memset(list, SE050_NOT_SET, sizeof(list));
		list[EC_NIST_P256] = SE050_SET;
		list[EC_NIST_P384] = SE050_SET;

		uint8_t body[sizeof(list) + 4];
		body[0] = TAG_1;
		body[1] = (uint8_t)sizeof(list);
		memcpy(&body[2], list, sizeof(list));
		sim_set_reply(&sim, body, 2u + sizeof(list), SW_NO_ERROR);

		uint32_t mask = 0;
		TT_EQ_INT(osk_se050_curves(&mask), OSK_OK, "abgefragt");
		TT_EQ_INT(sim.last_ins, INS_READ, "INS_READ");
		TT_EQ_INT(sim.last_p1, P1_CURVE, "P1_CURVE");
		TT_EQ_INT(sim.last_p2, P2_LIST, "P2_LIST");

		TT_TRUE(mask & (1u << OSK_CURVE_P256), "P256 gemeldet");
		TT_TRUE(mask & (1u << OSK_CURVE_P384), "P384 gemeldet");
		TT_TRUE(!(mask & (1u << OSK_CURVE_SECP256K1)),
			"secp256k1 nicht gemeldet");
		/* Die beiden Curve25519-Kurven stehen nicht in der Liste -
		 * sie kommen aus den Merkmalbits, die der Nachbau setzt. */
		TT_TRUE(mask & (1u << OSK_CURVE_ED25519), "Ed25519 gemeldet");
		TT_TRUE(mask & (1u << OSK_CURVE_X25519), "X25519 gemeldet");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Zufall kommt durch den verschluesselten Kanal");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		uint8_t give[34];
		give[0] = TAG_1;
		give[1] = 32u;
		for (size_t i = 0; i < 32u; i++) {
			give[2 + i] = (uint8_t)(0xA0u + i);
		}
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);

		uint8_t out[32];
		TT_EQ_INT(osk_se050_random(out, sizeof(out)), OSK_OK, "geholt");
		TT_EQ_MEM(out, &give[2], 32, "Inhalt");
		TT_EQ_INT(sim.last_p2, P2_RANDOM, "P2_RANDOM");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Schluessel anlegen schickt Kennung und Kurve");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_key_create(0x12345678u, OSK_CURVE_P256, NULL),
			  OSK_OK, "angelegt");
		TT_EQ_INT(sim.last_ins, INS_WRITE, "INS_WRITE");
		TT_EQ_INT(sim.last_p1, P1_KEY_PAIR | P1_EC, "Schluesselpaar, EC");

		/* Ein exportierbarer Schluessel wird abgelehnt, nicht still
		 * nicht-exportierbar angelegt. */
		const osk_key_policy_t exportable = { .exportable = true,
						      .needs_touch = false };
		TT_EQ_INT(osk_se050_key_create(1, OSK_CURVE_P256, &exportable),
			  OSK_ERR_UNSUPPORTED, "exportierbar abgelehnt");

		/* Eine Kurve, die der Vertrag nicht kennt. */
		TT_EQ_INT(osk_se050_key_create(1, (osk_curve_t)99, NULL),
			  OSK_ERR_ARG, "unbekannte Kurve");

		osk_se050_close();
	}

	TT_CASE("Vertrag: oeffentlichen Teil lesen");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* Unkomprimierter P256-Punkt: 0x04 und zweimal 32 Byte. */
		uint8_t pub[65];
		pub[0] = 0x04u;
		for (size_t i = 1; i < sizeof(pub); i++) {
			pub[i] = (uint8_t)i;
		}

		uint8_t give[2 + sizeof(pub)];
		give[0] = TAG_1;
		give[1] = (uint8_t)sizeof(pub);
		memcpy(&give[2], pub, sizeof(pub));
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);

		uint8_t out[128];
		size_t n = 0;
		TT_EQ_INT(osk_se050_key_public(7, out, sizeof(out), &n),
			  OSK_OK, "gelesen");
		TT_EQ_INT(n, sizeof(pub), "Laenge");
		TT_EQ_MEM(out, pub, sizeof(pub), "Inhalt");

		/* Zielpuffer zu klein wird gemeldet. */
		uint8_t small[8];
		TT_EQ_INT(osk_se050_key_public(7, small, sizeof(small), &n),
			  OSK_ERR_FULL, "zu klein");

		osk_se050_close();
	}

	TT_CASE("Vertrag: signieren waehlt das Verfahren nach der Hashlaenge");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* osk_se050_sign() fragt zuerst den oeffentlichen Teil ab,
		 * um die Kurve zu erkennen. Der Nachbau gibt auf jeden
		 * Befehl dieselbe Antwort - ein P256-Punkt passt fuer die
		 * Abfrage, und als Signatur ist es eben eine Bytefolge. */
		uint8_t pub[65];
		pub[0] = 0x04u;
		memset(&pub[1], 0x11, sizeof(pub) - 1u);

		uint8_t give[2 + sizeof(pub)];
		give[0] = TAG_1;
		give[1] = (uint8_t)sizeof(pub);
		memcpy(&give[2], pub, sizeof(pub));
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);

		uint8_t hash[32];
		memset(hash, 0x5A, sizeof(hash));
		uint8_t sig[128];
		size_t n = 0;
		TT_EQ_INT(osk_se050_sign(3, hash, sizeof(hash), sig,
					 sizeof(sig), &n),
			  OSK_OK, "signiert");
		TT_EQ_INT(sim.last_ins, INS_CRYPTO, "INS_CRYPTO");
		TT_EQ_INT(sim.last_p1, P1_SIGNATURE, "P1_SIGNATURE");
		TT_EQ_INT(sim.last_p2, P2_SIGN, "P2_SIGN");

		/* Eine Hashlaenge, zu der es kein Verfahren gibt, wird
		 * abgelehnt - nicht auf gut Glueck als SHA256 geschickt. */
		TT_EQ_INT(osk_se050_sign(3, hash, 20u, sig, sizeof(sig), &n),
			  OSK_ERR_ARG, "20 Byte abgelehnt");
		TT_EQ_INT(osk_se050_sign(3, NULL, 32u, sig, sizeof(sig), &n),
			  OSK_ERR_ARG, "kein Hash");

		osk_se050_close();
	}

	TT_CASE("Vertrag: HKDF liefert genau die bestellte Laenge");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		uint8_t give[2 + 32];
		give[0] = TAG_1;
		give[1] = 32u;
		for (size_t i = 0; i < 32u; i++) {
			give[2 + i] = (uint8_t)(i * 3u);
		}
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);

		static const uint8_t info[] = "fido-credential";
		uint8_t out[32];
		TT_EQ_INT(osk_se050_hkdf(9, info, sizeof(info) - 1u, out,
					 sizeof(out)),
			  OSK_OK, "abgeleitet");
		TT_EQ_MEM(out, &give[2], 32, "Inhalt");
		TT_EQ_INT(sim.last_p2, P2_HKDF, "P2_HKDF");

		/* Weniger als bestellt ist ein Fehler, kein Teilergebnis:
		 * ein halb gefuellter Schluessel waere schwach und wuerde
		 * nicht auffallen. */
		uint8_t shortish[2 + 16];
		shortish[0] = TAG_1;
		shortish[1] = 16u;
		memset(&shortish[2], 0xEE, 16);
		sim_set_reply(&sim, shortish, sizeof(shortish), SW_NO_ERROR);

		TT_EQ_INT(osk_se050_hkdf(9, info, sizeof(info) - 1u, out,
					 sizeof(out)),
			  OSK_ERR_FORMAT, "zu kurz gemeldet");

		static const uint8_t zero[32] = { 0 };
		TT_EQ_MEM(out, zero, sizeof(out), "Ausgabe geleert");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Fehlerstatus des Bausteins kommt als Grund an");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		sim_set_reply(&sim, NULL, 0, SW_CONDITIONS_NOT_SATISFIED);
		TT_EQ_INT(osk_se050_key_delete(5), OSK_ERR_DENIED,
			  "6985 wird OSK_ERR_DENIED");

		sim_set_reply(&sim, NULL, 0, SW_WRONG_DATA);
		TT_EQ_INT(osk_se050_key_delete(5), OSK_ERR_ARG,
			  "6A80 wird OSK_ERR_ARG");

		osk_se050_close();
	}

	TT_CASE("Vertrag: PIN setzen und pruefen");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		static const uint8_t pin[] = { '1', '2', '3', '4', '5', '6' };

		/* Erstes Einrichten: ohne alte PIN. */
		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_pin_set(NULL, 0, pin, sizeof(pin)),
			  OSK_OK, "eingerichtet");
		TT_EQ_INT(sim.last_ins, INS_WRITE | INS_AUTH_OBJECT,
			  "als Anmeldeobjekt angelegt");
		TT_EQ_INT(sim.last_p1, P1_USERID, "P1_USERID");

		/* Pruefen: der Nachbau gibt eine Sitzungskennung zurueck. */
		uint8_t sess[2 + 8];
		sess[0] = TAG_1;
		sess[1] = 8u;
		memset(&sess[2], 0x77, 8);
		sim_set_reply(&sim, sess, sizeof(sess), SW_NO_ERROR);

		TT_EQ_INT(osk_se050_pin_verify(pin, sizeof(pin)), OSK_OK,
			  "geprueft");

		/* Zu kurze und zu lange PIN werden abgelehnt, ohne dass ein
		 * Befehl hinausgeht. */
		const size_t before = sim.commands;
		TT_EQ_INT(osk_se050_pin_verify(pin, 2u), OSK_ERR_ARG, "zu kurz");
		uint8_t longpin[32];
		memset(longpin, '9', sizeof(longpin));
		TT_EQ_INT(osk_se050_pin_verify(longpin, sizeof(longpin)),
			  OSK_ERR_ARG, "zu lang");
		TT_EQ_INT(osk_se050_pin_verify(NULL, 6u), OSK_ERR_ARG, "NULL");
		TT_EQ_INT(sim.commands, before, "nichts gesendet");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Fehlversuche sind am SE050 nicht ablesbar");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* AN12413 Abschnitt 3.2.4.4 und Erratasheet APP.3: fuer ein
		 * Objekt des Typs UserID meldet der Baustein den Zaehler
		 * nicht. Der Treiber gibt darum OSK_ERR_UNSUPPORTED statt
		 * einer geratenen Zahl - eine falsche "3 Versuche uebrig"
		 * waere schlimmer als keine Auskunft. */
		uint8_t tries = 42u;
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_UNSUPPORTED,
			  "ehrlich nicht lieferbar");
		TT_EQ_INT(tries, 0u, "keine geratene Zahl");

		osk_se050_close();
	}
}
