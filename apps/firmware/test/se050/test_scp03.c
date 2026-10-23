/* SCP03: Ableitung, Einpacken, Auspacken.
 *
 * WAS HIER WIRKLICH GEPRUEFT WIRD - und was nicht:
 *
 * Geprueft gegen fremde, veroeffentlichte Werte:
 *   - AES, AES-CBC und AES-CMAC gegen NIST SP 800-38A und SP 800-38B
 *     (siehe test_errors.c, Abschnitt Krypto).
 *   - Der AUFBAU der Ableitungseingabe gegen die drei Beispiele in
 *     AN12413 Abschnitt 3.2. Damit ist die Feldreihenfolge aus
 *     Amendment D 4.1.5 gegengelesen, nicht nur behauptet.
 *
 * NICHT geprueft:
 *   - Ein vollstaendiger Sitzungsaufbau gegen einen echten Baustein.
 *     GlobalPlatform veroeffentlicht zu Amendment D keine Pruefvektoren,
 *     und es gibt noch keine Hardware. Die Rundlauf-Tests unten zeigen,
 *     dass Einpacken und Auspacken zueinander passen - sie koennen NICHT
 *     zeigen, dass beide zum SE050 passen. Steht so in docs/se050.md. */
#include "tiny_test.h"
#include "se050_scp03.h"
#include "se050_crypto.h"

/* Ein Schluesselsatz zum Spielen. Nichts Geheimes - ein Testwert. */
static const osk_scp03_static_keys_t K = {
	.enc = { 0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
		 0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F },
	.mac = { 0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
		 0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F },
};

static const uint8_t HOST_CH[8] = { 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88 };
static const uint8_t CARD_CH[8] = { 0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00 };

/* Einen aufgebauten Kanal herstellen, wie ihn der Treiber nach
 * INITIALIZE UPDATE und EXTERNAL AUTHENTICATE hat. */
static int make_session(osk_scp03_t *s)
{
	uint8_t card_cg[8];
	uint8_t host_cg[8];

	if (osk_scp03_derive(s, &K, HOST_CH, CARD_CH, card_cg, host_cg) != OSK_OK) {
		return 0;
	}

	uint8_t ea[32];
	size_t ea_len = 0;
	return osk_scp03_build_external_auth(s, host_cg, ea, sizeof(ea),
					     &ea_len) == OSK_OK;
}

void test_scp03(void)
{
	TT_EQ_INT(osk_crypto_init(), OSK_OK, "Krypto bereit");

	TT_CASE("Ableitung: Eingabe stimmt mit dem Beispiel aus AN12413");
	{
		/* Ohne Kontext - genau der Fall, den NXP im Dokument nennt. */
		uint8_t dd[32];
		size_t n = 0;

		static const uint8_t want_enc[16] = {
			0,0,0,0,0,0,0,0,0,0,0, 0x04, 0x00, 0x00, 0x80, 0x01 };
		TT_EQ_INT(osk_scp03_derivation_data(dd, &n, 0x04, 0x0080,
						    NULL, 0), OSK_OK, "S-ENC");
		TT_EQ_INT(n, 16u, "16 Byte ohne Kontext");
		TT_EQ_MEM(dd, want_enc, 16, "S-ENC-Eingabe");

		static const uint8_t want_mac[16] = {
			0,0,0,0,0,0,0,0,0,0,0, 0x06, 0x00, 0x00, 0x80, 0x01 };
		TT_EQ_INT(osk_scp03_derivation_data(dd, &n, 0x06, 0x0080,
						    NULL, 0), OSK_OK, "S-MAC");
		TT_EQ_MEM(dd, want_mac, 16, "S-MAC-Eingabe");

		static const uint8_t want_rmac[16] = {
			0,0,0,0,0,0,0,0,0,0,0, 0x07, 0x00, 0x00, 0x80, 0x01 };
		TT_EQ_INT(osk_scp03_derivation_data(dd, &n, 0x07, 0x0080,
						    NULL, 0), OSK_OK, "S-RMAC");
		TT_EQ_MEM(dd, want_rmac, 16, "S-RMAC-Eingabe");
	}

	TT_CASE("Ableitung: mit Kontext kommt er hinten dran");
	{
		uint8_t dd[32];
		size_t n = 0;
		uint8_t ctx[16];
		memcpy(&ctx[0], HOST_CH, 8);
		memcpy(&ctx[8], CARD_CH, 8);

		TT_EQ_INT(osk_scp03_derivation_data(dd, &n, 0x00, 0x0040,
						    ctx, sizeof(ctx)),
			  OSK_OK, "Kryptogramm-Eingabe");
		TT_EQ_INT(n, 32u, "16 Byte Kopf plus 16 Byte Kontext");
		TT_EQ_INT(dd[11], 0x00u, "Konstante Kartenkryptogramm");
		TT_EQ_INT(dd[13], 0x00u, "L hoeherwertig");
		TT_EQ_INT(dd[14], 0x40u, "L niederwertig: 64 Bit");
		TT_EQ_INT(dd[15], 0x01u, "Zaehler i");
		TT_EQ_MEM(&dd[16], ctx, sizeof(ctx), "Kontext hinten");
	}

	TT_CASE("Ableitung: drei verschiedene Sitzungsschluessel");
	{
		osk_scp03_t s;
		uint8_t card_cg[8];
		uint8_t host_cg[8];

		TT_EQ_INT(osk_scp03_derive(&s, &K, HOST_CH, CARD_CH,
					   card_cg, host_cg),
			  OSK_OK, "abgeleitet");

		/* Die drei duerfen nicht gleich sein - sonst waere eine
		 * Konstante verwechselt, und R-MAC und C-MAC haetten
		 * denselben Schluessel. */
		TT_TRUE(memcmp(s.s_enc, s.s_mac, 16) != 0, "ENC != MAC");
		TT_TRUE(memcmp(s.s_mac, s.s_rmac, 16) != 0, "MAC != RMAC");
		TT_TRUE(memcmp(s.s_enc, s.s_rmac, 16) != 0, "ENC != RMAC");

		/* Und keiner darf einem festen Schluessel gleichen. */
		TT_TRUE(memcmp(s.s_enc, K.enc, 16) != 0, "ENC abgeleitet");
		TT_TRUE(memcmp(s.s_mac, K.mac, 16) != 0, "MAC abgeleitet");

		/* Die beiden Kryptogramme unterscheiden sich ebenfalls:
		 * sonst koennte der Wirt sein eigenes als das des Bausteins
		 * durchgehen lassen. */
		TT_TRUE(memcmp(card_cg, host_cg, 8) != 0,
			"Karten- und Wirtskryptogramm verschieden");

		/* Verkettung startet auf Null (Amendment D 6.2.3). */
		static const uint8_t zero[16] = { 0 };
		TT_EQ_MEM(s.chain, zero, 16, "Verkettung auf Null");
		TT_TRUE(!s.established, "noch nicht angemeldet");

		osk_scp03_close(&s);
	}

	TT_CASE("Ableitung: eine andere Zufallszahl gibt andere Schluessel");
	{
		osk_scp03_t a;
		osk_scp03_t b;
		uint8_t cg[8];
		uint8_t hg[8];
		uint8_t other[8];
		memcpy(other, CARD_CH, 8);
		other[0] ^= 0x01u;

		TT_EQ_INT(osk_scp03_derive(&a, &K, HOST_CH, CARD_CH, cg, hg),
			  OSK_OK, "erste Sitzung");
		TT_EQ_INT(osk_scp03_derive(&b, &K, HOST_CH, other, cg, hg),
			  OSK_OK, "zweite Sitzung");
		TT_TRUE(memcmp(a.s_enc, b.s_enc, 16) != 0,
			"ein Bit im Zufall aendert alles");
		osk_scp03_close(&a);
		osk_scp03_close(&b);
	}

	TT_CASE("Anmeldung: EXTERNAL AUTHENTICATE sieht richtig aus");
	{
		osk_scp03_t s;
		uint8_t card_cg[8];
		uint8_t host_cg[8];
		TT_EQ_INT(osk_scp03_derive(&s, &K, HOST_CH, CARD_CH,
					   card_cg, host_cg),
			  OSK_OK, "abgeleitet");

		uint8_t ea[32];
		size_t n = 0;
		TT_EQ_INT(osk_scp03_build_external_auth(&s, host_cg, ea,
							sizeof(ea), &n),
			  OSK_OK, "gebaut");
		TT_EQ_INT(n, 21u, "5 Kopf + 8 Kryptogramm + 8 MAC");
		TT_EQ_INT(ea[0], 0x84u, "CLA gesichert");
		TT_EQ_INT(ea[1], 0x82u, "INS EXTERNAL AUTHENTICATE");
		TT_EQ_INT(ea[2], 0x33u, "hoechste Sicherheitsstufe");
		TT_EQ_INT(ea[3], 0x00u, "P2 ist 0");
		TT_EQ_INT(ea[4], 0x10u, "Lc ist 16");
		TT_EQ_MEM(&ea[5], host_cg, 8, "Wirtskryptogramm");

		TT_TRUE(s.established, "Kanal steht");

		/* Zaehler beginnt bei 1 (Amendment D 6.2.6). */
		static const uint8_t one[16] = {
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 };
		TT_EQ_MEM(s.counter, one, 16, "Zaehler auf 1");

		/* Verkettung ist nicht mehr Null. */
		static const uint8_t zero[16] = { 0 };
		TT_TRUE(memcmp(s.chain, zero, 16) != 0, "Verkettung gesetzt");

		/* Zweimal anmelden gibt es nicht. */
		TT_EQ_INT(osk_scp03_build_external_auth(&s, host_cg, ea,
							sizeof(ea), &n),
			  OSK_ERR_EXISTS, "kein zweites Mal");

		osk_scp03_close(&s);
	}

	TT_CASE("Einpacken: Nutzlast verschwindet, Kopf wird angepasst");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		/* Ein Befehl mit 6 Byte Nutzlast. */
		static const uint8_t plain[] = {
			0x80, 0x04, 0x00, 0x49, 0x06,
			0x41, 0x02, 0x00, 0x20, 0xAA, 0xBB,
		};
		uint8_t wire[64];
		size_t n = 0;
		TT_EQ_INT(osk_scp03_wrap(&s, plain, sizeof(plain),
					 wire, sizeof(wire), &n),
			  OSK_OK, "eingepackt");

		/* 6 Byte Nutzlast + 1 Byte 0x80 = 7, aufgefuellt auf 16,
		 * plus 8 Byte MAC = Lc 24; dazu 5 Byte Kopf und 1 Byte Le. */
		TT_EQ_INT(n, 5u + 24u + 1u, "Gesamtlaenge");
		TT_EQ_INT(wire[0], 0x84u, "CLA auf gesichert gesetzt");
		TT_EQ_INT(wire[1], plain[1], "INS unveraendert");
		TT_EQ_INT(wire[2], plain[2], "P1 unveraendert");
		TT_EQ_INT(wire[3], plain[3], "P2 unveraendert");
		TT_EQ_INT(wire[4], 24u, "Lc = Auffuellung + MAC");
		TT_EQ_INT(wire[n - 1u], 0x00u, "Le angehaengt");

		/* Die Klartext-Nutzlast darf nirgends im Rahmen stehen. */
		bool leaked = false;
		for (size_t i = 0; i + 6u <= n; i++) {
			if (memcmp(&wire[i], &plain[5], 6) == 0) {
				leaked = true;
			}
		}
		TT_TRUE(!leaked, "Klartext steht nicht im Rahmen");

		osk_scp03_close(&s);
	}

	TT_CASE("Einpacken: Befehl ohne Nutzlast wird nur gesichert");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		static const uint8_t plain[] = { 0x80, 0x02, 0x0B, 0x25 };
		uint8_t wire[64];
		size_t n = 0;
		TT_EQ_INT(osk_scp03_wrap(&s, plain, sizeof(plain),
					 wire, sizeof(wire), &n),
			  OSK_OK, "eingepackt");
		TT_EQ_INT(n, 5u + 8u + 1u, "nur Kopf, MAC und Le");
		TT_EQ_INT(wire[4], 8u, "Lc ist nur der MAC");

		/* Der Zaehler laeuft trotzdem weiter (Amendment D 6.2.6). */
		static const uint8_t two[16] = {
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2 };
		TT_EQ_MEM(s.counter, two, 16, "Zaehler trotzdem weiter");

		osk_scp03_close(&s);
	}

	TT_CASE("Einpacken: jeder Befehl sieht anders aus");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		static const uint8_t plain[] = {
			0x80, 0x04, 0x00, 0x49, 0x02, 0x00, 0x20 };
		uint8_t a[64];
		uint8_t b[64];
		size_t na = 0;
		size_t nb = 0;

		TT_EQ_INT(osk_scp03_wrap(&s, plain, sizeof(plain), a, sizeof(a),
					 &na), OSK_OK, "erster");
		TT_EQ_INT(osk_scp03_wrap(&s, plain, sizeof(plain), b, sizeof(b),
					 &nb), OSK_OK, "zweiter");
		TT_EQ_INT(na, nb, "gleiche Laenge");
		TT_TRUE(memcmp(a, b, na) != 0,
			"derselbe Befehl zweimal gibt verschiedene Rahmen");

		osk_scp03_close(&s);
	}

	TT_CASE("Einpacken: ohne Kanal geht nichts");
	{
		osk_scp03_t s;
		memset(&s, 0, sizeof(s));

		static const uint8_t plain[] = { 0x80, 0x04, 0x00, 0x49 };
		uint8_t wire[64];
		size_t n = 0;
		TT_EQ_INT(osk_scp03_wrap(&s, plain, sizeof(plain), wire,
					 sizeof(wire), &n),
			  OSK_ERR_DENIED, "abgelehnt");
	}

	TT_CASE("Einpacken: unstimmiges Lc wird abgelehnt");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		/* Lc sagt 4, es kommen aber nur 2 Byte. */
		static const uint8_t bad[] = { 0x80, 0x04, 0x00, 0x49, 0x04,
					       0xAA, 0xBB };
		uint8_t wire[64];
		size_t n = 0;
		TT_EQ_INT(osk_scp03_wrap(&s, bad, sizeof(bad), wire,
					 sizeof(wire), &n),
			  OSK_ERR_ARG, "Lc passt nicht");

		osk_scp03_close(&s);
	}

	TT_CASE("Rundlauf: was eingepackt wurde, kommt wieder heraus");
	{
		/* Der Baustein wird hier nachgespielt: dieselbe Ableitung,
		 * dieselben Schluessel. Das zeigt, dass Einpacken und
		 * Auspacken zueinander passen - nicht, dass sie zum echten
		 * SE050 passen. */
		osk_scp03_t host;
		TT_TRUE(make_session(&host), "Kanal aufgebaut");

		static const uint8_t body[] = {
			0x41, 0x08, 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08 };

		/* Die Antwort so bauen, wie sie der Baustein bauen wuerde:
		 * Nutzlast auffuellen, mit dem Antwort-Startwert
		 * verschluesseln, R-MAC daruebersetzen. */
		uint8_t padded[16];
		memcpy(padded, body, sizeof(body));
		padded[sizeof(body)] = 0x80u;
		memset(&padded[sizeof(body) + 1u], 0,
		       sizeof(padded) - sizeof(body) - 1u);

		/* Erst einen Befehl einpacken - danach steht der Zaehler so,
		 * wie das Auspacken ihn erwartet. */
		static const uint8_t cmd[] = { 0x80, 0x02, 0x00, 0x00 };
		uint8_t wire[64];
		size_t wn = 0;
		TT_EQ_INT(osk_scp03_wrap(&host, cmd, sizeof(cmd), wire,
					 sizeof(wire), &wn),
			  OSK_OK, "Befehl eingepackt");

		/* Startwert der Antwort: Zaehlerblock des Befehls (also der
		 * Wert vor dem Hochzaehlen) mit 0x80 im ersten Byte. */
		uint8_t block[16];
		memcpy(block, host.counter, 16);
		for (size_t i = 16; i-- > 0;) {
			if (block[i]-- != 0u) {
				break;
			}
		}
		block[0] = 0x80u;

		uint8_t icv[16];
		TT_EQ_INT(osk_aes_ecb_encrypt_block(host.s_enc, block, icv),
			  OSK_OK, "Startwert");

		uint8_t cipher[16];
		TT_EQ_INT(osk_aes_cbc_encrypt(host.s_enc, icv, padded, cipher,
					      sizeof(cipher)),
			  OSK_OK, "verschluesselt");

		uint8_t macin[16 + 16 + 2];
		memcpy(&macin[0], host.chain, 16);
		memcpy(&macin[16], cipher, 16);
		macin[32] = 0x90u;
		macin[33] = 0x00u;

		uint8_t mac[16];
		TT_EQ_INT(osk_aes_cmac(host.s_rmac, macin, sizeof(macin), mac),
			  OSK_OK, "R-MAC gerechnet");

		uint8_t rsp[16 + 8 + 2];
		memcpy(&rsp[0], cipher, 16);
		memcpy(&rsp[16], mac, 8);
		rsp[24] = 0x90u;
		rsp[25] = 0x00u;

		uint8_t out[64];
		size_t on = 0;
		uint16_t sw = 0;
		TT_EQ_INT(osk_scp03_unwrap(&host, rsp, sizeof(rsp), out,
					   sizeof(out), &on, &sw),
			  OSK_OK, "ausgepackt");
		TT_EQ_INT(sw, 0x9000u, "Statuswort");
		TT_EQ_INT(on, sizeof(body), "Laenge ohne Auffuellung");
		TT_EQ_MEM(out, body, sizeof(body), "Inhalt");

		/* Jetzt dasselbe mit einem verdorbenen Byte: muss auffallen. */
		for (size_t i = 0; i < 24u; i++) {
			uint8_t bad[sizeof(rsp)];
			memcpy(bad, rsp, sizeof(rsp));
			bad[i] ^= 0x01u;

			/* Der Zaehler ist durch das erste Auspacken nicht
			 * veraendert worden, also darf noch einmal geprueft
			 * werden. */
			size_t bn = 0;
			uint16_t bsw = 0;
			const osk_result_t r = osk_scp03_unwrap(
				&host, bad, sizeof(bad), out, sizeof(out),
				&bn, &bsw);
			TT_TRUE(r != OSK_OK,
				"ein veraendertes Byte wird nicht angenommen");
		}

		osk_scp03_close(&host);
	}

	TT_CASE("Auspacken: Fehlerstatus kommt ohne MAC durch");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		/* Nach Amendment D 6.2.5 sind es dann genau zwei Bytes. */
		static const uint8_t err[] = { 0x69, 0x85 };
		uint8_t out[16];
		size_t n = 0;
		uint16_t sw = 0;
		TT_EQ_INT(osk_scp03_unwrap(&s, err, sizeof(err), out,
					   sizeof(out), &n, &sw),
			  OSK_OK, "durchgereicht");
		TT_EQ_INT(sw, 0x6985u, "Statuswort");
		TT_EQ_INT(n, 0u, "keine Nutzlast");

		/* Fehlerstatus MIT Nutzlast gibt es nicht. */
		static const uint8_t weird[] = { 0xAA, 0xBB, 0x69, 0x85 };
		TT_EQ_INT(osk_scp03_unwrap(&s, weird, sizeof(weird), out,
					   sizeof(out), &n, &sw),
			  OSK_ERR_FORMAT, "Fehler mit Nutzlast abgelehnt");

		osk_scp03_close(&s);
	}

	TT_CASE("Auspacken: zu kurze Antworten");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		uint8_t out[16];
		size_t n = 0;
		uint16_t sw = 0;

		static const uint8_t one[] = { 0x90 };
		TT_EQ_INT(osk_scp03_unwrap(&s, one, 1, out, sizeof(out), &n, &sw),
			  OSK_ERR_FORMAT, "ein Byte");

		/* Erfolgsstatus, aber kein Platz fuer einen R-MAC. */
		static const uint8_t two[] = { 0x90, 0x00 };
		TT_EQ_INT(osk_scp03_unwrap(&s, two, 2, out, sizeof(out), &n, &sw),
			  OSK_ERR_FORMAT, "Erfolg ohne MAC");

		osk_scp03_close(&s);
	}

	TT_CASE("Schliessen loescht die Schluessel wirklich");
	{
		osk_scp03_t s;
		TT_TRUE(make_session(&s), "Kanal aufgebaut");

		osk_scp03_close(&s);

		static const uint8_t zero[sizeof(osk_scp03_t)] = { 0 };
		TT_EQ_MEM(&s, zero, sizeof(s), "alles auf Null");
		TT_TRUE(!s.established, "Kanal weg");
	}
}
