/* CTAPHID: Rahmen und Kanaele. Siehe usb_hid.h.
 * Spezifikation: CTAP 2.2, Abschnitt 11.2. */
#include <string.h>
#include <stdbool.h>
#include "usb_hid.h"
#include "osk_transport.h"
#include "osk_ctap.h"

/* So viele Kanaele merken wir uns. Mehr Programme sprechen ein
 * Sicherheitsschluessel nicht gleichzeitig an. */
#define OSK_HID_MAX_CHANNELS 4

/* Fassung des Protokolls (Abschnitt 11.2.9.1.3). */
#define OSK_HID_PROTOCOL_VERSION 2

typedef struct {
	uint32_t cid;
	bool used;
} channel_t;

typedef struct {
	bool busy; /* eine Nachricht wird gerade zusammengesetzt */
	uint32_t cid;
	uint8_t cmd;
	uint16_t expect; /* angekuendigte Gesamtlaenge */
	uint16_t have;
	int next_seq; /* -1 = noch kein Fortsetzungspaket erwartet */
	uint64_t started_ms;
	uint8_t buf[OSK_HID_BUF_LEN];
} assembly_t;

static osk_hid_send_fn g_send;
static void *g_ctx;
static channel_t g_channels[OSK_HID_MAX_CHANNELS];
static uint32_t g_next_cid = 1;
static assembly_t g_asm;
static bool g_cancel;

/* Zeit kommt aus dem Kern, damit die Tests sie stellen koennen. */
uint64_t osk_ctap_now_ms(void);

static void send_packet(const uint8_t *p)
{
	if (g_send)
		g_send(p, g_ctx);
}

/* Schickt eine ganze Nachricht als Kette von Paketen. */
static void send_message(uint32_t cid, uint8_t cmd, const uint8_t *data, size_t len)
{
	uint8_t pkt[OSK_HID_PACKET_LEN];
	size_t at = 0;

	if (len > OSK_HID_MAX_MSG)
		return;

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = cmd;
	pkt[5] = (uint8_t)(len >> 8);
	pkt[6] = (uint8_t)len;

	const size_t first = (len < OSK_HID_INIT_DATA) ? len : OSK_HID_INIT_DATA;
	if (first)
		memcpy(pkt + 7, data, first);
	send_packet(pkt);
	at = first;

	uint8_t seq = 0;
	while (at < len) {
		const size_t take = (len - at < OSK_HID_CONT_DATA) ? len - at : OSK_HID_CONT_DATA;
		memset(pkt, 0, sizeof(pkt));
		pkt[0] = (uint8_t)(cid >> 24);
		pkt[1] = (uint8_t)(cid >> 16);
		pkt[2] = (uint8_t)(cid >> 8);
		pkt[3] = (uint8_t)cid;
		pkt[4] = seq++;
		memcpy(pkt + 5, data + at, take);
		send_packet(pkt);
		at += take;
	}
}

static void send_error(uint32_t cid, uint8_t code)
{
	send_message(cid, CTAPHID_ERROR, &code, 1);
}

void osk_hid_keepalive(uint8_t status)
{
	if (g_asm.busy)
		send_message(g_asm.cid, CTAPHID_KEEPALIVE, &status, 1);
}

static bool channel_known(uint32_t cid)
{
	for (unsigned i = 0; i < OSK_HID_MAX_CHANNELS; i++) {
		if (g_channels[i].used && g_channels[i].cid == cid)
			return true;
	}
	return false;
}

/* Vergibt einen Kanal. Ist das Verzeichnis voll, wird der aelteste Platz
 * ueberschrieben - ein Programm, das danach weiterredet, bekommt
 * ERR_INVALID_CHANNEL und faengt mit INIT neu an. */
static uint32_t channel_new(void)
{
	uint32_t cid = g_next_cid++;

	if (g_next_cid == OSK_HID_BROADCAST_CID || g_next_cid == 0)
		g_next_cid = 1;

	for (unsigned i = 0; i < OSK_HID_MAX_CHANNELS; i++) {
		if (!g_channels[i].used) {
			g_channels[i].used = true;
			g_channels[i].cid = cid;
			return cid;
		}
	}
	static unsigned round_robin;
	g_channels[round_robin % OSK_HID_MAX_CHANNELS].cid = cid;
	round_robin++;
	return cid;
}

static void assembly_reset(void)
{
	/* Der Puffer kann eine Nachricht mit Geheimnissen getragen haben. */
	memset(&g_asm, 0, sizeof(g_asm));
	g_asm.next_seq = -1;
}

/* Beantwortet eine vollstaendige Nachricht. */
static void dispatch(void)
{
	uint8_t out[OSK_HID_BUF_LEN];
	osk_request_t req;
	osk_response_t res;

	const uint32_t cid = g_asm.cid;
	const uint8_t cmd = g_asm.cmd;

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_USB_HID;
	req.data = g_asm.buf;
	req.len = g_asm.have;
	res.data = out;
	res.cap = sizeof(out);

	switch (cmd) {
	case CTAPHID_PING:
		send_message(cid, CTAPHID_PING, g_asm.buf, g_asm.have);
		break;
	case CTAPHID_CBOR: {
		if (g_asm.have < 1) {
			send_error(cid, OSK_HID_ERR_INVALID_LEN);
			break;
		}
		/* Vor der langen Arbeit ein Lebenszeichen: der Nutzer soll
		 * drueckten, und das Programm darf nicht ins Zeitlimit laufen. */
		osk_hid_keepalive(OSK_HID_STATUS_UPNEEDED);
		if (osk_ctap_handle(&req, &res) != OSK_OK || res.len < 1) {
			send_error(cid, OSK_HID_ERR_OTHER);
			break;
		}
		if (g_cancel) {
			/* Abgebrochen: die Antwort auf die CBOR-Anfrage ist der
			 * Abbruchfehler, nicht ein Transportfehler. */
			const uint8_t cancelled = 0x2d; /* CTAP2_ERR_KEEPALIVE_CANCEL */
			send_message(cid, CTAPHID_CBOR, &cancelled, 1);
			break;
		}
		send_message(cid, CTAPHID_CBOR, out, res.len);
		break;
	}
	case CTAPHID_MSG:
		if (osk_ctap_handle_u2f(&req, &res) != OSK_OK || res.len < 2) {
			send_error(cid, OSK_HID_ERR_OTHER);
			break;
		}
		send_message(cid, CTAPHID_MSG, out, res.len);
		break;
	case CTAPHID_WINK:
		if (g_asm.have != 0) {
			send_error(cid, OSK_HID_ERR_INVALID_LEN);
			break;
		}
		send_message(cid, CTAPHID_WINK, NULL, 0);
		break;
	case CTAPHID_LOCK:
		/* Kanalsperre ist laut Spezifikation freiwillig. Wir
		 * unterstuetzen sie nicht und sagen das auch so. */
		send_error(cid, OSK_HID_ERR_INVALID_CMD);
		break;
	default:
		send_error(cid, OSK_HID_ERR_INVALID_CMD);
		break;
	}

	memset(out, 0, sizeof(out));
	assembly_reset();
	g_cancel = false;
}

static void handle_init(uint32_t cid, const uint8_t *data, size_t len)
{
	uint8_t reply[17];

	if (len != 8) {
		send_error(cid, OSK_HID_ERR_INVALID_LEN);
		return;
	}

	uint32_t use_cid = cid;
	if (cid == OSK_HID_BROADCAST_CID) {
		use_cid = channel_new();
	} else {
		/* INIT auf einem bekannten Kanal raeumt ihn auf. */
		if (!channel_known(cid)) {
			send_error(cid, OSK_HID_ERR_INVALID_CHANNEL);
			return;
		}
		if (g_asm.busy && g_asm.cid == cid)
			assembly_reset();
	}

	memcpy(reply, data, 8);
	reply[8] = (uint8_t)(use_cid >> 24);
	reply[9] = (uint8_t)(use_cid >> 16);
	reply[10] = (uint8_t)(use_cid >> 8);
	reply[11] = (uint8_t)use_cid;
	reply[12] = OSK_HID_PROTOCOL_VERSION;
	reply[13] = 0; /* Hauptnummer des Geraets */
	reply[14] = 0;
	reply[15] = 0;
	reply[16] = (uint8_t)(OSK_HID_CAP_CBOR | OSK_HID_CAP_WINK);

	send_message(cid, CTAPHID_INIT, reply, sizeof(reply));
}

void osk_hid_rx_packet(const uint8_t packet[OSK_HID_PACKET_LEN])
{
	if (!packet)
		return;

	const uint32_t cid = ((uint32_t)packet[0] << 24) | ((uint32_t)packet[1] << 16) |
			     ((uint32_t)packet[2] << 8) | (uint32_t)packet[3];
	const uint8_t b4 = packet[4];
	const bool is_init_packet = (b4 & 0x80) != 0;

	/* Kanal 0 ist reserviert. */
	if (cid == 0) {
		send_error(cid, OSK_HID_ERR_INVALID_CHANNEL);
		return;
	}

	if (is_init_packet) {
		const uint16_t bcnt = (uint16_t)(((uint16_t)packet[5] << 8) | packet[6]);

		if (b4 == CTAPHID_INIT) {
			handle_init(cid, packet + 7, bcnt);
			return;
		}

		/* Alles ausser INIT braucht einen vergebenen Kanal. */
		if (cid == OSK_HID_BROADCAST_CID || !channel_known(cid)) {
			send_error(cid, OSK_HID_ERR_INVALID_CHANNEL);
			return;
		}

		if (b4 == CTAPHID_CANCEL) {
			/* Auf CANCEL selbst wird nie geantwortet. Laeuft auf
			 * diesem Kanal gerade nichts, wird es uebergangen. */
			if (g_asm.busy && g_asm.cid == cid)
				g_cancel = true;
			return;
		}

		if (g_asm.busy && g_asm.cid != cid) {
			send_error(cid, OSK_HID_ERR_CHANNEL_BUSY);
			return;
		}
		if (g_asm.busy && g_asm.cid == cid) {
			/* Ein neues Startpaket mitten in einer Nachricht ist ein
			 * Neuanfang desselben Kanals. */
			assembly_reset();
		}

		if (bcnt > OSK_HID_BUF_LEN) {
			send_error(cid, OSK_HID_ERR_INVALID_LEN);
			return;
		}

		assembly_reset();
		g_asm.busy = true;
		g_asm.cid = cid;
		g_asm.cmd = b4;
		g_asm.expect = bcnt;
		g_asm.started_ms = osk_ctap_now_ms();

		const uint16_t take =
		    (bcnt < OSK_HID_INIT_DATA) ? bcnt : (uint16_t)OSK_HID_INIT_DATA;
		if (take)
			memcpy(g_asm.buf, packet + 7, take);
		g_asm.have = take;
		g_asm.next_seq = 0;

		if (g_asm.have >= g_asm.expect)
			dispatch();
		return;
	}

	/* Fortsetzungspaket. */
	if (!g_asm.busy) {
		/* Ohne Startpaket wird es stillschweigend verworfen. */
		return;
	}
	if (g_asm.cid != cid) {
		send_error(cid, OSK_HID_ERR_CHANNEL_BUSY);
		return;
	}
	if (b4 != (uint8_t)g_asm.next_seq) {
		send_error(cid, OSK_HID_ERR_INVALID_SEQ);
		assembly_reset();
		return;
	}

	const uint16_t left = (uint16_t)(g_asm.expect - g_asm.have);
	const uint16_t take = (left < OSK_HID_CONT_DATA) ? left : (uint16_t)OSK_HID_CONT_DATA;
	/* Laengen stehen vor dem Kopieren fest - der Puffer kann nicht
	 * ueberlaufen, weil expect schon gegen OSK_HID_BUF_LEN geprueft wurde. */
	memcpy(g_asm.buf + g_asm.have, packet + 5, take);
	g_asm.have = (uint16_t)(g_asm.have + take);
	g_asm.next_seq++;
	if (g_asm.next_seq > 127) {
		send_error(cid, OSK_HID_ERR_INVALID_SEQ);
		assembly_reset();
		return;
	}

	if (g_asm.have >= g_asm.expect)
		dispatch();
}

void osk_hid_tick(void)
{
	if (!g_asm.busy || g_asm.have >= g_asm.expect)
		return;
	if (osk_ctap_now_ms() - g_asm.started_ms < OSK_HID_TIMEOUT_MS)
		return;

	const uint32_t cid = g_asm.cid;
	assembly_reset();
	send_error(cid, OSK_HID_ERR_MSG_TIMEOUT);
}

osk_result_t osk_hid_init(osk_hid_send_fn send, void *ctx)
{
	if (!send)
		return OSK_ERR_ARG;

	g_send = send;
	g_ctx = ctx;
	memset(g_channels, 0, sizeof(g_channels));
	g_next_cid = 1;
	g_cancel = false;
	assembly_reset();

	return osk_transport_register(OSK_LINK_USB_HID, osk_ctap_handle);
}
