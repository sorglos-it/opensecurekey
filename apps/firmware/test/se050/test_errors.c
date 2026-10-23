/* Fehlerwege und die Krypto-Pruefvektoren.
 *
 * Die Krypto gehoert hierher, weil sie das Fundament ist: stimmt AES-CMAC
 * nicht, ist jeder SCP03-Test wertlos, und das soll nicht erst in
 * test_scp03.c auffallen. Die Werte stammen aus NIST SP 800-38A und
 * SP 800-38B - fremde, veroeffentlichte Pruefwerte, nicht selbst
 * ausgerechnete. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_crypto.h"
#include "se050_t1.h"
#include "se050_apdu.h"
#include "se050_cmd.h"

/* AES-128-Schluessel aus NIST SP 800-38A Anhang F und SP 800-38B. */
static const uint8_t NK[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
			       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
static const uint8_t NP[16] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
			       0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};

static void test_crypto_vectors(void)
{
	TT_EQ_INT(osk_crypto_init(), OSK_OK, "Krypto bereit");

	TT_CASE("Krypto: AES-CMAC gegen NIST SP 800-38B");
	{
		uint8_t mac[16];

		/* Beispiel 1: leere Nachricht. */
		static const uint8_t w0[16] = {0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
					       0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46};
		TT_EQ_INT(osk_aes_cmac(NK, NULL, 0, mac), OSK_OK, "leer");
		TT_EQ_MEM(mac, w0, 16, "CMAC leer");

		/* Beispiel 2: genau ein Block. */
		static const uint8_t w16[16] = {0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
						0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c};
		TT_EQ_INT(osk_aes_cmac(NK, NP, 16, mac), OSK_OK, "ein Block");
		TT_EQ_MEM(mac, w16, 16, "CMAC 16 Byte");

		/* Beispiel 3: 40 Byte, letzter Block unvollstaendig - genau
		 * der Fall, in dem eine falsche Unterschluessel-Rechnung
		 * auffaellt. */
		static const uint8_t p40[40] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
						0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
						0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
						0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
						0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11};
		static const uint8_t w40[16] = {0xdf, 0xa6, 0x67, 0x47, 0xde, 0x9a, 0xe6, 0x30,
						0x30, 0xca, 0x32, 0x61, 0x14, 0x97, 0xc8, 0x27};
		TT_EQ_INT(osk_aes_cmac(NK, p40, 40, mac), OSK_OK, "40 Byte");
		TT_EQ_MEM(mac, w40, 16, "CMAC 40 Byte");
	}

	TT_CASE("Krypto: AES-ECB-Block gegen NIST SP 800-38A F.1.1");
	{
		static const uint8_t want[16] = {0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
						 0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97};
		uint8_t out[16];
		TT_EQ_INT(osk_aes_ecb_encrypt_block(NK, NP, out), OSK_OK, "ein Block");
		TT_EQ_MEM(out, want, 16, "ECB-Block");
	}

	TT_CASE("Krypto: AES-CBC gegen NIST SP 800-38A F.2.1");
	{
		static const uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
		static const uint8_t in[32] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
					       0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
					       0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
					       0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51};
		static const uint8_t want[32] = {0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
						 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
						 0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee,
						 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2};

		uint8_t out[32];
		TT_EQ_INT(osk_aes_cbc_encrypt(NK, iv, in, out, 32), OSK_OK, "chiffriert");
		TT_EQ_MEM(out, want, 32, "CBC vorwaerts");

		uint8_t back[32];
		TT_EQ_INT(osk_aes_cbc_decrypt(NK, iv, want, back, 32), OSK_OK, "dechiffriert");
		TT_EQ_MEM(back, in, 32, "CBC rueckwaerts");

		/* Der Startwert des Aufrufers darf nicht veraendert werden -
		 * sonst stimmt der naechste Aufruf nicht mehr. */
		static const uint8_t iv_orig[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
						    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
		TT_EQ_MEM(iv, iv_orig, 16, "Startwert unveraendert");
	}

	TT_CASE("Krypto: CBC nur in ganzen Bloecken");
	{
		static const uint8_t iv[16] = {0};
		uint8_t in[17] = {0};
		uint8_t out[32];
		TT_EQ_INT(osk_aes_cbc_encrypt(NK, iv, in, out, 17), OSK_ERR_ARG,
			  "17 Byte abgelehnt");
		TT_EQ_INT(osk_aes_cbc_encrypt(NK, iv, in, out, 0), OSK_ERR_ARG, "0 Byte abgelehnt");
	}

	TT_CASE("Krypto: Puffer leeren und in gleicher Zeit vergleichen");
	{
		uint8_t b[32];
		memset(b, 0xAA, sizeof(b));
		osk_wipe(b, sizeof(b));

		static const uint8_t zero[32] = {0};
		TT_EQ_MEM(b, zero, sizeof(b), "geleert");

		/* osk_wipe() auf NULL darf nicht abstuerzen. */
		osk_wipe(NULL, 16);
		osk_wipe(b, 0);

		static const uint8_t x[4] = {1, 2, 3, 4};
		static const uint8_t y[4] = {1, 2, 3, 4};
		static const uint8_t z[4] = {1, 2, 3, 5};
		TT_EQ_INT(osk_memeq_ct(x, y, 4), 0, "gleich");
		TT_EQ_INT(osk_memeq_ct(x, z, 4), 1, "nur das letzte Byte");
		TT_EQ_INT(osk_memeq_ct(x, y, 0), 0, "Laenge 0 ist gleich");
	}

	TT_CASE("Krypto: Zufall liefert etwas und wiederholt sich nicht");
	{
		uint8_t a[32];
		uint8_t b[32];
		TT_EQ_INT(osk_random(a, sizeof(a)), OSK_OK, "erster Zug");
		TT_EQ_INT(osk_random(b, sizeof(b)), OSK_OK, "zweiter Zug");
		TT_TRUE(memcmp(a, b, sizeof(a)) != 0, "zwei Zuege sind verschieden");

		static const uint8_t zero[32] = {0};
		TT_TRUE(memcmp(a, zero, sizeof(a)) != 0, "nicht alles Null");

		TT_EQ_INT(osk_random(NULL, 16), OSK_ERR_ARG, "NULL abgelehnt");
		TT_EQ_INT(osk_random(a, 0), OSK_ERR_ARG, "Laenge 0 abgelehnt");
	}
}

static void test_sw_mapping(void)
{
	TT_CASE("Statuswort: Uebersetzung nach osk_result_t");
	{
		TT_EQ_INT(osk_se050_sw_to_result(SW_NO_ERROR), OSK_OK, "9000");
		TT_EQ_INT(osk_se050_sw_to_result(SW_CONDITIONS_NOT_SATISFIED), OSK_ERR_DENIED,
			  "6985 heisst Freigabe fehlt");
		TT_EQ_INT(osk_se050_sw_to_result(SW_SECURITY_STATUS), OSK_ERR_DENIED, "6982");
		TT_EQ_INT(osk_se050_sw_to_result(SW_COMMAND_NOT_ALLOWED), OSK_ERR_DENIED, "6986");
		TT_EQ_INT(osk_se050_sw_to_result(SW_WRONG_DATA), OSK_ERR_ARG, "6A80");
		TT_EQ_INT(osk_se050_sw_to_result(SW_DATA_INVALID), OSK_ERR_ARG, "6984");

		/* Familien, die wir nicht einzeln kennen. */
		TT_EQ_INT(osk_se050_sw_to_result(0x6A86u), OSK_ERR_ARG, "6Axx");
		TT_EQ_INT(osk_se050_sw_to_result(0x6B00u), OSK_ERR_ARG, "6Bxx");
		TT_EQ_INT(osk_se050_sw_to_result(0x6999u), OSK_ERR_DENIED, "69xx");

		/* Unbekannt darf NIE als Erfolg durchgehen. */
		TT_EQ_INT(osk_se050_sw_to_result(0x1234u), OSK_ERR_INTERNAL,
			  "unbekannt ist kein Erfolg");
		TT_EQ_INT(osk_se050_sw_to_result(0x0000u), OSK_ERR_INTERNAL,
			  "0000 ist kein Erfolg");
	}
}

static void test_bus_faults(void)
{
	TT_CASE("Fehlerweg: kein Baustein am Bus");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add(&fb, FB_NO_CHIP, NULL, 0, NULL, 0);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_ERR_IO, "Oeffnen scheitert");
		TT_EQ_INT(fb.empty_writes, 0u, "kein leerer Rahmen versucht");
	}

	TT_CASE("Fehlerweg: Baustein bleibt stumm");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add(&fb, FB_SILENT, NULL, 0, NULL, 0);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_ERR_TIMEOUT, "Zeitlimit greift");
		TT_TRUE(fb.waits > 0u, "es wurde gewartet");
		/* Die Uhr muss wirklich vorangekommen sein, sonst haette die
		 * Schleife ewig gedreht. */
		TT_TRUE(fb.clock_ms != 0xFFFFFF00u, "Uhr lief");
	}

	TT_CASE("Fehlerweg: Abbruch mitten in der Nachricht");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* Eine Antwort, von der nur die ersten vier Byte kommen -
		 * der Prolog ist da, die Nutzlast nicht. */
		static const uint8_t body[] = {0x41, 0x04, 1, 2, 3, 4, 0x90, 0x00};
		fb_add_frame(&fb, FB_TRUNCATE, SE050_NAD_TO_HOST, SE050_PCB_TYPE_I, body,
			     sizeof(body));
		fb_set_cut(&fb, 4u);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x02, 0x00, 0x00, 0x00};
		uint8_t rsp[64];
		size_t n = 123u;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_ERR_TIMEOUT, "halbe Antwort wird nicht genommen");
		TT_EQ_INT(n, 0u, "nichts geliefert");

		/* Danach muss der Treiber wissen, dass er aufraeumen muss -
		 * sonst laeuft er in Erratasheet I2C.1. */
		TT_TRUE(t1.needs_resync, "Treiber merkt den Bruch");
	}

	TT_CASE("Fehlerweg: nach einem Bruch wird erst zurueckgesetzt");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* Erst ein Abbruch. */
		static const uint8_t body[] = {0x90, 0x00};
		fb_add_frame(&fb, FB_TRUNCATE, SE050_NAD_TO_HOST, SE050_PCB_TYPE_I, body,
			     sizeof(body));
		fb_set_cut(&fb, 1u);

		/* Dann muss ein S-Block "Resync" kommen, nicht gleich der
		 * naechste Befehl. Der Testbus prueft das, weil die Antwort
		 * nur auf einen Resync passt. */
		const uint8_t rs = SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE | SE050_S_RESYNC;
		fb_add_frame(&fb, FB_OK, SE050_NAD_TO_HOST, rs, NULL, 0);
		fb_add_frame(&fb, FB_OK, SE050_NAD_TO_HOST, SE050_PCB_TYPE_I, body, sizeof(body));

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x02, 0x00, 0x00, 0x00};
		uint8_t rsp[64];
		size_t n = 0;
		TT_TRUE(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n) !=
			    OSK_OK,
			"erster Versuch scheitert");
		TT_TRUE(t1.needs_resync, "Bruch gemerkt");

		/* Zweiter Versuch: jetzt muss der Resync kommen und danach
		 * der Befehl durchgehen. */
		n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_OK, "nach dem Zuruecksetzen durch");
		TT_TRUE(!t1.needs_resync, "Zustand wieder sauber");
		TT_TRUE(!fb.mismatch, "der Resync kam tatsaechlich");
	}

	TT_CASE("Fehlerweg: unbrauchbare Aufrufparameter");
	{
		fake_bus_t fb;
		fb_init(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(NULL, &fb.bus), OSK_ERR_ARG, "kein t1");
		TT_EQ_INT(osk_se050_t1_open(&t1, NULL), OSK_ERR_ARG, "kein Bus");

		/* Ein Bus ohne die Pflichtfunktionen wird abgelehnt, statt
		 * spaeter in einen NULL-Aufruf zu laufen. */
		osk_se050_bus_t half;
		memset(&half, 0, sizeof(half));
		TT_EQ_INT(osk_se050_t1_open(&t1, &half), OSK_ERR_ARG, "halber Bus");

		/* Senden auf einer nicht geoeffneten Schicht. */
		memset(&t1, 0, sizeof(t1));
		t1.bus = &fb.bus;
		static const uint8_t cmd[] = {0x80};
		uint8_t rsp[8];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_ERR_IO, "nicht geoeffnet");
		TT_EQ_INT(osk_se050_t1_transceive(&t1, NULL, 0, rsp, sizeof(rsp), &n), OSK_ERR_ARG,
			  "kein Befehl");
	}

	TT_CASE("Fehlerweg: Schliessen ist immer erlaubt");
	{
		fake_bus_t fb;
		fb_init(&fb);

		osk_se050_t1_t t1;
		memset(&t1, 0, sizeof(t1));
		t1.bus = &fb.bus;
		/* Nicht geoeffnet: kein Befehl geht hinaus, kein Fehler. */
		TT_EQ_INT(osk_se050_t1_close(&t1), OSK_OK, "ungeoeffnet zu");
		TT_EQ_INT(fb.writes, 0u, "nichts gesendet");
		TT_EQ_INT(osk_se050_t1_close(NULL), OSK_ERR_ARG, "NULL");
	}
}

void test_errors(void)
{
	test_crypto_vectors();
	test_sw_mapping();
	test_bus_faults();
}
