/* Pruefungen des Rahmens: aus 8-Byte-Berichten einen 70-Byte-Rahmen.
 *
 * Was hier bewiesen wird: ein Bericht, dessen Nummer nicht zum Rahmen
 * passt, wird abgewiesen - und zwar BEVOR die sieben Bytes irgendwohin
 * kopiert werden. Und: Spruenge in den Nummern sind erlaubt, weil der
 * Host Nullstuecke ueberspringt; die Luecken muessen Nullen bleiben. */
#include "yk_test.h"
#include "yk_frame.h"
#include "yk_crc.h"
#include "yk_hid.h"

/* Schickt einen Rahmen so hinein, wie ein Host ihn schickt - samt
 * Sparregel. Liefert das Ergebnis des letzten Berichts. */
static yk_rx_t feed_frame(yk_frame_rx_t *rx, const uint8_t frame[YK_FRAME_LEN], unsigned *sent)
{
	yk_rx_t r = YK_RX_IGNORE;
	uint8_t rpt[YK_RPT_LEN];

	if (sent)
		*sent = 0;
	for (unsigned seq = 0; seq < YK_FRAME_CHUNKS; seq++) {
		if (!yk_frame_chunk_needed(frame, seq))
			continue;
		yk_frame_chunk(frame, seq, rpt);
		r = yk_frame_rx_report(rx, rpt);
		if (sent)
			(*sent)++;
	}
	return r;
}

static void round_trip(void)
{
	yk_frame_rx_t rx;
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];
	unsigned sent = 0;

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)(0xa0u + i);

	yk_frame_rx_reset(&rx);
	T_TRUE("Rahmen gebaut",
	       yk_frame_build(YK_SLOT_CHAL_HMAC1, payload, sizeof(payload), frame));
	T_EQ("voller Rahmen kommt an", feed_frame(&rx, frame, &sent), YK_RX_FRAME);
	T_EQ("Steckplatz gelesen", yk_frame_slot(&rx), YK_SLOT_CHAL_HMAC1);
	T_HEX("Nutzlast unveraendert", yk_frame_payload(&rx), sizeof(payload),
	      t_hex(payload, sizeof(payload)));
	T_EQ("ohne Nullstuecke sind es 10 Berichte", sent, 10);
}

static void skipped_chunks(void)
{
	yk_frame_rx_t rx;
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];
	unsigned sent = 0;

	/* Nutzlast fast ganz null: nur die ersten drei Bytes stehen. Der
	 * Host schickt dann nur Stueck 0 und Stueck 9 - die Nummern
	 * springen von 0 auf 9. */
	memset(payload, 0, sizeof(payload));
	payload[0] = 0x11u;
	payload[1] = 0x22u;
	payload[2] = 0x33u;

	yk_frame_rx_reset(&rx);
	T_TRUE("Rahmen gebaut", yk_frame_build(YK_SLOT_DEVICE_SERIAL, payload, 3u, frame));
	T_EQ("Rahmen mit Luecken kommt an", feed_frame(&rx, frame, &sent), YK_RX_FRAME);
	T_EQ("nur zwei Berichte wurden gebraucht", sent, 2);
	T_EQ("Steckplatz gelesen", yk_frame_slot(&rx), YK_SLOT_DEVICE_SERIAL);
	T_HEX("die Luecken sind Nullen", yk_frame_payload(&rx), 8u, "1122330000000000");

	/* Ein zweiter Rahmen auf demselben Zustand darf keine Reste des
	 * ersten tragen. Das ist die Stelle, an der ein Parser ohne
	 * Nullsetzen beim Anfang alte Bytes durchreicht. */
	memset(payload, 0, sizeof(payload));
	payload[0] = 0x44u;
	T_TRUE("zweiter Rahmen gebaut", yk_frame_build(YK_SLOT_SWAP, payload, 1u, frame));
	T_EQ("zweiter Rahmen kommt an", feed_frame(&rx, frame, NULL), YK_RX_FRAME);
	T_HEX("kein Rest des ersten Rahmens", yk_frame_payload(&rx), 8u, "4400000000000000");
}

static void bad_sequences(void)
{
	yk_frame_rx_t rx;
	uint8_t frame[YK_FRAME_LEN];
	uint8_t rpt[YK_RPT_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)(i + 1u);
	yk_frame_build(YK_SLOT_CONFIG, payload, sizeof(payload), frame);

	/* Fortsetzung ohne Anfang. */
	yk_frame_rx_reset(&rx);
	yk_frame_chunk(frame, 3u, rpt);
	T_EQ("Stueck 3 ohne Anfang wird abgewiesen", yk_frame_rx_report(&rx, rpt), YK_RX_BAD);

	/* Letztes Stueck ohne Anfang - darf nicht als fertiger Rahmen
	 * durchgehen, auch wenn die Pruefsumme darin steht. */
	yk_frame_rx_reset(&rx);
	yk_frame_chunk(frame, 9u, rpt);
	T_EQ("Stueck 9 ohne Anfang wird abgewiesen", yk_frame_rx_report(&rx, rpt), YK_RX_BAD);

	/* Stillstand: dieselbe Nummer zweimal. Sonst liesse sich ein
	 * schon gelesenes Stueck nachtraeglich ueberschreiben. */
	yk_frame_rx_reset(&rx);
	yk_frame_chunk(frame, 0u, rpt);
	T_EQ("Anfang geht", yk_frame_rx_report(&rx, rpt), YK_RX_MORE);
	yk_frame_chunk(frame, 1u, rpt);
	T_EQ("Stueck 1 geht", yk_frame_rx_report(&rx, rpt), YK_RX_MORE);
	yk_frame_chunk(frame, 1u, rpt);
	T_EQ("Stueck 1 zweimal wird abgewiesen", yk_frame_rx_report(&rx, rpt), YK_RX_BAD);

	/* Rueckwaerts. */
	yk_frame_rx_reset(&rx);
	yk_frame_chunk(frame, 0u, rpt);
	yk_frame_rx_report(&rx, rpt);
	yk_frame_chunk(frame, 5u, rpt);
	T_EQ("Sprung vorwaerts geht", yk_frame_rx_report(&rx, rpt), YK_RX_MORE);
	yk_frame_chunk(frame, 2u, rpt);
	T_EQ("Rueckwaerts wird abgewiesen", yk_frame_rx_report(&rx, rpt), YK_RX_BAD);

	/* Nummer ausserhalb des Rahmens. Die Maske laesst 0..31 durch,
	 * der Rahmen hat aber nur 10 Stuecke - die 22 dazwischen sind der
	 * Bereich, in dem ein Parser ohne obere Grenze hinter den Puffer
	 * schreibt.
	 *
	 * ACHTUNG, eine Ueberschneidung, die dieser Prueflauf gefunden hat:
	 * die Nummer 15 ist in diesem Bereich KEIN Verstoss. 0x80 | 15 ist
	 * 0x8f, und das ist der Leerbericht - ein gueltiger Abbruch. Zwei
	 * Bedeutungen auf einem Bytewert. Wer beim Lesen des Parsers nur
	 * "Nummer ueber 9 ist falsch" denkt, haelt den Leerbericht fuer
	 * einen Angriff und bricht ykman ab; wer nur den Leerbericht sieht,
	 * laesst die Nummer 15 zum Schreiben durch. Darum wird hier BEIDES
	 * geprueft und nicht eines von beiden. */
	yk_frame_rx_reset(&rx);
	yk_frame_chunk(frame, 0u, rpt);
	yk_frame_rx_report(&rx, rpt);
	for (unsigned seq = YK_FRAME_CHUNKS; seq <= YK_SEQ_MASK; seq++) {
		const uint8_t status = (uint8_t)(YK_FLAG_WRITE | seq);
		const yk_rx_t want = (status == YK_DUMMY_REPORT) ? YK_RX_ABORT : YK_RX_BAD;
		yk_rx_t got;

		memset(rpt, 0xcc, YK_RPT_DATA);
		rpt[YK_RPT_STATUS] = status;
		got = yk_frame_rx_report(&rx, rpt);
		if (got != want) {
			char d[96];

			snprintf(d, sizeof(d), "Nummer %u (Statusbyte 0x%02x) gab %d, "
					       "erwartet %d",
				 seq, (unsigned)status, (int)got, (int)want);
			t_fail("Nummer ausserhalb des Rahmens", __FILE__, __LINE__, d);
		}
		t_checks++;
		/* Nach dem Abweisen ist der Rahmen weg - neu anfangen. */
		yk_frame_chunk(frame, 0u, rpt);
		yk_frame_rx_report(&rx, rpt);
	}
}

static void bad_crc(void)
{
	yk_frame_rx_t rx;
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)(i + 7u);

	/* Ein Bit in der Nutzlast kippen, nachdem die Pruefsumme steht. */
	yk_frame_build(YK_SLOT_CONFIG2, payload, sizeof(payload), frame);
	frame[17] ^= 0x08u;
	yk_frame_rx_reset(&rx);
	T_EQ("falsche Pruefsumme wird abgewiesen", feed_frame(&rx, frame, NULL), YK_RX_BAD);

	/* Ein Bit in der Pruefsumme selbst. */
	yk_frame_build(YK_SLOT_CONFIG2, payload, sizeof(payload), frame);
	frame[YK_FRAME_OFF_CRC] ^= 0x01u;
	yk_frame_rx_reset(&rx);
	T_EQ("gekippte Pruefsumme wird abgewiesen", feed_frame(&rx, frame, NULL), YK_RX_BAD);

	/* Das Steckplatzbyte gehoert NICHT zur Pruefsumme. Wird es
	 * verdreht, bleibt die Pruefsumme richtig - das ist so und wird
	 * hier festgehalten, damit niemand es fuer einen Fehler haelt und
	 * die Pruefsumme nachtraeglich ueber den Steckplatz zieht. */
	yk_frame_build(YK_SLOT_CONFIG2, payload, sizeof(payload), frame);
	frame[YK_FRAME_OFF_SLOT] = YK_SLOT_SWAP;
	yk_frame_rx_reset(&rx);
	T_EQ("verdrehter Steckplatz kommt durch - die Pruefsumme deckt ihn nicht",
	     feed_frame(&rx, frame, NULL), YK_RX_FRAME);
	T_EQ("und zwar mit dem verdrehten Wert", yk_frame_slot(&rx), YK_SLOT_SWAP);

	/* Die Fuellbytes gehoeren auch nicht dazu. */
	yk_frame_build(YK_SLOT_CONFIG2, payload, sizeof(payload), frame);
	frame[YK_FRAME_OFF_FILLER] = 0xffu;
	yk_frame_rx_reset(&rx);
	T_EQ("Fuellbyte stoert nicht", feed_frame(&rx, frame, NULL), YK_RX_FRAME);
}

static void special_reports(void)
{
	yk_frame_rx_t rx;
	uint8_t rpt[YK_RPT_LEN];
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[4] = {1, 2, 3, 4};

	yk_frame_build(YK_SLOT_CONFIG, payload, sizeof(payload), frame);

	/* Ruecksetzbericht: acht Byte 0xff. Sein Statusbyte ist 0xff und
	 * traegt damit auch den Schreibschalter - wer ihn nicht zuerst
	 * erkennt, liest ihn als Schreibpaket mit der Nummer 31. */
	yk_frame_rx_reset(&rx);
	memset(rpt, 0xff, sizeof(rpt));
	T_EQ("Ruecksetzbericht wird erkannt", yk_frame_rx_report(&rx, rpt), YK_RX_RESET);

	/* Und er muss einen angefangenen Rahmen wegwerfen. */
	yk_frame_chunk(frame, 0u, rpt);
	yk_frame_rx_report(&rx, rpt);
	memset(rpt, 0xff, sizeof(rpt));
	yk_frame_rx_report(&rx, rpt);
	yk_frame_chunk(frame, 1u, rpt);
	T_EQ("nach dem Ruecksetzen gibt es keine Fortsetzung",
	     yk_frame_rx_report(&rx, rpt), YK_RX_BAD);

	/* Leerbericht 0x8f: gueltiger Abbruch, kein Fehler. Seine Nummer
	 * ist 15 und laege ausserhalb des Rahmens. */
	yk_frame_rx_reset(&rx);
	memset(rpt, 0, sizeof(rpt));
	rpt[YK_RPT_STATUS] = (uint8_t)YK_DUMMY_REPORT;
	T_EQ("Leerbericht ist ein Abbruch, kein Fehler",
	     yk_frame_rx_report(&rx, rpt), YK_RX_ABORT);

	/* Bericht ohne Schreibschalter: nicht fuer uns. */
	yk_frame_rx_reset(&rx);
	memset(rpt, 0x5a, sizeof(rpt));
	rpt[YK_RPT_STATUS] = 0x40u;
	T_EQ("Bericht ohne Schreibschalter wird uebergangen",
	     yk_frame_rx_report(&rx, rpt), YK_RX_IGNORE);
	rpt[YK_RPT_STATUS] = 0x00u;
	T_EQ("Statusbyte null wird uebergangen", yk_frame_rx_report(&rx, rpt), YK_RX_IGNORE);

	/* NULL darf nicht in den Zeiger laufen. */
	T_EQ("NULL-Bericht", yk_frame_rx_report(&rx, NULL), YK_RX_BAD);
	T_EQ("NULL-Zustand", yk_frame_rx_report(NULL, rpt), YK_RX_BAD);
}

static void tx_split(void)
{
	yk_frame_tx_t tx;
	uint8_t rpt[YK_RPT_LEN];
	uint8_t data[20];
	uint8_t got[YK_RESP_MAX];
	size_t n = 0;
	unsigned reports = 0;

	/* 20 Byte ist die Laenge einer HMAC-SHA1-Antwort. */
	for (size_t i = 0; i < sizeof(data); i++)
		data[i] = (uint8_t)(0x10u + i);

	yk_frame_tx_reset(&tx);
	T_TRUE("Antwort eingestellt", yk_frame_tx_start(&tx, data, sizeof(data)));

	while (yk_frame_tx_next(&tx, rpt)) {
		const uint8_t st = rpt[YK_RPT_STATUS];

		T_TRUE("jeder Antwortbericht hat 0x40", (st & YK_FLAG_RESP_PENDING) != 0u);
		if (reports > 0u && (st & YK_SEQ_MASK) == 0u) {
			/* Das ist der Abschluss. */
			reports++;
			break;
		}
		T_EQ("Nummer laeuft mit", (unsigned)(st & YK_SEQ_MASK), reports);
		if (n + YK_RPT_DATA <= sizeof(got)) {
			memcpy(got + n, rpt, YK_RPT_DATA);
			n += YK_RPT_DATA;
		}
		reports++;
	}

	/* 20 Byte gehen in drei Stuecke (7+7+6), dazu der Abschluss. */
	T_EQ("drei Stuecke und ein Abschluss", reports, 4);
	T_HEX("die 20 Byte kommen heil heraus", got, sizeof(data), t_hex(data, sizeof(data)));
	T_TRUE("danach kommt nichts mehr", !yk_frame_tx_next(&tx, rpt));

	/* Eine zu lange Antwort wird abgewiesen, ohne den Zustand
	 * anzufassen. */
	yk_frame_tx_reset(&tx);
	T_TRUE("zu lange Antwort wird abgewiesen",
	       !yk_frame_tx_start(&tx, data, (size_t)YK_RESP_MAX + 1u));
	T_TRUE("und nichts eingestellt", !yk_frame_tx_next(&tx, rpt));

	/* Genau die Obergrenze geht noch. */
	{
		static uint8_t big[YK_RESP_MAX];
		unsigned count = 0;

		memset(big, 0x5a, sizeof(big));
		yk_frame_tx_reset(&tx);
		T_TRUE("die Obergrenze geht", yk_frame_tx_start(&tx, big, sizeof(big)));
		while (yk_frame_tx_next(&tx, rpt))
			count++;
		/* 31 Stuecke plus Abschluss, und keine Nummer ueber 0x1f. */
		T_EQ("31 Stuecke und ein Abschluss", count, YK_RESP_CHUNKS_MAX + 1);
	}

	/* Eine leere Antwort ist nur der Abschluss. */
	yk_frame_tx_reset(&tx);
	T_TRUE("leere Antwort geht", yk_frame_tx_start(&tx, NULL, 0u));
	T_TRUE("ein Bericht kommt", yk_frame_tx_next(&tx, rpt));
	T_EQ("und das ist der Abschluss", rpt[YK_RPT_STATUS], (uint8_t)YK_FLAG_RESP_PENDING);
	T_TRUE("danach nichts", !yk_frame_tx_next(&tx, rpt));
}

static void build_limits(void)
{
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN + 1];

	memset(payload, 0x7eu, sizeof(payload));
	T_TRUE("65 Byte Nutzlast wird abgewiesen",
	       !yk_frame_build(YK_SLOT_CONFIG, payload, YK_SLOT_DATA_LEN + 1u, frame));
	T_TRUE("64 Byte gehen",
	       yk_frame_build(YK_SLOT_CONFIG, payload, YK_SLOT_DATA_LEN, frame));
	T_TRUE("NULL mit Laenge wird abgewiesen",
	       !yk_frame_build(YK_SLOT_CONFIG, NULL, 4u, frame));
	T_TRUE("NULL-Ziel wird abgewiesen",
	       !yk_frame_build(YK_SLOT_CONFIG, payload, 4u, NULL));

	/* Die Zahlen muessen zueinander passen: 10 Stuecke mal 7 Byte
	 * sind genau der Rahmen. Geht einer der Werte verloren, faellt es
	 * hier auf und nicht erst am Gerat. */
	T_EQ("10 mal 7 ist der Rahmen", YK_FRAME_CHUNKS * YK_RPT_DATA, YK_FRAME_LEN);
	T_EQ("Nutzlast plus sechs ist der Rahmen", YK_SLOT_DATA_LEN + 6, YK_FRAME_LEN);
	T_EQ("die Fuellbytes enden am Rahmenende", YK_FRAME_OFF_FILLER + 3, YK_FRAME_LEN);
}

void suite_ykframe(void)
{
	printf("== Rahmen\n");
	round_trip();
	skipped_chunks();
	bad_sequences();
	bad_crc();
	special_reports();
	tx_split();
	build_limits();
}
