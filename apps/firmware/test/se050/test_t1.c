/* T=1': Rahmenbau, Pruefung, Kettenbildung, Wiederholung, Zeitlimit.
 *
 * Alle Erwartungen stehen in se050_t1_wire.h. Wird dort ein Wert nach dem
 * Lesen von UM11225 berichtigt, bleiben diese Tests gueltig - sie pruefen
 * das VERHALTEN, nicht die Zahlen. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_t1.h"
#include "se050_crc.h"
#include "se050_t1_wire.h"

/* Die Gruppe "Drahtwerte" prueft Konstanten gegeneinander, also sind die
 * Bedingungen dort zur Uebersetzungszeit bekannt. Genau das ist der Zweck:
 * ein Tippfehler in se050_t1_wire.h soll auffallen. MSVC warnt darueber
 * (C4127), und /WX macht daraus einen Fehler - hier abgeschaltet, nur fuer
 * diese Datei. Es ist die einzige Stelle, an der eine Warnung weggeschaltet
 * wird. */
#ifdef _MSC_VER
#pragma warning(disable : 4127)
#endif

/* Einen I-Block als Antwort anhaengen. */
static void add_i(fake_bus_t *fb, uint8_t seq, bool more, const uint8_t *inf, uint8_t len)
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

/* Wie viele Schreibvorgaenge osk_se050_t1_open() selbst kostet:
 * S(SWR), S(CIP), S(IFS) - drei (GPC_SPE_172 Abschnitt 4.1).
 * Als Konstante, damit eine Aenderung am Oeffnen nicht jede
 * Zaehlpruefung einzeln trifft. */
#define WRITES_OPEN 3u

void test_t1(void)
{
	/* Diese erste Gruppe prueft nicht den Treiber, sondern die
	 * Drahtdatei selbst auf innere Stimmigkeit. Sie ist dafuer da, dass
	 * ein Tippfehler beim Nachtragen aus UM11225 sofort auffaellt und
	 * nicht erst als unerklaerliches Verhalten am Geraet. */

	TT_CASE("Drahtwerte: die drei Blockarten schliessen sich aus");
	{
		/* Jedes der 256 moeglichen PCB-Bytes muss genau EINER Art
		 * zugeordnet sein. Ueberschneiden sich die Pruefungen, wird
		 * irgendwo ein Block falsch behandelt. */
		for (unsigned p = 0; p < 256u; p++) {
			const uint8_t pcb = (uint8_t)p;
			const int n = (SE050_PCB_IS_I(pcb) ? 1 : 0) +
				      (SE050_PCB_IS_R(pcb) ? 1 : 0) + (SE050_PCB_IS_S(pcb) ? 1 : 0);
			TT_EQ_INT(n, 1, "genau eine Blockart je PCB-Byte");
		}
	}

	TT_CASE("Drahtwerte: Art, Folgenummer und Kennzeichen stoeren sich nicht");
	{
		/* Ein I-Block bleibt einer, egal wie Folgenummer und
		 * Kettenbit stehen. Genau das war beim Bau einmal falsch. */
		TT_TRUE(SE050_PCB_IS_I(SE050_PCB_TYPE_I), "I schlicht");
		TT_TRUE(SE050_PCB_IS_I(SE050_PCB_TYPE_I | SE050_PCB_I_SEQ), "I mit Folgenummer 1");
		TT_TRUE(SE050_PCB_IS_I(SE050_PCB_TYPE_I | SE050_PCB_I_MORE), "I mit Kettenbit");
		TT_TRUE(SE050_PCB_IS_I(SE050_PCB_TYPE_I | SE050_PCB_I_SEQ | SE050_PCB_I_MORE),
			"I mit beidem");

		/* Ein R-Block bleibt einer, egal welche Folgenummer und
		 * welcher Fehlergrund darin stehen. */
		TT_TRUE(SE050_PCB_IS_R(SE050_PCB_TYPE_R), "R schlicht");
		TT_TRUE(SE050_PCB_IS_R(SE050_PCB_TYPE_R | SE050_PCB_R_SEQ), "R mit Folgenummer 1");
		for (uint8_t e = 0; e <= SE050_PCB_R_ERR_MASK; e++) {
			TT_TRUE(SE050_PCB_IS_R(SE050_PCB_TYPE_R | e), "R mit jedem Fehlergrund");
		}

		/* Ein S-Block bleibt einer, egal welcher Code darin steht
		 * und ob es Anfrage oder Antwort ist. */
		TT_TRUE(SE050_PCB_IS_S(SE050_PCB_TYPE_S), "S schlicht");
		TT_TRUE(SE050_PCB_IS_S(SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE), "S als Antwort");
	}

	TT_CASE("Drahtwerte: S-Block-Codes passen in ihr Feld und sind verschieden");
	{
		/* Alle Codes muessen in SE050_PCB_S_CODE_MASK passen -
		 * sonst wandert ein Bit in das Antwortbit oder in die
		 * Blockart und macht aus dem Befehl einen anderen. */
		static const uint8_t codes[] = {
		    SE050_S_RESYNC, SE050_S_IFS,     SE050_S_ABORT, SE050_S_WTX,
		    SE050_S_CIP,    SE050_S_RELEASE, SE050_S_SWR,
		};
		for (size_t i = 0; i < sizeof(codes); i++) {
			TT_EQ_INT(codes[i] & ~SE050_PCB_S_CODE_MASK, 0,
				  "Code passt in das Codefeld");

			/* Und der fertige S-Block muss noch als S erkannt
			 * werden, als Anfrage wie als Antwort. */
			const uint8_t req = (uint8_t)(SE050_PCB_TYPE_S | codes[i]);
			const uint8_t rsp = (uint8_t)(req | SE050_PCB_S_RESPONSE);
			TT_TRUE(SE050_PCB_IS_S(req), "Anfrage bleibt S-Block");
			TT_TRUE(SE050_PCB_IS_S(rsp), "Antwort bleibt S-Block");
			TT_EQ_INT(req & SE050_PCB_S_CODE_MASK, codes[i],
				  "Code kommt unveraendert heraus");
			TT_TRUE((req & SE050_PCB_S_RESPONSE) == 0,
				"Anfrage hat das Antwortbit nicht");

			/* Keine zwei Codes duerfen gleich sein - sonst
			 * beantwortet der Treiber den falschen S-Block. */
			for (size_t j = i + 1u; j < sizeof(codes); j++) {
				TT_TRUE(codes[i] != codes[j],
					"S-Block-Codes sind paarweise verschieden");
			}
		}
	}

	TT_CASE("Drahtwerte: Laengen und Grenzen sind stimmig");
	{
		TT_EQ_INT(SE050_T1_OVERHEAD, SE050_T1_PROLOGUE_LEN + SE050_T1_EPILOGUE_LEN,
			  "Prolog plus Epilog");
		TT_EQ_INT(SE050_T1_EPILOGUE_LEN, 2u, "zwei CRC-Bytes");

		/* Was der Treiber selbst sendet, muss in ein LEN-Byte
		 * passen - sonst baut er Rahmen, die er nicht beschreiben
		 * kann. */
		/* Was wir ankuendigen, muss das Protokoll auch zulassen. */
		TT_TRUE(SE050_IFSD <= SE050_T1_MAX_INF, "unser IFSD passt ins Protokoll");
		TT_TRUE(SE050_T1_MAX_INF <= 4089u, "Protokollgrenze aus Abschnitt 4.2.5");
		/* Die Vorgabe muss fuer eine S(CIP request) reichen - genau
		 * dafuer nennt die Beschreibung acht Byte. */
		TT_TRUE(SE050_IFSC_DEFAULT >= 8u, "Vorgabe-IFSC reicht fuer S(CIP)");
		/* Ein Prolog mit zweibytigem LEN ist vier Byte lang. */
		TT_EQ_INT(SE050_T1_PROLOGUE_LEN, 4u, "NAD, PCB, LEN hoch, LEN niedrig");

		/* Hin- und Rueckrichtung muessen unterscheidbar sein. */
		TT_TRUE(SE050_NAD_TO_SE != SE050_NAD_TO_HOST,
			"die beiden NAD-Bytes sind verschieden");

		/* Zeitgrenzen muessen in der richtigen Ordnung stehen. */
		TT_TRUE(SE050_T_WTX_MAX_MS >= SE050_BWT_DEFAULT_MS,
			"Obergrenze nicht unter dem Grundbudget");
		TT_TRUE(SE050_T_POLL_MS >= 1u, "Pause zwischen Leseversuchen nicht Null");
		TT_TRUE(SE050_MAX_RETRY >= 1u, "mindestens eine Wiederholung");
	}

	TT_CASE("Laengenfeld: zwei Byte, hoeherwertiges zuerst");
	{
		/* Das 2-Byte-LEN ist der einzige echte Formatunterschied zu
		 * UM11225 - und ein Feld, das ploetzlich doppelt so breit
		 * ist, ist genau die Stelle fuer einen Ueberlauf. Darum hier
		 * die Raender einzeln. */
		static uint8_t inf[600];
		for (size_t i = 0; i < sizeof(inf); i++) {
			inf[i] = (uint8_t)(i & 0xFFu);
		}

		uint8_t frame[SE050_T1_OVERHEAD + sizeof(inf)];

		/* Eine Laenge ueber 255: genau hier war frueher Schluss. */
		const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE,
						    SE050_PCB_TYPE_I, inf, 300u);
		TT_EQ_INT(n, SE050_T1_OVERHEAD + 300u, "Gesamtlaenge");
		TT_EQ_INT(frame[2], 0x01u, "LEN hoeherwertig: 300 = 0x012C");
		TT_EQ_INT(frame[3], 0x2Cu, "LEN niederwertig");
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_OK, "prueft sich selbst");
		TT_EQ_MEM(&frame[SE050_T1_PROLOGUE_LEN], inf, 300u, "Nutzlast unveraendert");

		/* Genau an der Grenze zwischen ein und zwei Byte. */
		for (uint16_t len = 254u; len <= 257u; len++) {
			const size_t m = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE,
							    SE050_PCB_TYPE_I, inf, len);
			TT_EQ_INT(m, SE050_T1_OVERHEAD + len, "Laenge an der Grenze");
			TT_EQ_INT(((size_t)frame[2] << 8) | frame[3], len, "LEN gelesen");
			TT_EQ_INT(osk_se050_t1_check(frame, m), OSK_OK, "gueltig");
		}

		/* Ueber der Protokollgrenze wird nichts gebaut, statt die
		 * Laenge stillschweigend zu beschneiden. */
		TT_EQ_INT(osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE,
					     SE050_PCB_TYPE_I, inf,
					     (uint16_t)(SE050_T1_MAX_INF + 1u)),
			  0u, "ueber der Protokollgrenze abgelehnt");
	}

	TT_CASE("Laengenfeld: eine erlogene Laenge faellt auf");
	{
		/* Ein Baustein, der im Prolog mehr ankuendigt, als er
		 * schickt - der Fall, in dem ein Leser ueber das Ende
		 * hinauslaufen wuerde. */
		static const uint8_t inf[] = {0xAA, 0xBB, 0xCC, 0xDD};
		uint8_t frame[32];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_HOST,
						    SE050_PCB_TYPE_I, inf, sizeof(inf));

		/* LEN auf 0x0100 hochdrehen, Laenge des Puffers lassen. */
		frame[2] = 0x01u;
		frame[3] = 0x00u;
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_ERR_FORMAT, "erlogene Laenge");

		/* Und andersherum: LEN kleiner als der Puffer. */
		frame[2] = 0x00u;
		frame[3] = 0x01u;
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_ERR_FORMAT, "zu kleine Laenge");
	}

	TT_CASE("Rahmenbau: Prolog, Nutzlast, Pruefsumme");
	{
		static const uint8_t inf[] = {0xAA, 0xBB, 0xCC};
		uint8_t frame[16];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE,
						    SE050_PCB_TYPE_I, inf, sizeof(inf));
		TT_EQ_INT(n, SE050_T1_OVERHEAD + sizeof(inf), "Gesamtlaenge");
		TT_EQ_INT(frame[0], SE050_NAD_TO_SE, "NAD");
		TT_EQ_INT(frame[1], SE050_PCB_TYPE_I, "PCB");
		TT_EQ_INT(frame[2], 0u, "LEN hoeherwertig");
		TT_EQ_INT(frame[3], sizeof(inf), "LEN niederwertig");
		TT_EQ_MEM(&frame[SE050_T1_PROLOGUE_LEN], inf, sizeof(inf), "Nutzlast");

		/* Pruefsumme laeuft ueber Prolog UND Nutzlast. Welches der
		 * zwei Bytes vorn steht, sagt SE050_CRC_LSB_FIRST - der Test
		 * liest den Schalter, statt eine Reihenfolge anzunehmen.
		 * Sonst wuerde er fehlschlagen, wenn UM11225 die andere
		 * Reihenfolge nennt und der Schalter richtig umgestellt ist. */
		const uint16_t crc = osk_se050_crc16(frame, SE050_T1_PROLOGUE_LEN + sizeof(inf));
		const uint8_t lo = (uint8_t)(crc & 0xFFu);
		const uint8_t hi = (uint8_t)(crc >> 8);
		const uint8_t *epi = &frame[SE050_T1_PROLOGUE_LEN + sizeof(inf)];
#if SE050_CRC_LSB_FIRST
		TT_EQ_INT(epi[0], lo, "CRC: niederwertiges Byte vorn");
		TT_EQ_INT(epi[1], hi, "CRC: hoeherwertiges Byte hinten");
#else
		TT_EQ_INT(epi[0], hi, "CRC: hoeherwertiges Byte vorn");
		TT_EQ_INT(epi[1], lo, "CRC: niederwertiges Byte hinten");
#endif

		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_OK, "prueft sich selbst");
	}

	TT_CASE("Rahmenbau: Rahmen ohne Nutzlast ist erlaubt");
	{
		uint8_t frame[8];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE,
						    SE050_PCB_TYPE_R, NULL, 0);
		TT_EQ_INT(n, SE050_T1_OVERHEAD, "nur Prolog und Epilog");
		TT_EQ_INT(frame[2], 0u, "LEN hoeherwertig ist 0");
		TT_EQ_INT(frame[3], 0u, "LEN niederwertig ist 0");
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_OK, "gueltig");
	}

	TT_CASE("Rahmenbau: zu kleiner Puffer gibt 0");
	{
		uint8_t frame[4];
		static const uint8_t inf[] = {1, 2, 3};
		TT_EQ_INT(
		    osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE, 0, inf, sizeof(inf)),
		    0u, "passt nicht");
		TT_EQ_INT(osk_se050_t1_build(NULL, 64, 0, 0, NULL, 0), 0u, "NULL-Puffer");
		TT_EQ_INT(osk_se050_t1_build(frame, sizeof(frame), 0, 0, NULL, 3), 0u,
			  "Laenge ohne Daten");
	}

	TT_CASE("Rahmenpruefung: verdorbene Pruefsumme faellt auf");
	{
		static const uint8_t inf[] = {0x11, 0x22};
		uint8_t frame[16];
		const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_HOST,
						    SE050_PCB_TYPE_I, inf, sizeof(inf));
		frame[n - 1u] ^= 0x80u;
		TT_EQ_INT(osk_se050_t1_check(frame, n), OSK_ERR_FORMAT, "Pruefsumme falsch");
	}

	TT_CASE("Rahmenpruefung: LEN passt nicht zur Laenge");
	{
		uint8_t frame[16];
		const size_t n =
		    osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_HOST, 0, NULL, 0);
		TT_EQ_INT(osk_se050_t1_check(frame, n - 1u), OSK_ERR_FORMAT, "zu kurz");
		TT_EQ_INT(osk_se050_t1_check(frame, n + 1u), OSK_ERR_FORMAT, "zu lang");
		TT_EQ_INT(osk_se050_t1_check(frame, 2), OSK_ERR_FORMAT, "kuerzer als der Prolog");
		TT_EQ_INT(osk_se050_t1_check(NULL, 8), OSK_ERR_FORMAT, "NULL");
	}

	TT_CASE("Austausch: ein Befehl, eine Antwort");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		static const uint8_t answer[] = {0x41, 0x02, 0xDE, 0xAD, 0x90, 0x00};
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x04, 0x00, 0x20, 0x00};
		uint8_t rsp[64];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
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

		/* Der Testbus nennt in der CIP ein IFSC von 254 Byte. Ein
		 * Befehl darueber muss in zwei Bloecke zerfallen. Die Zahl
		 * kommt aus der Antwort des Bausteins, nicht aus einer
		 * Annahme im Test. */
		uint8_t cmd[254u + 10u];
		for (size_t i = 0; i < sizeof(cmd); i++) {
			cmd[i] = (uint8_t)i;
		}

		/* Erster Block: Quittung. Zweiter Block: die Antwort. */
		add_r(&fb, 1, SE050_PCB_R_ERR_NONE);
		static const uint8_t answer[] = {0x90, 0x00};
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_OK, "lange Nachricht durch");
		TT_EQ_INT(n, sizeof(answer), "Antwortlaenge");

		/* Bis hierher: das Oeffnen und zwei I-Bloecke. Das
		 * Schliessen kommt erst danach und zaehlt noch nicht mit. */
		TT_EQ_INT(fb.writes, WRITES_OPEN + 2u, "Oeffnen und zwei I-Bloecke");

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

		static const uint8_t cmd[] = {0x80, 0x02, 0x00, 0x00, 0x00};
		uint8_t rsp[512];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_OK, "Kette eingesammelt");
		TT_EQ_INT(n, sizeof(part1) + sizeof(part2), "Gesamtlaenge");
		TT_EQ_MEM(rsp, part1, sizeof(part1), "erstes Stueck");
		TT_EQ_MEM(&rsp[sizeof(part1)], part2, sizeof(part2), "zweites Stueck");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Wiederholung: abgewiesener Rahmen wird erneut geschickt");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		/* So oft abweisen, wie der Treiber nach SE050_MAX_RETRY noch
		 * durchhalten muss, dann annehmen. Die Zahl kommt aus der
		 * Drahtdatei, damit das Nachtragen aus UM11225 den Test
		 * mitzieht statt ihn zu brechen. */
		for (uint32_t i = 0; i < SE050_MAX_RETRY; i++) {
			add_r(&fb, 0, (i % 2u == 0u) ? SE050_PCB_R_ERR_CRC : SE050_PCB_R_ERR_OTHER);
		}
		static const uint8_t answer[] = {0x90, 0x00};
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x04, 0x00, 0x20, 0x00};
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_OK, "nach allen erlaubten Wiederholungen durch");
		/* Das Oeffnen, dann der Erstversuch plus SE050_MAX_RETRY
		 * Wiederholungen. */
		TT_EQ_INT(fb.writes, WRITES_OPEN + 1u + SE050_MAX_RETRY,
			  "Erstversuch und alle Wiederholungen gesendet");

		(void)osk_se050_t1_close(&t1);
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");
	}

	TT_CASE("Wiederholung: verdorbene Pruefsumme loest eine aus");
	{
		fake_bus_t fb;
		fb_init(&fb);
		fb_add_open_reply(&fb);

		static const uint8_t answer[] = {0x90, 0x00};
		/* Erst eine Antwort mit falscher Pruefsumme, dann eine gute. */
		fb_add_frame(&fb, FB_BAD_CRC, SE050_NAD_TO_HOST, SE050_PCB_TYPE_I, answer,
			     sizeof(answer));
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x04, 0x00, 0x20, 0x00};
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
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

		static const uint8_t answer[] = {0x90, 0x00};
		add_i(&fb, 0, false, answer, sizeof(answer));
		fb_add_close_reply(&fb);

		osk_se050_t1_t t1;
		TT_EQ_INT(osk_se050_t1_open(&t1, &fb.bus), OSK_OK, "geoeffnet");

		static const uint8_t cmd[] = {0x80, 0x03, 0x0C, 0x09, 0x00};
		uint8_t rsp[32];
		size_t n = 0;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_OK, "mit Verlaengerung durch");
		/* Der Befehl selbst darf NICHT noch einmal gesendet werden -
		 * nur die Bestaetigung der Verlaengerung kommt dazu:
		 * Oeffnen, Befehl, WTX-Antwort. */
		TT_EQ_INT(fb.writes, WRITES_OPEN + 2u, "Befehl nur einmal gesendet");

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

		static const uint8_t cmd[] = {0x80, 0x02, 0x00, 0x00, 0x00};
		uint8_t rsp[16];
		size_t n = 123u;
		TT_EQ_INT(osk_se050_t1_transceive(&t1, cmd, sizeof(cmd), rsp, sizeof(rsp), &n),
			  OSK_ERR_FORMAT, "zu gross gemeldet");
		TT_EQ_INT(n, 0u, "keine halbe Antwort");
		TT_EQ_INT(fb.empty_writes, 0u, "kein leerer Rahmen");
	}
}
