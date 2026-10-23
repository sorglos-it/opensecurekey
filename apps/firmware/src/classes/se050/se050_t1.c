#include <string.h>
#include "se050_t1.h"
#include "se050_crc.h"

/* Groesster Rahmen, den dieser Treiber bauen oder annehmen muss: unsere
 * angekuendigte Empfangsgroesse plus Prolog und Epilog. NICHT
 * SE050_T1_MAX_INF - 4089 Byte je Puffer waeren hier masslos, und mehr als
 * SE050_IFSD kuendigen wir nie an. */
#define FRAME_BUF (SE050_T1_OVERHEAD + SE050_IFSD)

/* ------------------------------------------------------------ Rahmenbau */

size_t osk_se050_t1_build(uint8_t *buf, size_t cap, uint8_t nad, uint8_t pcb, const uint8_t *inf,
			  uint16_t inf_len)
{
	if (buf == NULL) {
		return 0;
	}
	if (inf_len > 0 && inf == NULL) {
		return 0;
	}
	if (inf_len > SE050_T1_MAX_INF) {
		return 0;
	}
	const size_t total = SE050_T1_OVERHEAD + (size_t)inf_len;
	if (cap < total) {
		return 0;
	}

	/* Prolog: NAD, PCB, dann LEN als zwei Byte, hoeherwertiges zuerst
	 * (GPC_SPE_172 Abschnitt 4.2). */
	buf[0] = nad;
	buf[1] = pcb;
	buf[2] = (uint8_t)(inf_len >> 8);
	buf[3] = (uint8_t)(inf_len & 0xFFu);
	if (inf_len > 0) {
		memcpy(&buf[SE050_T1_PROLOGUE_LEN], inf, inf_len);
	}

	const uint16_t crc = osk_se050_crc16(buf, SE050_T1_PROLOGUE_LEN + inf_len);
	uint8_t *epi = &buf[SE050_T1_PROLOGUE_LEN + inf_len];
#if SE050_CRC_LSB_FIRST
	epi[0] = (uint8_t)(crc & 0xFFu);
	epi[1] = (uint8_t)(crc >> 8);
#else
	epi[0] = (uint8_t)(crc >> 8);
	epi[1] = (uint8_t)(crc & 0xFFu);
#endif
	return total;
}

/* Die Laengenangabe aus einem Prolog lesen. */
static size_t len_of(const uint8_t *frame)
{
	return ((size_t)frame[2] << 8) | frame[3];
}

osk_result_t osk_se050_t1_check(const uint8_t *frame, size_t len)
{
	if (frame == NULL || len < SE050_T1_OVERHEAD) {
		return OSK_ERR_FORMAT;
	}
	/* Das LEN-Feld muss zur wirklich gelesenen Laenge passen. Erst danach
	 * darf die Pruefsumme gerechnet werden, sonst liest man ueber das
	 * Ende des Puffers hinaus. */
	const size_t inf_len = len_of(frame);
	if (len != SE050_T1_OVERHEAD + inf_len) {
		return OSK_ERR_FORMAT;
	}

	const uint16_t want = osk_se050_crc16(frame, SE050_T1_PROLOGUE_LEN + inf_len);
	const uint8_t *epi = &frame[SE050_T1_PROLOGUE_LEN + inf_len];
#if SE050_CRC_LSB_FIRST
	const uint16_t got = (uint16_t)(epi[0] | ((uint16_t)epi[1] << 8));
#else
	const uint16_t got = (uint16_t)(((uint16_t)epi[0] << 8) | epi[1]);
#endif
	if (got != want) {
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

/* ------------------------------------------------------------ Bus-Hilfen */

/* Einen fertigen Rahmen hinausschreiben. Ein Rahmen ist nie leer, damit der
 * Baustein nicht nach SE050-Erratasheet I2C.2 die Taktleitung dauerhaft
 * haelt. */
static osk_result_t frame_write(osk_se050_t1_t *t1, const uint8_t *frame, size_t len)
{
	if (len == 0) {
		return OSK_ERR_INTERNAL;
	}
	const int rc = t1->bus->write(t1->bus, frame, len);
	if (rc == 0) {
		return OSK_OK;
	}
	return (rc == OSK_BUS_ARG) ? OSK_ERR_ARG : OSK_ERR_IO;
}

/* Genau want Bytes lesen, solange das Zeitbudget reicht.
 * Halb gelesene Bytes bleiben in dst stehen; der Aufrufer verwirft sie. */
static osk_result_t read_exact(osk_se050_t1_t *t1, uint8_t *dst, size_t want, uint32_t start_ms,
			       uint32_t budget_ms)
{
	size_t have = 0;

	while (have < want) {
		const int rc = t1->bus->read(t1->bus, &dst[have], want - have);
		if (rc > 0) {
			have += (size_t)rc;
			continue;
		}
		if (rc != OSK_BUS_AGAIN && rc != 0) {
			return OSK_ERR_IO;
		}
		/* Nichts da. Zeit pruefen, kurz warten, erneut fragen.
		 * Die Differenz in uint32_t gerechnet, damit ein Ueberlauf
		 * der Uhr nichts kaputt macht. */
		const uint32_t waited = t1->bus->now_ms(t1->bus) - start_ms;
		if (waited >= budget_ms) {
			return OSK_ERR_TIMEOUT;
		}
		t1->bus->wait_ms(t1->bus, SE050_T_POLL_MS);
	}
	return OSK_OK;
}

/* Einen Rahmen lesen: erst der Prolog, dann INF und Epilog nach dessen
 * Laengenangabe. So wird nie mehr gelesen, als der Baustein schickt.
 *
 * Kuendigt der Prolog mehr an, als in frame passt, wird der Rest trotzdem
 * vom Bus geholt und verworfen: ein Rahmen, der im Baustein halb abgeholt
 * liegen bleibt, laesst ihn haengen (SE050-Erratasheet I2C.1). Eine Laenge
 * ueber unserem angekuendigten IFSD ist nach GPC_SPE_172 Abschnitt 4.2.3
 * ohnehin ungueltig. */
static osk_result_t frame_read(osk_se050_t1_t *t1, uint8_t *frame, size_t cap, size_t *len_out,
			       uint32_t budget_ms)
{
	if (cap < SE050_T1_OVERHEAD) {
		return OSK_ERR_INTERNAL;
	}
	const uint32_t start = t1->bus->now_ms(t1->bus);

	osk_result_t r = read_exact(t1, frame, SE050_T1_PROLOGUE_LEN, start, budget_ms);
	if (r != OSK_OK) {
		return r;
	}

	const size_t inf_len = len_of(frame);
	const size_t rest = inf_len + SE050_T1_EPILOGUE_LEN;

	if (SE050_T1_PROLOGUE_LEN + rest <= cap) {
		r = read_exact(t1, &frame[SE050_T1_PROLOGUE_LEN], rest, start, budget_ms);
		if (r != OSK_OK) {
			return r;
		}
		*len_out = SE050_T1_PROLOGUE_LEN + rest;
		return osk_se050_t1_check(frame, *len_out);
	}

	/* Passt nicht in den Puffer: haeppchenweise leer lesen und wegwerfen. */
	uint8_t sink[32];
	size_t left = rest;
	while (left > 0) {
		const size_t chunk = (left < sizeof(sink)) ? left : sizeof(sink);
		r = read_exact(t1, sink, chunk, start, budget_ms);
		if (r != OSK_OK) {
			return r;
		}
		left -= chunk;
	}
	return OSK_ERR_FORMAT;
}

/* --------------------------------------------------------------- S-Block */

/* Einen S-Block senden und auf die zugehoerige Antwort warten.
 * rsp_inf darf NULL sein, wenn die Antwortdaten nicht gebraucht werden. */
static osk_result_t s_block(osk_se050_t1_t *t1, uint8_t code, const uint8_t *inf, uint16_t inf_len,
			    uint8_t *rsp_inf, uint16_t *rsp_inf_len)
{
	uint8_t frame[FRAME_BUF];
	const uint8_t pcb = (uint8_t)(SE050_PCB_TYPE_S | (code & SE050_PCB_S_CODE_MASK));

	const size_t n =
	    osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE, pcb, inf, inf_len);
	if (n == 0) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = frame_write(t1, frame, n);
	if (r != OSK_OK) {
		return r;
	}

	size_t got = 0;
	r = frame_read(t1, frame, sizeof(frame), &got, t1->bwt_ms);
	if (r != OSK_OK) {
		return r;
	}

	const uint8_t rpcb = frame[1];
	if (!SE050_PCB_IS_S(rpcb)) {
		return OSK_ERR_FORMAT;
	}
	if ((rpcb & SE050_PCB_S_CODE_MASK) != (code & SE050_PCB_S_CODE_MASK)) {
		return OSK_ERR_FORMAT;
	}
	if ((rpcb & SE050_PCB_S_RESPONSE) == 0) {
		return OSK_ERR_FORMAT;
	}

	if (rsp_inf != NULL && rsp_inf_len != NULL) {
		const size_t n_inf = len_of(frame);
		if (n_inf > *rsp_inf_len) {
			return OSK_ERR_FORMAT;
		}
		memcpy(rsp_inf, &frame[SE050_T1_PROLOGUE_LEN], n_inf);
		*rsp_inf_len = (uint16_t)n_inf;
	}
	return OSK_OK;
}

osk_result_t osk_se050_t1_resync(osk_se050_t1_t *t1)
{
	if (t1 == NULL || t1->bus == NULL) {
		return OSK_ERR_ARG;
	}
	const osk_result_t r = s_block(t1, SE050_S_RESYNC, NULL, 0, NULL, NULL);
	if (r == OSK_OK) {
		t1->seq_tx = 0;
		t1->seq_rx = 0;
		t1->needs_resync = false;
	}
	return r;
}

/* ----------------------------------------------------------- Auf und zu */

/* Die CIP auswerten (GPC_SPE_172 Abschnitt 4.3):
 *   PVER(1) | LenIIN(1) | IIN(var) | PLID(1) | LenPLP(1) | PLP(var)
 *   | LenDLLP(1) | DLLP(var) | LenHB(1) | HB(var)
 * Uns interessiert nur DLLP: BWT (2 Byte) und IFSC (2 Byte), Tabelle 4-7.
 *
 * Jede Laenge wird geprueft, bevor darauf zugegriffen wird - das ist der
 * erste Puffer, den der Baustein schickt, und er kommt noch ungesichert
 * ueber den Draht. */
static osk_result_t cip_parse(osk_se050_t1_t *t1, const uint8_t *cip, size_t len)
{
	size_t pos = 0;

	if (len < 1u) {
		return OSK_ERR_FORMAT;
	}
	pos += 1u; /* PVER */

	/* IIN */
	if (pos >= len) {
		return OSK_ERR_FORMAT;
	}
	const size_t iin_len = cip[pos++];
	if (iin_len > len - pos) {
		return OSK_ERR_FORMAT;
	}
	pos += iin_len;

	/* PLID und PLP */
	if (pos >= len) {
		return OSK_ERR_FORMAT;
	}
	pos += 1u; /* PLID */
	if (pos >= len) {
		return OSK_ERR_FORMAT;
	}
	const size_t plp_len = cip[pos++];
	if (plp_len > len - pos) {
		return OSK_ERR_FORMAT;
	}
	pos += plp_len;

	/* DLLP: hier stehen BWT und IFSC. */
	if (pos >= len) {
		return OSK_ERR_FORMAT;
	}
	const size_t dllp_len = cip[pos++];
	if (dllp_len > len - pos) {
		return OSK_ERR_FORMAT;
	}
	if (dllp_len < 4u) {
		/* Kuerzer als BWT + IFSC: dann bleibt es bei den Vorgaben. */
		return OSK_ERR_FORMAT;
	}

	const uint16_t bwt = (uint16_t)(((uint16_t)cip[pos] << 8) | cip[pos + 1u]);
	const uint16_t ifsc = (uint16_t)(((uint16_t)cip[pos + 2u] << 8) | cip[pos + 3u]);

	/* Unsinnige Werte nicht uebernehmen - lieber langsam und richtig als
	 * mit einem IFSC von 0 oder einer Wartezeit von 0 ms weiterarbeiten.
	 * Spaetere Fassungen duerfen hinten mehr Felder anhaengen; die
	 * werden nach Abschnitt 4.3.2 ausdruecklich ignoriert. */
	if (bwt > 0u) {
		t1->bwt_ms = bwt;
	}
	if (ifsc > 0u && ifsc <= SE050_T1_MAX_INF) {
		t1->ifsc = ifsc;
	}
	return OSK_OK;
}

osk_result_t osk_se050_t1_open(osk_se050_t1_t *t1, osk_se050_bus_t *bus)
{
	if (t1 == NULL || bus == NULL || bus->write == NULL || bus->read == NULL ||
	    bus->wait_ms == NULL || bus->now_ms == NULL) {
		return OSK_ERR_ARG;
	}

	memset(t1, 0, sizeof(*t1));
	t1->bus = bus;
	t1->ifsc = SE050_IFSC_DEFAULT;
	t1->bwt_ms = SE050_BWT_DEFAULT_MS;

	/* 1. Schnittstelle zuruecksetzen. Damit faellt auch die Entscheidung,
	 * welches der beiden T=1-Protokolle gilt: der Baustein erkennt es am
	 * Prolog dieses ersten Blocks (SE051-Datenblatt Abschnitt 2.1).
	 * Dass das wirklich greift, ist die allererste Messung am Geraet. */
	const osk_result_t r = s_block(t1, SE050_S_SWR, NULL, 0, NULL, NULL);
	if (r != OSK_OK) {
		return r;
	}

	/* 2. Schnittstellenwerte erfragen. Geht das schief, bleibt es bei
	 * IFSC 8 und BWT 300 ms - langsam, aber richtig. Darum wird der
	 * Fehler hier bewusst verschluckt. */
	uint8_t cip[64];
	uint16_t cip_len = (uint16_t)sizeof(cip);
	if (s_block(t1, SE050_S_CIP, NULL, 0, cip, &cip_len) == OSK_OK) {
		(void)cip_parse(t1, cip, cip_len);
	}

	/* 3. Dem Baustein sagen, wie viel wir annehmen koennen. Der Wert geht
	 * als zwei Byte hinaus; die Beschreibung erlaubt ein oder zwei
	 * (Abschnitt 4.1). Auch hier ist ein Fehlschlag nicht toedlich: dann
	 * bleibt der Baustein bei seiner Vorgabe von 64 Byte. */
	const uint8_t ifsd[2] = {(uint8_t)(SE050_IFSD >> 8), (uint8_t)(SE050_IFSD & 0xFFu)};
	(void)s_block(t1, SE050_S_IFS, ifsd, sizeof(ifsd), NULL, NULL);

	t1->open = true;
	return OSK_OK;
}

osk_result_t osk_se050_t1_close(osk_se050_t1_t *t1)
{
	if (t1 == NULL || t1->bus == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = OSK_OK;
	if (t1->open) {
		/* S(RELEASE): der Baustein darf in den Stromsparzustand.
		 * Beim SE050 heisst derselbe Block "End of APDU session". */
		r = s_block(t1, SE050_S_RELEASE, NULL, 0, NULL, NULL);
	}
	/* Geschlossen ist geschlossen, auch wenn der Baustein nicht quittiert. */
	memset(t1, 0, sizeof(*t1));
	return r;
}

/* ------------------------------------------------------- Hin und zurueck */

/* Einen R-Block als Quittung senden: "ich erwarte den Block mit dieser
 * Folgenummer". */
static osk_result_t send_r_block(osk_se050_t1_t *t1, uint8_t expect_seq, uint8_t err)
{
	uint8_t frame[SE050_T1_OVERHEAD];
	uint8_t pcb = (uint8_t)(SE050_PCB_TYPE_R | (err & SE050_PCB_R_ERR_MASK));
	if (expect_seq) {
		pcb |= SE050_PCB_R_SEQ;
	}
	const size_t n = osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE, pcb, NULL, 0);
	if (n == 0) {
		return OSK_ERR_INTERNAL;
	}
	return frame_write(t1, frame, n);
}

/* Die Bitte des Bausteins um mehr Zeit beantworten. Der Rahmen selbst wird
 * dabei NICHT erneut gesendet - nur das Zeitbudget waechst. */
static osk_result_t handle_wtx(osk_se050_t1_t *t1, const uint8_t *rsp, uint32_t *budget_ms)
{
	const uint8_t units = (len_of(rsp) >= 1u) ? rsp[SE050_T1_PROLOGUE_LEN] : 1u;

	uint8_t ack[SE050_T1_OVERHEAD + 1u];
	const uint8_t apcb = (uint8_t)(SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE | SE050_S_WTX);
	const size_t an = osk_se050_t1_build(ack, sizeof(ack), SE050_NAD_TO_SE, apcb, &units, 1u);
	if (an == 0) {
		return OSK_ERR_INTERNAL;
	}
	const osk_result_t r = frame_write(t1, ack, an);
	if (r != OSK_OK) {
		return r;
	}

	/* WTX zaehlt in Vielfachen der Blockwartezeit. */
	const uint32_t more_ms = (uint32_t)units * t1->bwt_ms;
	if (*budget_ms + more_ms > SE050_T_WTX_MAX_MS) {
		*budget_ms = SE050_T_WTX_MAX_MS;
	} else {
		*budget_ms += more_ms;
	}
	return OSK_OK;
}

/* Einen I-Block senden, bis der Baustein ihn annimmt.
 * Bei einer Ablehnung (R-Block mit Fehlergrund) oder einer kaputten
 * Pruefsumme wird derselbe Rahmen noch einmal geschickt, hoechstens
 * SE050_MAX_RETRY mal.
 *
 * Nach erfolgreicher Annahme steht in rsp der naechste gelesene Rahmen:
 * entweder die Quittung (R-Block) oder schon der erste Antwortblock. */
static osk_result_t send_i_block(osk_se050_t1_t *t1, const uint8_t *inf, uint16_t inf_len,
				 bool more, uint8_t *rsp, size_t rsp_cap, size_t *rsp_len)
{
	uint8_t frame[FRAME_BUF];
	uint8_t pcb = SE050_PCB_TYPE_I;
	if (t1->seq_tx) {
		pcb |= SE050_PCB_I_SEQ;
	}
	if (more) {
		pcb |= SE050_PCB_I_MORE;
	}

	const size_t n =
	    osk_se050_t1_build(frame, sizeof(frame), SE050_NAD_TO_SE, pcb, inf, inf_len);
	if (n == 0) {
		return OSK_ERR_ARG;
	}

	for (uint32_t attempt = 0; attempt <= SE050_MAX_RETRY; attempt++) {
		/* Zeitbudget je Versuch neu: der Baustein darf per WTX mehr
		 * verlangen, SE050_T_WTX_MAX_MS ist die harte Obergrenze. */
		uint32_t budget = t1->bwt_ms;

		osk_result_t r = frame_write(t1, frame, n);
		if (r != OSK_OK) {
			t1->needs_resync = true;
			return r;
		}

		bool retry = false;
		while (!retry) {
			r = frame_read(t1, rsp, rsp_cap, rsp_len, budget);
			if (r == OSK_ERR_FORMAT) {
				/* Pruefsumme kaputt oder Laenge unstimmig:
				 * wiederholen statt den Inhalt zu raten. */
				retry = true;
				break;
			}
			if (r != OSK_OK) {
				t1->needs_resync = true;
				return r;
			}

			const uint8_t rpcb = rsp[1];
			const bool is_s = SE050_PCB_IS_S(rpcb);
			const bool is_r = SE050_PCB_IS_R(rpcb);

			if (is_s && (rpcb & SE050_PCB_S_CODE_MASK) == SE050_S_WTX &&
			    (rpcb & SE050_PCB_S_RESPONSE) == 0) {
				r = handle_wtx(t1, rsp, &budget);
				if (r != OSK_OK) {
					t1->needs_resync = true;
					return r;
				}
				continue;
			}

			if (is_r && (rpcb & SE050_PCB_R_ERR_MASK) != SE050_PCB_R_ERR_NONE) {
				retry = true;
				break;
			}

			/* Alles andere ist eine gueltige Reaktion. */
			t1->seq_tx ^= 1u;
			return OSK_OK;
		}
	}

	t1->needs_resync = true;
	return OSK_ERR_IO;
}

osk_result_t osk_se050_t1_transceive(osk_se050_t1_t *t1, const uint8_t *cmd, size_t cmd_len,
				     uint8_t *rsp, size_t rsp_cap, size_t *rsp_len)
{
	if (t1 == NULL || t1->bus == NULL || cmd == NULL || cmd_len == 0 || rsp == NULL ||
	    rsp_len == NULL) {
		return OSK_ERR_ARG;
	}
	if (!t1->open) {
		return OSK_ERR_IO;
	}
	*rsp_len = 0;

	/* Nach einem Abbruch erst aufraeumen. Ein neuer Befehl auf eine nicht
	 * abgeholte Antwort laesst den Baustein haengen
	 * (SE050-Erratasheet I2C.1). */
	if (t1->needs_resync) {
		const osk_result_t r = osk_se050_t1_resync(t1);
		if (r != OSK_OK) {
			return r;
		}
	}

	uint8_t frame[FRAME_BUF];
	size_t frame_len = 0;

	/* ---- Hinweg: zerlegen, solange mehr als ifsc uebrig ist. */
	size_t sent = 0;
	for (;;) {
		const size_t left = cmd_len - sent;
		const bool more = (left > t1->ifsc);
		const uint16_t chunk = more ? t1->ifsc : (uint16_t)left;

		osk_result_t r =
		    send_i_block(t1, &cmd[sent], chunk, more, frame, sizeof(frame), &frame_len);
		if (r != OSK_OK) {
			return r;
		}
		sent += chunk;

		if (!more) {
			break;
		}

		/* Zwischenblock: der Baustein quittiert mit einem R-Block ohne
		 * Fehlergrund. Alles andere mitten in der Kette ist ein Bruch. */
		const uint8_t rpcb = frame[1];
		if (!SE050_PCB_IS_R(rpcb) ||
		    (rpcb & SE050_PCB_R_ERR_MASK) != SE050_PCB_R_ERR_NONE) {
			t1->needs_resync = true;
			return OSK_ERR_FORMAT;
		}
	}

	/* ---- Rueckweg: Antwortkette einsammeln.
	 * frame haelt bereits den ersten Antwortblock. */
	for (;;) {
		const uint8_t rpcb = frame[1];
		if (!SE050_PCB_IS_I(rpcb)) {
			t1->needs_resync = true;
			return OSK_ERR_FORMAT;
		}

		const size_t inf_len = len_of(frame);
		if (inf_len > rsp_cap - *rsp_len) {
			t1->needs_resync = true;
			return OSK_ERR_FORMAT;
		}
		memcpy(&rsp[*rsp_len], &frame[SE050_T1_PROLOGUE_LEN], inf_len);
		*rsp_len += inf_len;
		t1->seq_rx ^= 1u;

		if ((rpcb & SE050_PCB_I_MORE) == 0) {
			return OSK_OK;
		}

		/* Es folgt mehr: quittieren und den naechsten Block holen. */
		osk_result_t r = send_r_block(t1, t1->seq_rx, SE050_PCB_R_ERR_NONE);
		if (r != OSK_OK) {
			t1->needs_resync = true;
			return r;
		}
		r = frame_read(t1, frame, sizeof(frame), &frame_len, t1->bwt_ms);
		if (r != OSK_OK) {
			t1->needs_resync = true;
			return r;
		}
	}
}
