/* CCID-Rahmen. Siehe usb_ccid.h.
 * Spezifikation: CCID 1.1, Abschnitte 4.1, 4.2, 5.1 und 6.2. */
#include <string.h>
#include "usb_ccid.h"
#include "osk_transport.h"
#include "osk_applets.h"

/* Die ATR einer Karte, die immer steckt. Aufbau nach ISO/IEC 7816-3
 * Abschnitt 8: TS, T0, dann die angekuendigten Schnittstellenbytes, die
 * historischen Bytes und die Pruefsumme.
 *
 * TS   0x3B  direkte Konvention
 * T0   0xF8  TA1..TD1 vorhanden, 8 historische Bytes
 * TA1  0x13  Fi/Di
 * TB1  0x00  keine Programmierspannung
 * TC1  0x00  keine zusaetzliche Schutzzeit
 * TD1  0x81  TD2 folgt, Protokoll T=1
 * TD2  0x31  TA3 und TB3 folgen, T=1
 * TA3  0xFE  IFSC 254
 * TB3  0x15
 * historisch: Kategorie 0x80, dann COMPACT-TLV 0x73 (Kartenfaehigkeiten,
 *             3 Bytes) und "OSK" als Kennung
 * TCK  wird gerechnet, nicht hingeschrieben - siehe atr_tck()
 */
static const uint8_t g_atr_body[] = {
    0x3b, 0xf8, 0x13, 0x00, 0x00, 0x81, 0x31, 0xfe, 0x15,
    0x80, 0x73, 0xc0, 0x21, 0x13, 0x4f, 0x53, 0x4b,
};

static osk_ccid_send_fn g_send;
static void *g_ctx;
static bool g_powered = true;

/* Pruefsumme: XOR ueber alles ab T0. TS bleibt aussen vor (7816-3 8.2.5). */
static uint8_t atr_tck(void)
{
	uint8_t x = 0;

	for (size_t i = 1u; i < sizeof(g_atr_body); i++)
		x ^= g_atr_body[i];
	return x;
}

size_t osk_ccid_atr(uint8_t *out, size_t cap)
{
	const size_t len = sizeof(g_atr_body) + 1u;

	if (out == NULL || cap < len)
		return 0u;
	memcpy(out, g_atr_body, sizeof(g_atr_body));
	out[sizeof(g_atr_body)] = atr_tck();
	return len;
}

/* T=1-Parameter, wie GetParameters sie meldet (Abschnitt 6.2.3, bProtocolNum 1). */
static const uint8_t g_params_t1[] = {
    0x13, /* bmFindexDindex - passt zu TA1 der ATR */
    0x10, /* bmTCCKST1: T=1, direkte Konvention, CRC aus */
    0x00, /* bGuardTimeT1 */
    0x15, /* bmWaitingIntegersT1 - wie TB3 */
    0x00, /* bClockStop: nicht erlaubt */
    0xfe, /* bIFSC */
    0x00, /* bNadValue */
};

static void put_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

size_t osk_ccid_descriptor(uint8_t *out, size_t cap)
{
	uint8_t *p = out;

	if (out == NULL || cap < OSK_CCID_DESC_LEN)
		return 0u;
	memset(out, 0, OSK_CCID_DESC_LEN);

	*p++ = OSK_CCID_DESC_LEN; /* bLength */
	*p++ = 0x21u;		  /* bDescriptorType: CCID-Klasse */
	*p++ = 0x10u;		  /* bcdCCID 1.10, klein zuerst */
	*p++ = 0x01u;
	*p++ = 0x00u;		 /* bMaxSlotIndex: ein Schlitz */
	*p++ = 0x07u;		 /* bVoltageSupport: 5 V, 3 V, 1,8 V */
	put_u32(p, 0x00000002u); /* dwProtocols: nur T=1 */
	p += 4;
	put_u32(p, 4000u); /* dwDefaultClock in kHz */
	p += 4;
	put_u32(p, 5000u); /* dwMaximumClock */
	p += 4;
	*p++ = 0x00u;	   /* bNumClockSupported: nur die beiden oben */
	put_u32(p, 9600u); /* dwDataRate */
	p += 4;
	put_u32(p, 625000u); /* dwMaxDataRate */
	p += 4;
	*p++ = 0x00u;	  /* bNumDataRatesSupported */
	put_u32(p, 254u); /* dwMaxIFSD */
	p += 4;
	put_u32(p, 0u); /* dwSynchProtocols: keine */
	p += 4;
	put_u32(p, 0u); /* dwMechanical: kein Schacht, keine Mechanik */
	p += 4;
	/* dwFeatures: Parameter selbst setzen (0x02), Spannung, Takt, Baudrate
	 * und PPS selbst waehlen (0x08, 0x10, 0x20, 0x40), IFSD selbst
	 * aushandeln (0x400) und Austausch auf Ebene der kurzen APDU
	 * (0x40000). Die Karte steckt fest, darum kein automatisches
	 * Einschalten beim Einstecken. */
	put_u32(p, 0x0004047au);
	p += 4;
	put_u32(p, OSK_CCID_MAX_MSG); /* dwMaxCCIDMessageLength */
	p += 4;
	*p++ = 0xffu; /* bClassGetResponse: wie im Kommando */
	*p++ = 0xffu; /* bClassEnvelope */
	*p++ = 0x00u; /* wLcdLayout: kein Display am Leser */
	*p++ = 0x00u;
	*p++ = 0x00u; /* bPINSupport: keine Tastatur am Leser */
	*p++ = 0x01u; /* bMaxCCIDBusySlots */

	return OSK_CCID_DESC_LEN;
}

/* --- Antworten ---------------------------------------------------------- */

static void send_msg(uint8_t type, uint8_t slot, uint8_t seq, uint8_t status, uint8_t error,
		     uint8_t last, const uint8_t *data, size_t len)
{
	uint8_t msg[OSK_CCID_MAX_MSG];

	if (g_send == NULL)
		return;
	if (len > OSK_CCID_MAX_DATA)
		return;

	msg[0] = type;
	put_u32(msg + 1, (uint32_t)len);
	msg[5] = slot;
	msg[6] = seq;
	msg[7] = status;
	msg[8] = error;
	msg[9] = last;
	if (len != 0u && data != NULL)
		memcpy(msg + OSK_CCID_HEADER_LEN, data, len);
	g_send(msg, OSK_CCID_HEADER_LEN + len, g_ctx);
}

static uint8_t icc_status(void)
{
	/* 0 = Karte da und aktiv, 1 = da und aus. Weg ist sie nie. */
	return g_powered ? (uint8_t)0x00u : (uint8_t)0x01u;
}

static void send_status(uint8_t slot, uint8_t seq, uint8_t cmd_status, uint8_t error)
{
	send_msg(OSK_CCID_RDR_SLOT_STATUS, slot, seq, (uint8_t)(icc_status() | cmd_status), error,
		 0x00u, NULL, 0u);
}

static void send_data(uint8_t slot, uint8_t seq, const uint8_t *data, size_t len)
{
	send_msg(OSK_CCID_RDR_DATA_BLOCK, slot, seq, icc_status(), 0x00u, 0x00u, data, len);
}

static void send_data_failed(uint8_t slot, uint8_t seq, uint8_t error)
{
	send_msg(OSK_CCID_RDR_DATA_BLOCK, slot, seq, (uint8_t)(icc_status() | OSK_CCID_CMD_FAILED),
		 error, 0x00u, NULL, 0u);
}

/* --- Eingang ------------------------------------------------------------ */

static void handle_xfr(uint8_t slot, uint8_t seq, const uint8_t *msg, size_t data_len)
{
	uint8_t out[OSK_CCID_MAX_DATA];
	osk_request_t req;
	osk_response_t res;
	osk_result_t rc;

	/* wLevelParameter muss bei kurzer APDU null sein (Abschnitt 6.1.4). */
	if (msg[8] != 0u || msg[9] != 0u) {
		send_data_failed(slot, seq, OSK_CCID_ERR_BAD_LEVEL_PARAM);
		return;
	}
	if (!g_powered) {
		send_data_failed(slot, seq, OSK_CCID_ERR_HW_ERROR);
		return;
	}
	if (data_len < 4u) {
		send_data_failed(slot, seq, OSK_CCID_ERR_BAD_LENGTH);
		return;
	}

	req.link = OSK_LINK_USB_CCID;
	req.data = msg + OSK_CCID_HEADER_LEN;
	req.len = data_len;
	res.data = out;
	res.cap = sizeof(out);
	res.len = 0u;

	rc = osk_applets_handle(&req, &res);
	if (rc != OSK_OK || res.len == 0u) {
		/* Der Verteiler antwortet selbst mit einem Statuswort, auch
		 * auf Unsinn. Kommt hier trotzdem nichts an, ist das ein
		 * Fehler bei uns und keine Kartenantwort. */
		send_data_failed(slot, seq, OSK_CCID_ERR_HW_ERROR);
		return;
	}
	send_data(slot, seq, out, res.len);
}

void osk_ccid_rx(const uint8_t *msg, size_t len)
{
	uint8_t type;
	uint8_t slot;
	uint8_t seq;
	uint32_t announced;
	size_t data_len;

	if (msg == NULL || len < OSK_CCID_HEADER_LEN)
		return; /* kein Kopf, keine Antwort - wir wissen nicht an wen */

	type = msg[0];
	announced = (uint32_t)msg[1] | ((uint32_t)msg[2] << 8) | ((uint32_t)msg[3] << 16) |
		    ((uint32_t)msg[4] << 24);
	slot = msg[5];
	seq = msg[6];
	data_len = len - OSK_CCID_HEADER_LEN;

	if (announced != data_len || data_len > OSK_CCID_MAX_DATA) {
		send_status(slot, seq, OSK_CCID_CMD_FAILED, OSK_CCID_ERR_BAD_LENGTH);
		return;
	}
	if (slot != 0u) {
		send_status(slot, seq, OSK_CCID_CMD_FAILED, OSK_CCID_ERR_BAD_SLOT);
		return;
	}

	switch (type) {
	case OSK_CCID_PC_ICC_POWER_ON: {
		uint8_t atr[sizeof(g_atr_body) + 1u];
		const size_t n = osk_ccid_atr(atr, sizeof(atr));

		g_powered = true;
		osk_applets_reset();
		send_data(slot, seq, atr, n);
		break;
	}
	case OSK_CCID_PC_ICC_POWER_OFF:
		g_powered = false;
		osk_applets_reset();
		send_status(slot, seq, OSK_CCID_CMD_OK, 0x00u);
		break;
	case OSK_CCID_PC_GET_SLOT_STATUS:
	case OSK_CCID_PC_ABORT:
		send_status(slot, seq, OSK_CCID_CMD_OK, 0x00u);
		break;
	case OSK_CCID_PC_XFR_BLOCK:
		handle_xfr(slot, seq, msg, data_len);
		break;
	case OSK_CCID_PC_GET_PARAMETERS:
	case OSK_CCID_PC_RESET_PARAMETERS:
	case OSK_CCID_PC_SET_PARAMETERS:
		/* Wir nehmen jede Einstellung an und melden die eigene zurueck:
		 * geaendert wird nichts, die Karte sitzt fest am I2C-Bus und
		 * nicht an einer Kartenschnittstelle. bProtocolNum = 1. */
		send_msg(OSK_CCID_RDR_PARAMETERS, slot, seq, icc_status(), 0x00u, 0x01u,
			 g_params_t1, sizeof(g_params_t1));
		break;
	case OSK_CCID_PC_ICC_CLOCK:
		send_status(slot, seq, OSK_CCID_CMD_OK, 0x00u);
		break;
	case OSK_CCID_PC_SET_RATE_AND_CLOCK: {
		/* Die geforderte Rate wird nicht uebernommen; wir melden die
		 * eigene (Abschnitt 6.2.5 verlangt beide Werte). */
		uint8_t body[8];

		put_u32(body, 4000u);
		put_u32(body + 4, 9600u);
		send_msg(OSK_CCID_RDR_RATE_AND_CLOCK, slot, seq, icc_status(), 0x00u, 0x00u, body,
			 sizeof(body));
		break;
	}
	case OSK_CCID_PC_ESCAPE:
		send_msg(OSK_CCID_RDR_ESCAPE, slot, seq,
			 (uint8_t)(icc_status() | OSK_CCID_CMD_FAILED),
			 OSK_CCID_ERR_CMD_NOT_SUPPORTED, 0x00u, NULL, 0u);
		break;
	case OSK_CCID_PC_SECURE:
		/* Keine Tastatur am Leser: die PIN wird nie hier eingegeben. */
		send_data_failed(slot, seq, OSK_CCID_ERR_CMD_NOT_SUPPORTED);
		break;
	case OSK_CCID_PC_T0_APDU:
	case OSK_CCID_PC_MECHANICAL:
	default:
		send_status(slot, seq, OSK_CCID_CMD_FAILED, OSK_CCID_ERR_CMD_NOT_SUPPORTED);
		break;
	}
}

osk_result_t osk_ccid_init(osk_ccid_send_fn send, void *ctx)
{
	osk_result_t rc;

	if (send == NULL)
		return OSK_ERR_ARG;
	g_send = send;
	g_ctx = ctx;
	g_powered = true;

	rc = osk_applets_init();
	if (rc != OSK_OK)
		return rc;
	return osk_transport_register(OSK_LINK_USB_CCID, osk_applets_handle);
}
