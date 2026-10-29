/* Siehe yk_frame.h. */
#include <string.h>

#include "yk_frame.h"
#include "yk_crc.h"
#include "osk_mem.h"

/* Alle acht Bytes 0xff: der Host setzt den Antwortzustand zurueck. */
static bool is_reset_report(const uint8_t *rpt)
{
	for (unsigned i = 0; i < YK_RPT_LEN; i++) {
		if (rpt[i] != 0xffu)
			return false;
	}
	return true;
}

void yk_frame_rx_reset(yk_frame_rx_t *rx)
{
	if (rx == NULL)
		return;
	osk_wipe(rx->buf, sizeof(rx->buf));
	rx->last_seq = 0u;
	rx->active = false;
}

yk_rx_t yk_frame_rx_report(yk_frame_rx_t *rx, const uint8_t rpt[YK_RPT_LEN])
{
	if (rx == NULL || rpt == NULL)
		return YK_RX_BAD;

	/* Der Ruecksetzbericht zuerst: er traegt zufaellig 0x80 im
	 * Statusbyte und wuerde sonst als Schreibpaket mit Nummer 31
	 * gelesen. */
	if (is_reset_report(rpt)) {
		yk_frame_rx_reset(rx);
		return YK_RX_RESET;
	}

	const uint8_t status = rpt[YK_RPT_STATUS];

	/* Der Leerbericht ist ein gueltiger Abbruch, kein Fehler - und er
	 * muss VOR der Nummernpruefung stehen, weil 0x8f die Nummer 15
	 * traegt und die Pruefung ihn sonst als Verstoss meldet. */
	if (status == YK_DUMMY_REPORT) {
		yk_frame_rx_reset(rx);
		return YK_RX_ABORT;
	}

	/* Ohne Schreibschalter ist es kein Paket fuer uns. */
	if ((status & YK_FLAG_WRITE) == 0u)
		return YK_RX_IGNORE;

	const uint8_t seq = (uint8_t)(status & YK_SEQ_MASK);

	/* Nummer ausserhalb des Rahmens. */
	if (seq >= YK_FRAME_CHUNKS) {
		yk_frame_rx_reset(rx);
		return YK_RX_BAD;
	}

	if (seq == 0u) {
		/* Jede Null faengt einen neuen Rahmen an, auch mitten in
		 * einem alten. Der Puffer wird dabei vollstaendig genullt -
		 * darauf beruht, dass uebersprungene Stuecke Nullen sind. */
		yk_frame_rx_reset(rx);
		rx->active = true;
	} else {
		/* Ohne angefangenen Rahmen gibt es keine Fortsetzung. */
		if (!rx->active) {
			yk_frame_rx_reset(rx);
			return YK_RX_BAD;
		}
		/* Die Nummer muss echt steigen. Spruenge sind erlaubt (das
		 * sind die uebersprungenen Nullstuecke), Stillstand und
		 * Rueckwaertsgang nicht - sonst liesse sich ein bereits
		 * gelesenes Stueck nachtraeglich ueberschreiben. */
		if (seq <= rx->last_seq) {
			yk_frame_rx_reset(rx);
			return YK_RX_BAD;
		}
	}

	/* Grenze vor dem Kopieren nachrechnen, nicht danach. seq ist hier
	 * bewiesen kleiner als YK_FRAME_CHUNKS, also endet das Stueck bei
	 * hoechstens 10*7 = 70 = YK_FRAME_LEN. Die Pruefung bleibt
	 * trotzdem stehen: sie kostet nichts und faellt auf, wenn jemand
	 * eine der Zahlen aendert. */
	const size_t off = (size_t)seq * YK_RPT_DATA;

	if (off + YK_RPT_DATA > YK_FRAME_LEN) {
		yk_frame_rx_reset(rx);
		return YK_RX_BAD;
	}

	memcpy(rx->buf + off, rpt, YK_RPT_DATA);
	rx->last_seq = seq;

	if (seq != YK_FRAME_CHUNKS - 1u)
		return YK_RX_MORE;

	/* Letztes Stueck: Pruefsumme der Nutzlast gegen die uebertragene
	 * halten. Gerechnet wird ueber die 64 Nutzlastbytes - das
	 * Steckplatzbyte liegt dazwischen und gehoert nicht dazu. */
	const uint16_t want = (uint16_t)((uint16_t)rx->buf[YK_FRAME_OFF_CRC] |
					 (uint16_t)((uint16_t)rx->buf[YK_FRAME_OFF_CRC + 1] << 8));
	const uint16_t got = yk_crc16_wire(rx->buf, YK_SLOT_DATA_LEN);

	rx->active = false;
	if (got != want) {
		yk_frame_rx_reset(rx);
		return YK_RX_BAD;
	}
	return YK_RX_FRAME;
}

uint8_t yk_frame_slot(const yk_frame_rx_t *rx)
{
	if (rx == NULL)
		return 0u;
	return rx->buf[YK_FRAME_OFF_SLOT];
}

const uint8_t *yk_frame_payload(const yk_frame_rx_t *rx)
{
	if (rx == NULL)
		return NULL;
	return rx->buf;
}

void yk_frame_tx_reset(yk_frame_tx_t *tx)
{
	if (tx == NULL)
		return;
	osk_wipe(tx->buf, sizeof(tx->buf));
	tx->len = 0u;
	tx->seq = 0u;
	tx->active = false;
	tx->closing = false;
}

bool yk_frame_tx_start(yk_frame_tx_t *tx, const uint8_t *data, size_t len)
{
	if (tx == NULL)
		return false;
	/* Grenze zuerst, dann kopieren. Bei einer zu langen Antwort bleibt
	 * der alte Zustand unberuehrt statt halb ueberschrieben. */
	if (len > YK_RESP_MAX)
		return false;
	if (len > 0u && data == NULL)
		return false;

	yk_frame_tx_reset(tx);
	if (len > 0u)
		memcpy(tx->buf, data, len);
	tx->len = len;
	tx->active = true;
	return true;
}

bool yk_frame_tx_next(yk_frame_tx_t *tx, uint8_t rpt[YK_RPT_LEN])
{
	if (tx == NULL || rpt == NULL || !tx->active)
		return false;

	memset(rpt, 0, YK_RPT_LEN);

	const size_t off = (size_t)tx->seq * YK_RPT_DATA;

	/* Alle Daten heraus? Dann kommt der Abschlussbericht: Nummer 0 bei
	 * schon gesendeten Stuecken heisst fuer den Host "fertig". */
	if (tx->closing || off >= tx->len) {
		rpt[YK_RPT_STATUS] = (uint8_t)YK_FLAG_RESP_PENDING;
		tx->active = false;
		tx->closing = false;
		return true;
	}

	size_t n = tx->len - off;

	if (n > YK_RPT_DATA)
		n = YK_RPT_DATA;
	memcpy(rpt, tx->buf + off, n);
	rpt[YK_RPT_STATUS] = (uint8_t)(YK_FLAG_RESP_PENDING | (tx->seq & YK_SEQ_MASK));

	tx->seq = (uint8_t)(tx->seq + 1u);
	if ((size_t)tx->seq * YK_RPT_DATA >= tx->len)
		tx->closing = true;
	return true;
}

bool yk_frame_build(uint8_t slot, const uint8_t *payload, size_t len,
		    uint8_t out[YK_FRAME_LEN])
{
	if (out == NULL || len > YK_SLOT_DATA_LEN)
		return false;
	if (len > 0u && payload == NULL)
		return false;

	memset(out, 0, YK_FRAME_LEN);
	if (len > 0u)
		memcpy(out, payload, len);
	out[YK_FRAME_OFF_SLOT] = slot;

	const uint16_t crc = yk_crc16_wire(out, YK_SLOT_DATA_LEN);

	out[YK_FRAME_OFF_CRC] = (uint8_t)(crc & 0xffu);
	out[YK_FRAME_OFF_CRC + 1] = (uint8_t)((crc >> 8) & 0xffu);
	return true;
}

bool yk_frame_chunk_needed(const uint8_t frame[YK_FRAME_LEN], unsigned seq)
{
	if (frame == NULL || seq >= YK_FRAME_CHUNKS)
		return false;
	if (seq == 0u || seq == YK_FRAME_CHUNKS - 1u)
		return true;

	const size_t off = (size_t)seq * YK_RPT_DATA;

	for (size_t i = 0; i < YK_RPT_DATA; i++) {
		if (frame[off + i] != 0u)
			return true;
	}
	return false;
}

bool yk_frame_chunk(const uint8_t frame[YK_FRAME_LEN], unsigned seq,
		    uint8_t rpt[YK_RPT_LEN])
{
	if (frame == NULL || rpt == NULL || seq >= YK_FRAME_CHUNKS)
		return false;

	memcpy(rpt, frame + (size_t)seq * YK_RPT_DATA, YK_RPT_DATA);
	rpt[YK_RPT_STATUS] = (uint8_t)(YK_FLAG_WRITE | (seq & YK_SEQ_MASK));
	return true;
}
