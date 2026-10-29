/* Fuzzlauf ueber den Yubico-HID-Parser.
 *
 * Warum dieser Parser und nicht ein anderer: jede Software auf dem
 * Rechner darf HID-Feature-Berichte schicken - ohne Rechte, ohne
 * Nachfrage. Das ist die kuerzeste Strecke von einem Angreifer zu einem
 * Parser, die dieses Geraet hat.
 *
 * Aufruf: fuzz_ykhid [Sekunden] [Startwert]
 *
 * Was geworfen wird:
 *   - vollstaendig zufaellige Berichte
 *   - gueltige Rahmen (sonst trifft der Lauf nur die erste Pruefung)
 *   - gueltige Rahmen mit einem gekippten Byte
 *   - abgebrochene Rahmen: Anfang ohne Ende
 *   - Stuecke in verdrehter Reihenfolge, doppelt, rueckwaerts
 *   - Folgenummern, die springen, und solche ausserhalb des Rahmens
 *   - Laengen- und Nummernfelder, die groesser sind als der Rahmen
 *   - Ruecksetz- und Leerberichte an beliebiger Stelle
 *   - Abholen und Uhrticks zwischen allem
 *
 * Zusicherungen:
 *   1. Kein Bericht darf den Prozess umbringen.
 *   2. Ein Abholen schreibt GENAU 8 Byte. Um den Puffer liegen
 *      Wachbytes; wird eines angefasst, bricht der Lauf ab. Das ist die
 *      Pruefung, die ein zu grosser Bericht ausloest.
 *   3. Der Bearbeiter wird genau so oft gerufen, wie Rahmen angenommen
 *      wurden (plus die Freigaben, die der Lauf selbst erteilt). Ein
 *      Rahmen, der nicht angenommen wurde, darf NIE beim Bearbeiter
 *      ankommen - das ist die eigentliche Zusicherung des Parsers.
 *   4. Eine Antwort endet immer. Ein Zustand, der endlos Stuecke
 *      liefert, haengt den Host auf.
 *   5. Der Lauf muss gueltige Rahmen durchbekommen UND ungueltige
 *      abweisen. Fehlt eines von beiden, hat er nicht gemessen, was er
 *      zu messen vorgibt. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "yk_hid.h"
#include "yk_frame.h"
#include "yk_crc.h"

static uint64_t g_rng;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static uint32_t rnd_below(uint32_t n)
{
	return n ? (rnd() % n) : 0u;
}

/* --- Bearbeiter ------------------------------------------------------- */

static unsigned long handler_calls;
static unsigned long handler_denied;
static int deny_next;

static osk_result_t handler(uint8_t slot, const uint8_t payload[YK_SLOT_DATA_LEN], uint8_t *out,
			    size_t cap, size_t *out_len, void *ctx)
{
	(void)slot;
	(void)payload;
	(void)ctx;
	handler_calls++;

	if (deny_next) {
		deny_next = 0;
		handler_denied++;
		*out_len = 0u;
		return OSK_ERR_DENIED;
	}

	/* Eine Antwort wechselnder Laenge, damit alle Zerlegungswege
	 * vorkommen - auch 0 und die Obergrenze. */
	size_t n = (size_t)rnd_below((uint32_t)YK_RESP_MAX + 1u);

	if (n > cap)
		n = cap;
	for (size_t i = 0; i < n; i++)
		out[i] = (uint8_t)rnd();
	*out_len = n;
	return OSK_OK;
}

/* --- Abholen mit Wachbytes -------------------------------------------- */

/* Der Bericht liegt mitten in einem groesseren Puffer. Was ausserhalb
 * der 8 Byte liegt, muss unberuehrt bleiben. */
#define GUARD 16
static uint8_t box[GUARD + YK_RPT_LEN + GUARD];

static int get_report_guarded(uint8_t out[YK_RPT_LEN])
{
	memset(box, 0xa5, sizeof(box));
	yk_hid_get_report(box + GUARD);

	for (size_t i = 0; i < GUARD; i++) {
		if (box[i] != 0xa5u || box[GUARD + YK_RPT_LEN + i] != 0xa5u) {
			printf("FEHLER: ein Abholen hat neben den Bericht "
			       "geschrieben (Wachbyte %zu)\n",
			       i);
			return 0;
		}
	}
	memcpy(out, box + GUARD, YK_RPT_LEN);
	return 1;
}

/* Holt eine Antwort komplett ab. Liefert 0, wenn sie nicht endet. */
static int drain(unsigned long *reports)
{
	uint8_t rpt[YK_RPT_LEN];
	unsigned n = 0;
	const unsigned limit = YK_RESP_CHUNKS_MAX + 4u;

	for (;;) {
		if (!get_report_guarded(rpt))
			return 0;
		if ((rpt[YK_RPT_STATUS] & YK_FLAG_RESP_PENDING) == 0u)
			return 1; /* Status- oder Wartebericht: fertig */
		n++;
		if (reports)
			(*reports)++;
		if (n > 0u && (rpt[YK_RPT_STATUS] & YK_SEQ_MASK) == 0u && n > 1u)
			return 1; /* Abschluss */
		if (n > limit) {
			printf("FEHLER: eine Antwort endet nicht (%u Stuecke, "
			       "Grenze %u)\n",
			       n, limit);
			return 0;
		}
	}
}

/* --- Berichte werfen -------------------------------------------------- */

static void fill_random_frame(uint8_t frame[YK_FRAME_LEN])
{
	uint8_t payload[YK_SLOT_DATA_LEN];
	const size_t len = (size_t)rnd_below(YK_SLOT_DATA_LEN + 1u);

	for (size_t i = 0; i < len; i++)
		payload[i] = (uint8_t)rnd();
	yk_frame_build((uint8_t)rnd(), payload, len, frame);
}

int main(int argc, char **argv)
{
	unsigned secs = 20u;
	uint8_t rpt[YK_RPT_LEN];
	uint8_t frame[YK_FRAME_LEN];
	yk_hid_status_t st;
	unsigned long rounds = 0, reports_in = 0, reports_out = 0;
	unsigned long touch_grants = 0;
	time_t start;

	if (argc > 1)
		secs = (unsigned)strtoul(argv[1], NULL, 10);
	g_rng = (argc > 2) ? strtoull(argv[2], NULL, 10) : 0x5eed1234abcdef01ull;
	if (g_rng == 0u)
		g_rng = 0x9e3779b97f4a7c15ull;

	memset(&st, 0, sizeof(st));
	st.version[0] = 5u;
	st.version[1] = 7u;
	st.version[2] = 1u;
	if (yk_hid_init(handler, NULL, &st) != OSK_OK) {
		printf("FEHLER: Anmeldung fehlgeschlagen\n");
		return 1;
	}

	start = time(NULL);
	while ((unsigned)(time(NULL) - start) < secs) {
		/* Ein Block Arbeit, dann wieder auf die Uhr schauen. */
		for (unsigned k = 0; k < 2000u; k++) {
			const uint32_t what = rnd_below(100u);

			rounds++;
			/* Die Uhr springt zufaellig, damit Ablaeufe mitten
			 * im Zusammenbau greifen. */
			if (rnd_below(8u) == 0u)
				yk_hid_tick((uint32_t)rnd());

			if (what < 30u) {
				/* Vollstaendig zufaelliger Bericht. */
				for (size_t i = 0; i < YK_RPT_LEN; i++)
					rpt[i] = (uint8_t)rnd();
				yk_hid_set_report(rpt);
				reports_in++;
			} else if (what < 40u) {
				/* Zufaelliges Statusbyte, zufaellige Daten -
				 * trifft die Nummern 10 bis 31, also den
				 * Bereich ausserhalb des Rahmens. */
				for (size_t i = 0; i < YK_RPT_DATA; i++)
					rpt[i] = (uint8_t)rnd();
				rpt[YK_RPT_STATUS] =
				    (uint8_t)(YK_FLAG_WRITE | (uint8_t)rnd_below(32u));
				yk_hid_set_report(rpt);
				reports_in++;
			} else if (what < 55u) {
				/* Ein gueltiger Rahmen, ordentlich geschickt. */
				fill_random_frame(frame);
				if (rnd_below(10u) == 0u)
					deny_next = 1;
				for (unsigned seq = 0; seq < YK_FRAME_CHUNKS; seq++) {
					if (!yk_frame_chunk_needed(frame, seq))
						continue;
					yk_frame_chunk(frame, seq, rpt);
					yk_hid_set_report(rpt);
					reports_in++;
				}
				if (!drain(&reports_out))
					return 1;
				/* Wartete er auf den Taster, dann freigeben
				 * oder verweigern. */
				if (rnd_below(2u) == 0u) {
					yk_hid_touch_done(true);
					touch_grants++;
					if (!drain(&reports_out))
						return 1;
				} else {
					yk_hid_touch_done(false);
				}
			} else if (what < 65u) {
				/* Gueltiger Rahmen mit einem gekippten Byte. */
				fill_random_frame(frame);
				frame[rnd_below(YK_FRAME_LEN)] ^=
				    (uint8_t)(1u << rnd_below(8u));
				for (unsigned seq = 0; seq < YK_FRAME_CHUNKS; seq++) {
					yk_frame_chunk(frame, seq, rpt);
					yk_hid_set_report(rpt);
					reports_in++;
				}
				if (!drain(&reports_out))
					return 1;
			} else if (what < 78u) {
				/* Abgebrochener Rahmen: Anfang ohne Ende. */
				fill_random_frame(frame);
				const unsigned upto = 1u + rnd_below(YK_FRAME_CHUNKS - 1u);

				for (unsigned seq = 0; seq < upto; seq++) {
					yk_frame_chunk(frame, seq, rpt);
					yk_hid_set_report(rpt);
					reports_in++;
				}
			} else if (what < 90u) {
				/* Stuecke in verdrehter Reihenfolge: doppelt,
				 * rueckwaerts, mit Spruengen. */
				fill_random_frame(frame);
				const unsigned n = 1u + rnd_below(14u);

				for (unsigned i = 0; i < n; i++) {
					yk_frame_chunk(frame, rnd_below(YK_FRAME_CHUNKS), rpt);
					yk_hid_set_report(rpt);
					reports_in++;
				}
				if (!drain(&reports_out))
					return 1;
			} else if (what < 95u) {
				/* Ruecksetzbericht. */
				memset(rpt, 0xff, sizeof(rpt));
				yk_hid_set_report(rpt);
				reports_in++;
			} else {
				/* Leerbericht. */
				memset(rpt, 0, sizeof(rpt));
				rpt[YK_RPT_STATUS] = (uint8_t)YK_DUMMY_REPORT;
				yk_hid_set_report(rpt);
				reports_in++;
			}

			/* Zwischendurch abholen, auch wenn nichts ansteht. */
			if (rnd_below(3u) == 0u) {
				if (!get_report_guarded(rpt))
					return 1;
			}
		}
	}

	/* --- Zusicherungen nachrechnen --- */
	yk_hid_counts_t c;

	yk_hid_counts(&c);

	printf("fuzz_ykhid: %lu Runden, %lu Berichte herein, %lu Antwortstuecke "
	       "heraus\n",
	       rounds, reports_in, reports_out);
	printf("  Rahmen angenommen %u, abgewiesen %u, Ruecksetzen %u, "
	       "Abbrueche %u, Ablaeufe %u\n",
	       c.frames_ok, c.frames_bad, c.resets, c.aborts, c.timeouts);
	printf("  Bearbeiter gerufen %lu (davon %lu mal Taster verlangt), "
	       "Freigaben %lu\n",
	       handler_calls, handler_denied, touch_grants);

	if (rounds == 0u) {
		printf("FEHLER: kein Lauf\n");
		return 1;
	}
	if (c.frames_ok == 0u) {
		printf("FEHLER: kein einziger Rahmen wurde angenommen - der Lauf "
		       "hat nur die erste Pruefung getroffen\n");
		return 1;
	}
	if (c.frames_bad == 0u) {
		printf("FEHLER: kein einziger Rahmen wurde abgewiesen - dann hat "
		       "der Lauf die Abwehr nicht getroffen\n");
		return 1;
	}
	if (c.resets == 0u || c.aborts == 0u) {
		printf("FEHLER: Ruecksetzen oder Abbruch kam nicht vor\n");
		return 1;
	}
	if (reports_out == 0u) {
		printf("FEHLER: es kam nie eine Antwort heraus\n");
		return 1;
	}
	/* Die eigentliche Zusicherung: der Bearbeiter sieht genau die
	 * angenommenen Rahmen, dazu die vom Lauf erteilten Freigaben. Kein
	 * abgewiesener Rahmen darf durchgekommen sein. */
	if (handler_calls > (unsigned long)c.frames_ok + touch_grants) {
		printf("FEHLER: der Bearbeiter wurde %lu mal gerufen, aber nur %u "
		       "Rahmen angenommen (+%lu Freigaben) - ein abgewiesener "
		       "Rahmen ist durchgekommen\n",
		       handler_calls, c.frames_ok, touch_grants);
		return 1;
	}
	if (handler_calls < (unsigned long)c.frames_ok) {
		printf("FEHLER: %u Rahmen angenommen, aber nur %lu Aufrufe - ein "
		       "angenommener Rahmen wurde verschluckt\n",
		       c.frames_ok, handler_calls);
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
