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
#include "se050_scp03.h"
#include "se050_crypto.h"

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

/* Kennung des Anmeldeobjekts fuer den Geraetekanal. Auf dem Geraet legt das
 * die Platine fest; hier ein fester Wert, den der Test nachprueft. */
#define TEST_CHANNEL_OBJ 0x0F0000FFu

osk_result_t osk_se050_platform_channel_obj(uint32_t *obj_id_out)
{
	*obj_id_out = TEST_CHANNEL_OBJ;
	return OSK_OK;
}

/* Den Schluessel aus der PIN nachrechnen - derselbe Weg wie in se050.c.
 * Absichtlich hier noch einmal und nicht aus dem Treiber geholt: so prueft
 * der Test die Ableitung mit, statt sie zu uebernehmen. Weicht der Treiber
 * ab, scheitert der Sitzungsaufbau und der Test faellt durch. */
static void pin_keyset_for_test(const uint8_t *pin, size_t len, osk_scp03_static_keys_t *out)
{
	static const uint8_t label[16] = "osk-pin-key-v1";
	uint8_t mac[16];

	TT_EQ_INT(osk_aes_cmac(label, pin, len, mac), OSK_OK, "PIN-Schluessel gerechnet");
	memcpy(out->enc, mac, 16);
	memcpy(out->mac, mac, 16);
	memcpy(out->dek, mac, 16);
}

/* Die Objektattribute bauen, wie der Baustein sie beim beglaubigten Lesen
 * in TAG_2 liefert (AN12413 Tabelle 3). */
static size_t attrs_for_test(uint8_t *out, uint16_t used, uint16_t max_tries)
{
	uint8_t a[14];
	memset(a, 0, sizeof(a));
	a[4] = TYPE_AES_KEY;
	a[5] = SE050_SET;
	a[6] = (uint8_t)(used >> 8);
	a[7] = (uint8_t)(used & 0xFFu);
	a[12] = (uint8_t)(max_tries >> 8);
	a[13] = (uint8_t)(max_tries & 0xFFu);

	out[0] = TAG_2;
	out[1] = (uint8_t)sizeof(a);
	memcpy(&out[2], a, sizeof(a));
	return 2u + sizeof(a);
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

		/* Die Sitzung muss ueber CreateSession gegen das genannte
		 * Anmeldeobjekt aufgebaut worden sein (AN12413 3.6.3.2) -
		 * nicht mit einem INITIALIZE UPDATE ins Blaue. */
		TT_TRUE(sim.session_created, "CreateSession ist gelaufen");
		TT_EQ_INT(sim.session_obj, TEST_CHANNEL_OBJ,
			  "und zwar gegen das genannte Anmeldeobjekt");

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
		TT_EQ_INT(osk_se050_key_create(1, OSK_CURVE_P256, NULL), OSK_ERR_IO, "key_create");
		TT_EQ_INT(osk_se050_key_delete(1), OSK_ERR_IO, "key_delete");
		TT_EQ_INT(osk_se050_key_public(1, buf, sizeof(buf), &n), OSK_ERR_IO, "key_public");
		TT_EQ_INT(osk_se050_sign(1, buf, 32, buf, sizeof(buf), &n), OSK_ERR_IO, "sign");
		TT_EQ_INT(osk_se050_ecdh(1, buf, 32, buf, sizeof(buf), &n), OSK_ERR_IO, "ecdh");
		TT_EQ_INT(osk_se050_hkdf(1, buf, 8, buf, 32), OSK_ERR_IO, "hkdf");
		TT_EQ_INT(osk_se050_hmac(1, buf, 8, buf, sizeof(buf), &n), OSK_ERR_IO, "hmac");
		TT_EQ_INT(osk_se050_pin_verify(buf, 6), OSK_ERR_IO, "pin_verify");
		TT_EQ_INT(osk_se050_pin_set(NULL, 0, buf, 6), OSK_ERR_IO, "pin_set");
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_IO, "pin_tries_left");
		TT_EQ_INT(osk_se050_pin_clear(), OSK_ERR_IO, "pin_clear");
		TT_EQ_INT(osk_se050_key_derive_transient(1, OSK_CURVE_P256, buf, 8, 2), OSK_ERR_IO,
			  "key_derive_transient");

		/* Auch die zwei Einrichtungsfunktionen: ohne Kanal darf
		 * nichts davon durchgehen - ein Schluesselwechsel ohne
		 * verschluesselten Kanal waere der Schluessel im Klartext
		 * auf dem Bus. */
		osk_scp_keyset_t a;
		osk_scp_keyset_t b;
		memset(&a, 0x11, sizeof(a));
		memset(&b, 0x22, sizeof(b));
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x0Bu, &a, &b), OSK_ERR_IO, "rotate_scp_keys");
		TT_EQ_INT(osk_se050_require_scp(), OSK_ERR_IO, "require_scp");

		uint16_t bytes = 0;
		TT_EQ_INT(osk_se050_free_memory(&bytes), OSK_ERR_IO, "free_memory");
	}

	TT_CASE("Vertrag: ein Teil im FIPS-Modus meldet ECDH ehrlich ab");
	{
		/* Das Merkmalbild 0x27F2 stammt aus der FIPS-Richtlinie fuer
		 * einen echten SE051: ECDSA an, aber EdDSA und DH_Mont aus,
		 * und das Bit CONFIG_FIPS_MODE_DISABLED ist 0 - der Baustein
		 * kann also im FIPS-Modus laufen. Dann verweigert er ECDH
		 * grundsaetzlich (AN12413 4.10.3.1). */
		fake_bus_t fb;
		se050_sim_t sim;
		fb_init(&fb);
		sim_attach(&sim, &fb);
		sim.applet_config = 0x27F2u;
		g_bus = &fb;
		TT_EQ_INT(osk_se050_open(), OSK_OK, "angemeldet");

		uint8_t peer[65];
		memset(peer, 0x04, sizeof(peer));
		uint8_t secret[64];
		size_t n = 0;

		const size_t before = sim.commands;
		TT_EQ_INT(osk_se050_ecdh(1, peer, sizeof(peer), secret, sizeof(secret), &n),
			  OSK_ERR_UNSUPPORTED, "ECDH ehrlich abgelehnt");
		TT_EQ_INT(sim.commands, before, "und kein Befehl verschwendet");

		/* Die zwei Curve25519-Kurven duerfen dann auch nicht als
		 * vorhanden gemeldet werden. */
		uint8_t list[0x12];
		memset(list, SE050_NOT_SET, sizeof(list));
		list[EC_NIST_P256] = SE050_SET;

		uint8_t body[sizeof(list) + 4];
		body[0] = TAG_1;
		body[1] = (uint8_t)sizeof(list);
		memcpy(&body[2], list, sizeof(list));
		sim_set_reply(&sim, body, 2u + sizeof(list), SW_NO_ERROR);

		uint32_t mask = 0;
		TT_EQ_INT(osk_se050_curves(&mask), OSK_OK, "Kurven abgefragt");
		TT_TRUE(mask & (1u << OSK_CURVE_P256), "P256 bleibt");
		TT_TRUE(!(mask & (1u << OSK_CURVE_ED25519)), "Ed25519 nicht gemeldet");
		TT_TRUE(!(mask & (1u << OSK_CURVE_X25519)), "X25519 nicht gemeldet");

		osk_se050_close();
	}

	TT_CASE("Vertrag: freier Platz im Chip");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* GetFreeMemory antwortet mit zwei Byte in TAG_1. */
		uint8_t give[4];
		give[0] = TAG_1;
		give[1] = 2u;
		give[2] = 0x12u;
		give[3] = 0x34u;
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);

		uint16_t bytes = 0;
		TT_EQ_INT(osk_se050_free_memory(&bytes), OSK_OK, "abgefragt");
		TT_EQ_INT(bytes, 0x1234u, "hoechstwertiges Byte zuerst");
		TT_EQ_INT(sim.last_ins, INS_MGMT, "INS_MGMT");
		TT_EQ_INT(sim.last_p2, P2_MEMORY, "P2_MEMORY");

		/* Gefragt werden muss nach DAUERHAFTEM Speicher - fluechtiger
		 * sagt nichts darueber, wie viele Schluessel noch passen. */
		TT_EQ_INT(sim.last_payload_len, 3u, "ein TLV mit einem Byte");
		TT_EQ_INT(sim.last_payload[0], TAG_1, "TAG_1");
		TT_EQ_INT(sim.last_payload[2], MEM_PERSISTENT, "MEM_PERSISTENT");

		/* 0x7FFF heisst "32768 oder mehr". Das wird durchgereicht,
		 * nicht zu einer genaueren Zahl erfunden. */
		give[2] = 0x7Fu;
		give[3] = 0xFFu;
		sim_set_reply(&sim, give, sizeof(give), SW_NO_ERROR);
		TT_EQ_INT(osk_se050_free_memory(&bytes), OSK_OK, "obere Grenze");
		TT_EQ_INT(bytes, 0x7FFFu, "unveraendert gemeldet");

		/* Ein Chip, der nur ein Byte schickt, ist ein Formatfehler
		 * und keine 0 - sonst meldete das Geraet "voll", obwohl es
		 * nur die Antwort nicht verstanden hat. */
		uint8_t bad[3];
		bad[0] = TAG_1;
		bad[1] = 1u;
		bad[2] = 0x05u;
		sim_set_reply(&sim, bad, sizeof(bad), SW_NO_ERROR);
		bytes = 0xAAAAu;
		TT_EQ_INT(osk_se050_free_memory(&bytes), OSK_ERR_FORMAT, "ein Byte abgelehnt");

		TT_EQ_INT(osk_se050_free_memory(NULL), OSK_ERR_ARG, "NULL");

		osk_se050_close();
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
		TT_TRUE(!(mask & (1u << OSK_CURVE_SECP256K1)), "secp256k1 nicht gemeldet");
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
		TT_EQ_INT(osk_se050_key_create(0x12345678u, OSK_CURVE_P256, NULL), OSK_OK,
			  "angelegt");
		TT_EQ_INT(sim.last_ins, INS_WRITE, "INS_WRITE");
		TT_EQ_INT(sim.last_p1, P1_KEY_PAIR | P1_EC, "Schluesselpaar, EC");

		/* Ein exportierbarer Schluessel wird abgelehnt, nicht still
		 * nicht-exportierbar angelegt. */
		const osk_key_policy_t exportable = {.exportable = true, .needs_touch = false};
		TT_EQ_INT(osk_se050_key_create(1, OSK_CURVE_P256, &exportable), OSK_ERR_UNSUPPORTED,
			  "exportierbar abgelehnt");

		/* Eine Kurve, die der Vertrag nicht kennt. */
		TT_EQ_INT(osk_se050_key_create(1, (osk_curve_t)99, NULL), OSK_ERR_ARG,
			  "unbekannte Kurve");

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
		TT_EQ_INT(osk_se050_key_public(7, out, sizeof(out), &n), OSK_OK, "gelesen");
		TT_EQ_INT(n, sizeof(pub), "Laenge");
		TT_EQ_MEM(out, pub, sizeof(pub), "Inhalt");

		/* Zielpuffer zu klein wird gemeldet. */
		uint8_t small[8];
		TT_EQ_INT(osk_se050_key_public(7, small, sizeof(small), &n), OSK_ERR_FULL,
			  "zu klein");

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
		TT_EQ_INT(osk_se050_sign(3, hash, sizeof(hash), sig, sizeof(sig), &n), OSK_OK,
			  "signiert");
		TT_EQ_INT(sim.last_ins, INS_CRYPTO, "INS_CRYPTO");
		TT_EQ_INT(sim.last_p1, P1_SIGNATURE, "P1_SIGNATURE");
		TT_EQ_INT(sim.last_p2, P2_SIGN, "P2_SIGN");

		/* Eine Hashlaenge, zu der es kein Verfahren gibt, wird
		 * abgelehnt - nicht auf gut Glueck als SHA256 geschickt. */
		TT_EQ_INT(osk_se050_sign(3, hash, 20u, sig, sizeof(sig), &n), OSK_ERR_ARG,
			  "20 Byte abgelehnt");
		TT_EQ_INT(osk_se050_sign(3, NULL, 32u, sig, sizeof(sig), &n), OSK_ERR_ARG,
			  "kein Hash");

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
		TT_EQ_INT(osk_se050_hkdf(9, info, sizeof(info) - 1u, out, sizeof(out)), OSK_OK,
			  "abgeleitet");
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

		TT_EQ_INT(osk_se050_hkdf(9, info, sizeof(info) - 1u, out, sizeof(out)),
			  OSK_ERR_FORMAT, "zu kurz gemeldet");

		static const uint8_t zero[32] = {0};
		TT_EQ_MEM(out, zero, sizeof(out), "Ausgabe geleert");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Fehlerstatus des Bausteins kommt als Grund an");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		sim_set_reply(&sim, NULL, 0, SW_CONDITIONS_NOT_SATISFIED);
		TT_EQ_INT(osk_se050_key_delete(5), OSK_ERR_DENIED, "6985 wird OSK_ERR_DENIED");

		sim_set_reply(&sim, NULL, 0, SW_WRONG_DATA);
		TT_EQ_INT(osk_se050_key_delete(5), OSK_ERR_ARG, "6A80 wird OSK_ERR_ARG");

		osk_se050_close();
	}

	TT_CASE("Vertrag: PIN als AESKey-Anmeldeobjekt setzen");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		static const uint8_t pin[] = {'1', '2', '3', '4', '5', '6'};

		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_pin_set(NULL, 0, pin, sizeof(pin)), OSK_OK, "eingerichtet");

		/* Entscheidend: als AESKey-Anmeldeobjekt, nicht als UserID -
		 * nur dann meldet der Baustein den Fehlversuchszaehler
		 * (AN12413 3.2.4.4). */
		TT_EQ_INT(sim.last_ins, INS_WRITE | INS_AUTH_OBJECT, "Anmeldeobjekt");
		TT_EQ_INT(sim.last_p1, P1_AES, "P1_AES, nicht P1_USERID");

		/* Und mit einer Hoechstzahl Versuche - ohne die zaehlt der
		 * Baustein gar nicht (3.2.4.4). */
		TT_TRUE(sim.last_payload_len > 4u, "Nutzlast vorhanden");
		TT_EQ_INT(sim.last_payload[0], TAG_MAX_ATTEMPTS, "TAG_MAX_ATTEMPTS zuerst");
		TT_EQ_INT(sim.last_payload[1], 2u, "zwei Byte");

		/* Die PIN selbst darf nirgends im Befehl stehen. */
		bool leaked = false;
		for (size_t i = 0; i + sizeof(pin) <= sim.last_payload_len; i++) {
			if (memcmp(&sim.last_payload[i], pin, sizeof(pin)) == 0) {
				leaked = true;
			}
		}
		TT_TRUE(!leaked, "die PIN steht nicht im Befehl");

		osk_se050_close();
	}

	TT_CASE("Vertrag: richtige PIN oeffnet die Sitzung, falsche nicht");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		static const uint8_t pin[] = {'1', '2', '3', '4', '5', '6'};
		static const uint8_t wrong[] = {'1', '2', '3', '4', '5', '7'};

		/* Der Nachbau erwartet ab jetzt den aus der PIN abgeleiteten
		 * Satz - wie ein Baustein, in dem diese PIN gesetzt ist. */
		osk_scp03_static_keys_t pin_keys;
		pin_keyset_for_test(pin, sizeof(pin), &pin_keys);
		sim_set_auth_keys(&sim, &pin_keys);

		TT_EQ_INT(osk_se050_pin_verify(pin, sizeof(pin)), OSK_OK, "richtige PIN");

		/* Gegen das PIN-Objekt, nicht gegen den Geraetekanal. */
		TT_EQ_INT(sim.session_obj, 0x0F000001u, "Sitzung gegen das PIN-Objekt");

		/* Falsche PIN: das Kryptogramm des Bausteins stimmt dann
		 * nicht, der Aufbau scheitert. Der Vergleich findet im Chip
		 * statt - der Wirt erfaehrt nur Ja oder Nein. */
		TT_EQ_INT(osk_se050_pin_verify(wrong, sizeof(wrong)), OSK_ERR_DENIED,
			  "falsche PIN abgewiesen");

		/* Zu kurz und zu lang werden vorher abgefangen, ohne Befehl. */
		const size_t before = sim.commands;
		TT_EQ_INT(osk_se050_pin_verify(pin, 2u), OSK_ERR_ARG, "zu kurz");
		uint8_t longpin[32];
		memset(longpin, '9', sizeof(longpin));
		TT_EQ_INT(osk_se050_pin_verify(longpin, sizeof(longpin)), OSK_ERR_ARG, "zu lang");
		TT_EQ_INT(osk_se050_pin_verify(NULL, 6u), OSK_ERR_ARG, "NULL");
		TT_EQ_INT(sim.commands, before, "nichts gesendet");

		osk_se050_close();
	}

	TT_CASE("Vertrag: PIN loeschen ist wiederholbar");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_pin_clear(), OSK_OK, "geloescht");
		TT_EQ_INT(sim.last_ins, INS_MGMT, "INS_MGMT");
		TT_EQ_INT(sim.last_p2, P2_DELETE_OBJECT, "P2_DELETE_OBJECT");

		/* Zweimal loeschen muss denselben Zustand ergeben wie
		 * einmal. Der Baustein meldet "Bedingung nicht erfuellt",
		 * wenn das Objekt schon weg ist - das darf kein Fehler sein,
		 * sonst scheitert ein Zuruecksetzen auf Werkszustand daran,
		 * dass es schon zurueckgesetzt war. */
		sim_set_reply(&sim, NULL, 0, SW_CONDITIONS_NOT_SATISFIED);
		TT_EQ_INT(osk_se050_pin_clear(), OSK_OK, "zweites Loeschen ist kein Fehler");

		osk_se050_close();
	}

	TT_CASE("Vertrag: fluechtige Ableitung lehnt ehrlich ab");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* Der SE05x hat keinen Befehl, der ein Schluesselpaar INNEN
		 * aus einem Grundgeheimnis ableitet: HKDF und PBKDF2 liefern
		 * beide an den Wirt (AN12413 4.14), und WriteECKey nimmt den
		 * privaten Wert nur vom Wirt. Der Treiber lehnt darum ab,
		 * statt den privaten Schluessel durch den nRF52840 zu
		 * fuehren. */
		static const uint8_t info[] = "example.com";
		const size_t before = sim.commands;
		TT_EQ_INT(
		    osk_se050_key_derive_transient(1, OSK_CURVE_P256, info, sizeof(info) - 1u, 2),
		    OSK_ERR_UNSUPPORTED, "lehnt ab");
		TT_EQ_INT(sim.commands, before, "und schickt dabei keinen Befehl");

		/* Unbrauchbare Parameter werden vorher abgefangen. */
		TT_EQ_INT(osk_se050_key_derive_transient(1, OSK_CURVE_P256, NULL, 0, 2),
			  OSK_ERR_ARG, "kein info");
		TT_EQ_INT(osk_se050_key_derive_transient(1, (osk_curve_t)99, info, 4, 2),
			  OSK_ERR_ARG, "unbekannte Kurve");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Werksschluessel ersetzen baut PUT KEY richtig");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		osk_scp_keyset_t cur;
		osk_scp_keyset_t new_keys;
		memset(cur.enc, 0x11, sizeof(cur.enc));
		memset(cur.mac, 0x22, sizeof(cur.mac));
		memset(cur.dek, 0x33, sizeof(cur.dek));
		memset(new_keys.enc, 0xA1, sizeof(new_keys.enc));
		memset(new_keys.mac, 0xA2, sizeof(new_keys.mac));
		memset(new_keys.dek, 0xA3, sizeof(new_keys.dek));

		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x0Bu, &cur, &new_keys), OSK_OK, "ersetzt");

		TT_EQ_INT(sim.last_ins, 0xD8u, "INS PUT KEY");
		TT_EQ_INT(sim.last_p1, 0x0Bu, "P1 ist die zu ersetzende Fassung, b8 = 0");
		TT_EQ_INT(sim.last_p2, 0x81u, "P2: mehrere Schluessel ab Kennung 1");

		/* Datenfeld: 1 Byte neue Fassung + dreimal 23 Byte. */
		TT_EQ_INT(sim.last_payload_len, 1u + 3u * 23u, "Laenge");
		TT_EQ_INT(sim.last_payload[0], 0x0Bu, "neue Fassung vorn");
		for (size_t i = 0; i < 3u; i++) {
			const uint8_t *e = &sim.last_payload[1u + i * 23u];
			TT_EQ_INT(e[0], 0x88u, "Schluesselart AES");
			TT_EQ_INT(e[1], 0x11u, "Laenge der Schluesseldaten");
			TT_EQ_INT(e[2], 0x10u, "Schluessellaenge 16");
			TT_EQ_INT(e[3 + 16], 0x03u, "Pruefwertlaenge");
		}

		/* Kein neuer Schluessel darf im Klartext im Befehl stehen -
		 * das ist der ganze Zweck der Verpackung mit Key-DEK. */
		bool leaked = false;
		for (size_t i = 0; i + 16u <= sim.last_payload_len; i++) {
			if (memcmp(&sim.last_payload[i], new_keys.enc, 16) == 0 ||
			    memcmp(&sim.last_payload[i], new_keys.mac, 16) == 0 ||
			    memcmp(&sim.last_payload[i], new_keys.dek, 16) == 0) {
				leaked = true;
			}
		}
		TT_TRUE(!leaked, "kein neuer Schluessel im Klartext");

		/* Denselben Satz einsetzen ist ein Aufruf, der nichts tut -
		 * wird abgelehnt statt stillschweigend ausgefuehrt. */
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x0Bu, &cur, &cur), OSK_ERR_ARG,
			  "gleicher Satz abgelehnt");

		/* Fassung 0 waere "neu anlegen", nicht ersetzen. */
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x00u, &cur, &new_keys), OSK_ERR_ARG,
			  "Fassung 0 abgelehnt");
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x80u, &cur, &new_keys), OSK_ERR_ARG,
			  "Fassung ueber 0x7F abgelehnt");
		TT_EQ_INT(osk_se050_rotate_scp_keys(0x0Bu, NULL, &new_keys), OSK_ERR_ARG,
			  "kein alter Satz");

		osk_se050_close();
	}

	TT_CASE("Vertrag: Kanal verpflichtend machen");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		sim_set_reply(&sim, NULL, 0, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_require_scp(), OSK_OK, "gesetzt");
		TT_EQ_INT(sim.last_ins, INS_MGMT, "INS_MGMT");
		TT_EQ_INT(sim.last_p2, P2_SCP, "P2_SCP");

		/* Der Wert muss SCP_REQUIRED sein. SCP_NOT_REQUIRED waere
		 * der Werkszustand - genau das, was wir abstellen. */
		TT_EQ_INT(sim.last_payload_len, 3u, "ein TLV mit einem Byte");
		TT_EQ_INT(sim.last_payload[0], TAG_1, "TAG_1");
		TT_EQ_INT(sim.last_payload[2], SE050_SCP_REQUIRED,
			  "SCP_REQUIRED, nicht NOT_REQUIRED");

		osk_se050_close();
	}

	TT_CASE("Pruefwert: AES-Schluessel nach Amendment D 7.2.2");
	{
		/* Der Pruefwert sind die drei vordersten Bytes von
		 * AES(Schluessel, 16 Byte 0x01). Hier gegen den
		 * NIST-Schluessel gerechnet, dessen AES-Ergebnis fuer einen
		 * Block aus 0x01 sich unabhaengig nachrechnen laesst. */
		static const uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
						0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
		uint8_t block[16];
		memset(block, 0x01, sizeof(block));

		uint8_t full[16];
		TT_EQ_INT(osk_aes_ecb_encrypt_block(key, block, full), OSK_OK, "Block gerechnet");

		uint8_t kcv[3];
		TT_EQ_INT(osk_scp03_key_check_value(key, kcv), OSK_OK, "Pruefwert gerechnet");
		TT_EQ_MEM(kcv, full, 3, "die drei vordersten Bytes");

		/* Zwei verschiedene Schluessel muessen verschiedene
		 * Pruefwerte haben - sonst faengt die Karte einen
		 * verdrehten Schluessel nicht. */
		uint8_t other[16];
		memcpy(other, key, sizeof(other));
		other[0] ^= 0x01u;
		uint8_t kcv2[3];
		TT_EQ_INT(osk_scp03_key_check_value(other, kcv2), OSK_OK, "zweiter Pruefwert");
		TT_TRUE(memcmp(kcv, kcv2, 3) != 0, "verschiedene Pruefwerte");
	}

	TT_CASE("Vertrag: verbleibende Versuche kommen aus dem Chip");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot(&fb, &sim), "angemeldet");

		/* Der Baustein liefert die Attribute in TAG_2 des
		 * beglaubigten Lesens: 2 Fehlversuche von 8 erlaubt. */
		uint8_t body[32];
		const size_t bl = attrs_for_test(body, 2u, 8u);
		sim_set_reply(&sim, body, bl, SW_NO_ERROR);

		uint8_t tries = 0;
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_OK, "gelesen");
		TT_EQ_INT(tries, 6u, "8 erlaubt minus 2 verbraucht");

		/* Beglaubigt gelesen - ohne INS_ATTEST gibt der Baustein die
		 * Attribute nicht heraus (AN12413 3.2.7.2). */
		TT_EQ_INT(sim.last_ins, INS_READ | INS_ATTEST, "INS_READ mit INS_ATTEST");

		/* Zaehler voll: gesperrt, und 0 Versuche. */
		const size_t bl2 = attrs_for_test(body, 8u, 8u);
		sim_set_reply(&sim, body, bl2, SW_NO_ERROR);
		tries = 42u;
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_LOCKED, "gesperrt");
		TT_EQ_INT(tries, 0u, "keine Versuche uebrig");

		/* Hoechstzahl 0 heisst beim SE050 "unbegrenzt" - dann gibt es
		 * keine verbleibenden Versuche, und eine Zahl zu erfinden
		 * waere falsch. */
		const size_t bl3 = attrs_for_test(body, 0u, 0u);
		sim_set_reply(&sim, body, bl3, SW_NO_ERROR);
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_UNSUPPORTED, "unbegrenzt");

		/* Zu kurze Attribute werden gemeldet, nicht ausgelesen. */
		uint8_t stub[6];
		stub[0] = TAG_2;
		stub[1] = 4u;
		memset(&stub[2], 0, 4);
		sim_set_reply(&sim, stub, sizeof(stub), SW_NO_ERROR);
		TT_EQ_INT(osk_se050_pin_tries_left(&tries), OSK_ERR_FORMAT, "zu kurz");

		TT_EQ_INT(osk_se050_pin_tries_left(NULL), OSK_ERR_ARG, "NULL");

		osk_se050_close();
	}
}
