/* ISO-DEP als Karte. Siehe nfc_isodep.h.
 * Spezifikation: ISO/IEC 14443-4, Abschnitte 7.1 bis 7.5. */
#include <string.h>
#include "nfc_isodep.h"

/* Zeit kommt aus dem Kern, damit die Pruefungen sie stellen koennen. */
uint64_t osk_ctap_now_ms(void);

typedef struct {
	bool active; /* eine Kette wird gerade empfangen */
	size_t len;  /* schon zusammengesetzte Bytes */
	uint64_t started_ms;
	uint8_t buf[OSK_ISODEP_APDU_MAX];
} rx_state_t;

typedef struct {
	bool active; /* eine Antwort wird gerade in Rahmen zerlegt */
	size_t len;
	size_t at;
	uint8_t buf[OSK_ISODEP_APDU_MAX];
} tx_state_t;

static osk_isodep_send_fn g_send;
static osk_isodep_apdu_fn g_apdu;
static void *g_ctx;

static bool g_field;
static uint8_t g_block; /* eigene Blocknummer, 0 oder 1 */
static bool g_use_cid;	/* das Lesegeraet schickt eine Kennung mit */
static uint8_t g_cid;
static size_t g_frame_max = OSK_ISODEP_FRAME_MAX;

/* Der zuletzt gesendete Rahmen. ISO/IEC 14443-4 verlangt, dass ein
 * wiederholter Block mit derselben Blocknummer dieselbe Antwort bekommt -
 * sonst zaehlt das Lesegeraet zweimal, was einmal passiert ist. */
static uint8_t g_last[OSK_ISODEP_FRAME_MAX];
static size_t g_last_len;

static rx_state_t g_rx;
static tx_state_t g_tx;

static void send_frame(const uint8_t *frame, size_t len)
{
	if (!g_send || len == 0 || len > sizeof(g_last))
		return;
	memcpy(g_last, frame, len);
	g_last_len = len;
	g_send(frame, len, g_ctx);
}

static void resend_last(void)
{
	if (g_last_len && g_send)
		g_send(g_last, g_last_len, g_ctx);
}

/* Wie viele Nutzbytes in einen Rahmen gehen: Rahmen minus Steuerbyte und,
 * falls das Lesegeraet eine Kennung mitschickt, minus Kennungsbyte. */
static size_t inf_capacity(void)
{
	size_t cap = g_frame_max;

	if (cap > OSK_ISODEP_FRAME_MAX)
		cap = OSK_ISODEP_FRAME_MAX;
	/* Unter 16 Byte sieht ISO/IEC 14443-3 keinen Rahmen vor; weniger als
	 * ein Nutzbyte darf nie herauskommen. */
	if (cap < 16)
		cap = 16;
	cap -= 1;
	if (g_use_cid)
		cap -= 1;
	return cap;
}

/* Baut die Prologfelder eines Blocks auf und liefert die Schreibstelle. */
static size_t prologue(uint8_t *frame, uint8_t pcb)
{
	size_t at = 0;

	if (g_use_cid)
		pcb |= OSK_ISODEP_CID_PRESENT;
	frame[at++] = pcb;
	if (g_use_cid)
		frame[at++] = g_cid;
	return at;
}

static void send_simple(uint8_t pcb)
{
	uint8_t frame[3];
	const size_t at = prologue(frame, pcb);

	send_frame(frame, at);
}

static void rx_reset(void)
{
	/* Der Puffer kann eine Nachricht mit Geheimnissen getragen haben. */
	memset(&g_rx, 0, sizeof(g_rx));
}

static void tx_reset(void)
{
	memset(&g_tx, 0, sizeof(g_tx));
}

/* Die eigene Blocknummer umschalten (ISO/IEC 14443-4, Abschnitt 7.5.3). */
static void toggle(void)
{
	g_block ^= OSK_ISODEP_BLOCK_NUMBER;
}

/* Schickt das naechste Stueck der Antwort. Passt der Rest in einen Rahmen,
 * laeuft die Kette damit aus. */
static void send_next_chunk(void)
{
	uint8_t frame[OSK_ISODEP_FRAME_MAX];
	const size_t cap = inf_capacity();
	const size_t left = g_tx.len - g_tx.at;
	const size_t take = (left < cap) ? left : cap;
	const bool more = (left > take);
	uint8_t pcb = (uint8_t)(OSK_ISODEP_I_BLOCK | g_block);
	size_t at;

	if (more)
		pcb |= OSK_ISODEP_CHAINING;
	at = prologue(frame, pcb);
	if (take)
		memcpy(frame + at, g_tx.buf + g_tx.at, take);
	g_tx.at += take;
	send_frame(frame, at + take);
	/* Erst senden, dann umschalten: die Antwort traegt dieselbe
	 * Blocknummer wie der Block, der sie ausgeloest hat. */
	toggle();

	if (!more)
		tx_reset();
}

/* Eine vollstaendige APDU ist da: nach oben geben und die Antwort anstossen. */
static void dispatch(void)
{
	size_t len = 0;

	tx_reset();
	if (g_apdu)
		len = g_apdu(g_rx.buf, g_rx.len, g_tx.buf, sizeof(g_tx.buf), g_ctx);
	rx_reset();

	if (len == 0 || len > sizeof(g_tx.buf)) {
		/* Kein Bearbeiter oder eine Antwort, die nicht in den Puffer
		 * passt: lieber schweigen als etwas Halbes senden. Das
		 * Lesegeraet laeuft in seine eigene Zeitgrenze. */
		tx_reset();
		return;
	}
	g_tx.active = true;
	g_tx.len = len;
	g_tx.at = 0;
	send_next_chunk();
}

static void handle_i_block(uint8_t pcb, const uint8_t *inf, size_t inf_len)
{
	const uint8_t got = (uint8_t)(pcb & OSK_ISODEP_BLOCK_NUMBER);

	/* Eine andere Blocknummer als die erwartete heisst: unsere letzte
	 * Antwort kam nicht an. Wiederholen, nicht neu bearbeiten - sonst
	 * zaehlt das Lesegeraet zweimal, was einmal passiert ist. */
	if (got != g_block) {
		resend_last();
		return;
	}

	/* Ein Startblock mitten in einer Antwortkette verwirft die Antwort. */
	if (g_tx.active)
		tx_reset();

	if (!g_rx.active) {
		rx_reset();
		g_rx.active = true;
		g_rx.started_ms = osk_ctap_now_ms();
	}

	/* Laenge VOR dem Kopieren pruefen. Was nicht passt, bricht die Kette
	 * ab - es wird nie ein Teil uebernommen. */
	if (inf_len > sizeof(g_rx.buf) - g_rx.len) {
		/* Mehr als der Puffer: die Kette ist zu Ende, und zwar
		 * schweigend. Abschnitt 7.5.6 sieht fuer Protokollfehler das
		 * Schweigen der Karte vor - und ein Fehlerblock waere hier
		 * nur eine Auskunft darueber, wie gross der Puffer ist. */
		rx_reset();
		return;
	}
	if (inf_len)
		memcpy(g_rx.buf + g_rx.len, inf, inf_len);
	g_rx.len += inf_len;

	if (pcb & OSK_ISODEP_CHAINING) {
		/* Mehr kommt noch: bestaetigen und warten. Die Bestaetigung
		 * traegt die Blocknummer des bestaetigten Blocks. */
		send_simple((uint8_t)(OSK_ISODEP_R_ACK | g_block));
		toggle();
		return;
	}
	dispatch();
}

static void handle_r_block(uint8_t pcb)
{
	const bool nak = (pcb & OSK_ISODEP_CHAINING) != 0;
	const uint8_t got = (uint8_t)(pcb & OSK_ISODEP_BLOCK_NUMBER);

	if (nak) {
		/* Das Lesegeraet hat den letzten Rahmen nicht verstanden.
		 * Wiederholen, ohne die Blocknummer zu bewegen. */
		resend_last();
		return;
	}
	/* Eine Bestaetigung ohne laufende Kette, oder mit der falschen
	 * Blocknummer: der letzte Rahmen ist offenbar verloren gegangen. */
	if (!g_tx.active || got != g_block) {
		resend_last();
		return;
	}
	send_next_chunk();
}

static void handle_s_block(uint8_t pcb)
{
	/* S(WTX) kommt als Antwort auf unsere eigene Anforderung zurueck -
	 * darauf wird nicht noch einmal geantwortet. */
	if ((pcb & 0x30u) == 0x30u)
		return;

	/* S(DESELECT): bestaetigen und alles vergessen. */
	send_simple(OSK_ISODEP_S_DESELECT);
	rx_reset();
	tx_reset();
	g_block = 0;
}

void osk_isodep_rx_frame(const uint8_t *frame, size_t len)
{
	if (!frame || !g_field)
		return;
	/* Ein Rahmen ohne Steuerbyte ist keiner, und mehr als FSC nehmen wir
	 * nicht an. Beides wird verworfen, nicht teilweise gelesen. */
	if (len < 1 || len > OSK_ISODEP_FRAME_MAX)
		return;

	const uint8_t pcb = frame[0];
	size_t at = 1;

	/* Die Kennung des Lesegeraets, falls sie mitkommt. Wer sie einmal
	 * benutzt, benutzt sie immer - ein Wechsel mitten in der Sitzung ist
	 * ein Fehler und wird verworfen. */
	if (pcb & OSK_ISODEP_CID_PRESENT) {
		if (len < at + 1)
			return;
		const uint8_t cid = (uint8_t)(frame[at++] & 0x0fu);
		if (g_use_cid && cid != g_cid)
			return;
		g_use_cid = true;
		g_cid = cid;
	} else if (g_use_cid) {
		return;
	}

	/* Knotenadressen koennen wir nicht, und das sagen wir durch
	 * Schweigen - so verlangt es Abschnitt 7.5.6 fuer Protokollfehler. */
	if (pcb & OSK_ISODEP_NAD_PRESENT)
		return;

	const uint8_t kind = (uint8_t)(pcb & 0xc0u);

	if (kind == 0x00u) {
		/* I-Block. Bit 2 ist festgelegt auf 1, Bit 6 auf 0. */
		if ((pcb & OSK_ISODEP_I_BLOCK) == 0)
			return;
		if ((pcb & 0x20u) != 0)
			return;
		handle_i_block(pcb, frame + at, len - at);
		return;
	}
	if (kind == 0x80u) {
		/* R-Block. Er traegt nie Nutzdaten. */
		if ((pcb & 0x22u) != 0x22u)
			return;
		if (len != at)
			return;
		handle_r_block(pcb);
		return;
	}
	if (kind == 0xc0u) {
		if ((pcb & 0x02u) == 0)
			return;
		handle_s_block(pcb);
		return;
	}
	/* 0x40: nicht vergeben. */
}

void osk_isodep_request_more_time(uint8_t wtxm)
{
	uint8_t frame[4];
	size_t at;

	if (wtxm == 0 || wtxm > OSK_ISODEP_WTX_MAX)
		wtxm = OSK_ISODEP_WTX_MAX;
	at = prologue(frame, OSK_ISODEP_S_WTX);
	frame[at++] = wtxm;
	/* Nicht ueber g_last: eine Zeitanforderung ist kein Block, der
	 * wiederholt werden darf. */
	if (g_send)
		g_send(frame, at, g_ctx);
}

void osk_isodep_tick(void)
{
	if (!g_rx.active)
		return;
	if (osk_ctap_now_ms() - g_rx.started_ms < OSK_ISODEP_TIMEOUT_MS)
		return;
	rx_reset();
}

bool osk_isodep_busy(void)
{
	return g_tx.active;
}

void osk_isodep_field_on(void)
{
	g_field = true;
	g_block = 0;
	g_use_cid = false;
	g_cid = 0;
	g_last_len = 0;
	memset(g_last, 0, sizeof(g_last));
	rx_reset();
	tx_reset();
}

void osk_isodep_field_off(void)
{
	g_field = false;
	g_block = 0;
	g_use_cid = false;
	g_cid = 0;
	g_last_len = 0;
	memset(g_last, 0, sizeof(g_last));
	rx_reset();
	tx_reset();
}

void osk_isodep_set_frame_size(size_t fsd)
{
	g_frame_max = (fsd > OSK_ISODEP_FRAME_MAX) ? (size_t)OSK_ISODEP_FRAME_MAX : fsd;
}

osk_result_t osk_isodep_init(osk_isodep_send_fn send, osk_isodep_apdu_fn apdu, void *ctx)
{
	if (!send || !apdu)
		return OSK_ERR_ARG;

	g_send = send;
	g_apdu = apdu;
	g_ctx = ctx;
	g_frame_max = OSK_ISODEP_FRAME_MAX;
	osk_isodep_field_off();
	return OSK_OK;
}
