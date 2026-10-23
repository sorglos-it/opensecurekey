/* T=1': Rahmenbau, Pruefung, Kettenbildung, Wiederholung, Zeitlimit.
 *
 * Alle Erwartungen stehen in se050_wire.h. Wird dort ein Wert nach dem
 * Lesen von UM11225 berichtigt, bleiben diese Tests gueltig - sie pruefen
 * das VERHALTEN, nicht die Zahlen. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_t1.h"
#include "se050_crc.h"
#include "se050_wire.h"

/* Einen I-Block als Antwort anhaengen. */
static void add_i(fake_bus_t *fb, uint8_t seq, bool more,
		  const uint8_t *inf, uint8_t len)
{
	uint8_t pcb = SE050_PCB_TYPE_I;
	if (seq) {
		pcb |= SE050_PCB_I_SEQ;
	}
	if (more) {
		pcb |= SE050_PCB_I_MORE;
	}
	fb_add_frame(fb, FB_OK, SE050_NAD_TO_HOST, pcb, inf, len);
}

/* Einen R-Block als Antwort anhaengen. */
static void add_r(fake_bus_t *fb, uint8_t seq, uint8_t err)
{
	uint8_t pcb = (uint8_t)(SE050_PCB_TYPE_R | (err & SE050_PCB_R_ERR_MASK));
	if (seq) {
		pcb |= SE050_PCB_R_SEQ;
	}
	fb_add_frame(fb, FB_OK, SE050_NAD_TO_HOST, pcb, NULL, 0);
}

void test_t1(void)
{
	TT_CASE("Rahmenbau: Prolog, Nutzlast, Pruefsumme");
	{
		static const uint8_t inf[] = { 0xAA, 0xBB, 0xCC };
		uint8_t frame[16];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame),
						    SE050_NAD_TO_SE,
						    SE050_PCB_TYPE_I,
						    inf, sizeof(inf));
		TT_EQ_INT(n, SE050_T1_OVERHEAD + sizeof(inf), "Gesamtlaenge");
		TT_EQ_INT(frame[0], SE050_NAD_TO_SE, "NAD");
		TT_EQ_INT(frame[1], SE050_PCB_TYPE_I, "PCB");
		TT_EQ_INT(frame[2], sizeof(inf), "LEN");
		TT_EQ_MEM(&frame[3], inf, sizeof(inf), "Nutzlast");

		/* Pruefsumme laeuft ueber Prolog UND Nutzlast. */
		const uint16_t crc = osk_se050_crc16(frame, 3u + sizeof(inf));
		TT_EQ_INT(frame[6], (uint8_t)(crc & 0xFFu), "CRC niederwertig");
		TT_EQ_INT(frame[7], (uint8_t)(crc >> 8), "CRC hoeherwertig");

		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_OK, "prueft sich selbst");
	}

	TT_CASE("Rahmenbau: Rahmen ohne Nutzlast ist erlaubt");
	{
		uint8_t frame[8];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame),
						    SE050_NAD_TO_SE,
						    SE050_PCB_TYPE_R, NULL, 0);
		TT_EQ_INT(n, SE050_T1_OVERHEAD, "nur Prolog und Epilog");
		TT_EQ_INT(frame[2], 0u, "LEN ist 0");
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_OK, "gueltig");
	}

	TT_CASE("Rahmenbau: zu kleiner Puffer gibt 0");
	{
		uint8_t frame[4];
		static const uint8_t inf[] = { 1, 2, 3 };
		TT_EQ_INT(osk_se050_t1_build(frame, sizeof(frame),
					     SE050_NAD_TO_SE, 0, inf,
					     sizeof(inf)),
			  0u, "passt nicht");
		TT_EQ_INT(osk_se050_t1_build(NULL, 64, 0, 0, NULL, 0), 0u,
			  "NULL-Puffer");
		TT_EQ_INT(osk_se050_t1_build(frame, sizeof(frame), 0, 0, NULL, 3),
			  0u, "Laenge ohne Daten");
	}

	TT_CASE("Rahmenpruefung: verdorbene Pruefsumme faellt auf");
	{
		static const uint8_t inf[] = { 0x11, 0x22 };
		uint8_t frame[16];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame),
						    SE050_NAD_TO_HOST,
						    SE050_PCB_TYPE_I,
						    inf, sizeof(inf));
		frame[n - 1u] ^= 0x80u;
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_ERR_FORMAT,
			  "Pruefsumme falsch");
	}

	TT_CASE("Rahmenpruefung: LEN passt nicht zur Laenge");
	{
		uint8_t frame[16];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame),
						    SE050_NAD_TO_HOST, 0,
						    NULL, 0);
		TT_EQ_INT(osk_se050_t1_check(frame, n - 1u), OSK_ERR_FORMAT,
			  "zu kurz");
		TT_EQ_INT(osk_se050_t1_check(frame, n + 1u), OSK_ERR_FORMAT,
			  "zu lang");
		TT_EQ_INT(osk_se050_t1_check(frame, 2), OSK_ERR_FORMAT,
			  "kuerzer als der Prolog");
		TT_EQ_INT(osk_se050_t1_check(NULL, 8), OSK_ERR_FORMAT, "NULL");
	}

	TT_CASE("Austausch: ein Befehl, eine Antwort");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		static const uint8_t answer[] = { 0x41, 0x02, 0xDE, 0xAD,
						  0x90, 0x00 };
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x04, 0x00, 0x20, 0x00 };
		uint8_t rsp[64];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "ausgetauscht");
		TT_EQ_INT(n, sizeof(answer), "Antwortlaenge");
		TT_EQ_MEM(rsp, answer, sizeof(answer), "Antwortinhalt");

		TT_EQ_INT(osk_se050_t1_close(&t1), OSK_OK, "geschlossen");
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Kettenbildung Hinweg: langer Befehl wird zerlegt");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* Der Treiber darf hoechstens SE050_T1_MAX_INF je Block
		 * schicken. Ein Befehl darueber muss in zwei Bloecke. */
		uint8_t cmd[SE050_T1_MAX_INF + 10u];
		for (size_t i = 0; i < sizeof(cmd); i++) {
			cmd[i] = (uint8_t)i;
		}

		/* Erster Block: Quittung. Zweiter Block: die Antwort. */
		add_r(&fb, 1, SE050_PCB_R_ERR_NONE);
		static const uint8_t answer[] = { 0x90, 0x00 };
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "lange Nachricht durch");
		TT_EQ_INT(n, sizeof(answer), "Antwortlaenge");

		/* Bis hierher: Soft-Reset und zwei I-Bloecke. Das Schliessen
		 * kommt erst danach und zaehlt hier noch nicht mit. */
		TT_EQ_INT(fb.writes, 3u, "Soft-Reset und zwei I-Bloecke");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Kettenbildung Rueckweg: lange Antwort wird zusammengesetzt");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		uint8_t part1[200];
		uint8_t part2[100];
		for (size_t i = 0; i < sizeof(part1); i++) {
			part1[i] = (uint8_t)(i + 1u);
		}
		for (size_t i = 0; i < sizeof(part2); i++) {
			part2[i] = (uint8_t)(i + 0x80u);
		}

		add_i(&fb, 0, true, part1, sizeof(part1));
		add_i(&fb, 1, false, part2, sizeof(part2));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x02, 0x00, 0x00, 0x00 };
		uint8_t rsp[512];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "Kette eingesammelt");
		TT_EQ_INT(n, sizeof(part1) + sizeof(part2), "Gesamtlaenge");
		TT_EQ_MEM(rsp, part1, sizeof(part1), "erstes Stueck");
		TT_EQ_MEM(&rsp[sizeof(part1)], part2, sizeof(part2),
			  "zweites Stueck");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Wiederholung: abgewiesener Rahmen wird erneut geschickt");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* Zweimal abweisen, dann annehmen - der Treiber darf nach
		 * SE050_MAX_RETRY Versuchen aufgeben, also reichen zwei. */
		add_r(&fb, 0, SE050_PCB_R_ERR_CRC);
		add_r(&fb, 0, SE050_PCB_R_ERR_OTHER);
		static const uint8_t answer[] = { 0x90, 0x00 };
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x04, 0x00, 0x20, 0x00 };
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "nach zwei Wiederholungen durch");
		/* Soft-Reset und drei Versuche. */
		TT_EQ_INT(fb.writes, 4u, "dreimal gesendet");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Wiederholung: verdorbene Pruefsumme loest eine aus");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		static const uint8_t answer[] = { 0x90, 0x00 };
		/* Erst eine Antwort mit falscher Pruefsumme, dann eine gute. */
		fb_add_frame(&fb, FB_BAD_CRC, SE050_NAD_TO_HOST,
			     SE050_PCB_TYPE_I, answer, sizeof(answer));
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x04, 0x00, 0x20, 0x00 };
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "nach Wiederholung durch");
		TT_EQ_MEM(rsp, answer, sizeof(answer), "Antwort");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Zeit: der Baustein bittet um mehr und bekommt sie");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* S-Block WTX als Anfrage: "ich brauche 2 Einheiten laenger". */
		static const uint8_t units = 2u;
		const uint8_t wtx = SE050_PCB_TYPE_S | SE050_S_WTX;
		fb_add_frame(&fb, FB_OK, SE050_NAD_TO_HOST, wtx, &units, 1);

		static const uint8_t answer[] = { 0x90, 0x00 };
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x03, 0x0C, 0x09, 0x00 };
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_OK, "mit Verlaengerung durch");
		/* Der Befehl selbst darf NICHT noch einmal gesendet werden -
		 * nur die Bestaetigung der Verlaengerung kommt dazu:
		 * Soft-Reset, Befehl, WTX-Antwort. */
		TT_EQ_INT(fb.writes, 3u, "Befehl nur einmal gesendet");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Puffer: Antwort passt nicht und wird nicht halb geliefert");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		uint8_t big[200];
		memset(big, 0x5A, sizeof(big));
		add_i(&fb, 0, false, big, sizeof(big));

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = { 0x80, 0x02, 0x00, 0x00, 0x00 };
		uint8_t rsp[16];
		size_t n = 123u;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd),
						  rsp, sizeof(rsp), &n),
			  OSK_ERR_FORMAT, "zu gross gemeldet");
		TT_EQ_INT(n, 0u, "keine halbe Antwort");
		TT_EQ_INT(fb.empty_writes, 0u, "kein leerer Rahmen");
	}
}
