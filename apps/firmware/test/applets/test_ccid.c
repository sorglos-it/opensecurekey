/* Der CCID-Rahmen: Kopf, Reihenfolge, ATR, Deskriptor und das Weitergeben
 * der APDU. Geprueft gegen CCID 1.1, Abschnitte 4.1, 4.2, 5.1 und 6.2.
 *
 * Gelaufen ist das am PC gegen diese Pruefung - nicht gegen pcscd, nicht
 * gegen Windows, nicht gegen macOS. */
#include "test_util.h"
#include "usb_ccid.h"
#include "osk_piv.h"
#include "mocks.h"

/* Was der Leser hinausgeschickt hat. */
static uint8_t g_out[512];
static size_t g_out_len;
static unsigned g_out_count;

static void capture(const uint8_t *msg, size_t len, void *ctx)
{
	(void)ctx;
	g_out_count++;
	g_out_len = (len < sizeof(g_out)) ? len : sizeof(g_out);
	memcpy(g_out, msg, g_out_len);
}

static void send_msg(uint8_t type, uint8_t seq, const uint8_t *data, size_t len)
{
	uint8_t msg[300];

	g_out_len = 0u;
	memset(msg, 0, sizeof(msg));
	msg[0] = type;
	msg[1] = (uint8_t)len;
	msg[2] = (uint8_t)(len >> 8);
	msg[5] = 0x00u; /* Schlitz */
	msg[6] = seq;
	if (len != 0u)
		memcpy(msg + OSK_CCID_HEADER_LEN, data, len);
	osk_ccid_rx(msg, OSK_CCID_HEADER_LEN + len);
}

static void test_descriptor(void)
{
	uint8_t desc[OSK_CCID_DESC_LEN];
	uint8_t small[8];

	T_EQ("Deskriptor Laenge", osk_ccid_descriptor(desc, sizeof(desc)), OSK_CCID_DESC_LEN);
	T_EQ("bLength", desc[0], OSK_CCID_DESC_LEN);
	T_EQ("Typ 0x21", desc[1], 0x21);
	T_EQ("bcdCCID klein zuerst", desc[2], 0x10);
	T_EQ("bcdCCID gross", desc[3], 0x01);
	T_EQ("ein Schlitz", desc[4], 0x00);
	/* dwProtocols: nur T=1. */
	T_EQ("T=1", desc[6], 0x02);
	/* dwFeatures an Stelle 40..43: kurze APDU (0x00040000) muss drin sein. */
	T_EQ("kurze APDU angekuendigt", desc[42], 0x04);
	T_EQ("bClassGetResponse", desc[48], 0xff);
	T_EQ("ein belegter Schlitz", desc[53], 0x01);
	T_EQ("zu kleiner Puffer", osk_ccid_descriptor(small, sizeof(small)), 0);
	T_EQ("kein Puffer", osk_ccid_descriptor(NULL, 64u), 0);
}

static void test_atr(void)
{
	uint8_t atr[32];
	size_t len;
	uint8_t x = 0;

	len = osk_ccid_atr(atr, sizeof(atr));
	T_EQ("ATR Laenge", len, 18);
	T_EQ("direkte Konvention", atr[0], 0x3b);
	T_EQ("T0", atr[1], 0xf8);
	T_EQ("Protokoll T=1 angekuendigt", atr[5], 0x81);
	/* Die Pruefsumme ist das XOR ueber alles ab T0, einschliesslich ihrer
	 * selbst muss null herauskommen. */
	for (size_t i = 1; i < len; i++)
		x = (uint8_t)(x ^ atr[i]);
	T_EQ("ATR-Pruefsumme stimmt", x, 0);
	T_EQ("zu kleiner Puffer", osk_ccid_atr(atr, 4u), 0);
}

static void test_power(void)
{
	uint8_t atr[32];
	const size_t atr_len = osk_ccid_atr(atr, sizeof(atr));

	send_msg(OSK_CCID_PC_ICC_POWER_ON, 0x11u, NULL, 0u);
	T_EQ("Antwort ist ein Datenblock", g_out[0], OSK_CCID_RDR_DATA_BLOCK);
	T_EQ("Laenge im Kopf", g_out[1], (uint8_t)atr_len);
	T_EQ("Schlitz zurueck", g_out[5], 0x00);
	T_EQ("Folgenummer zurueck", g_out[6], 0x11);
	T_EQ("Karte da und aktiv", g_out[7], 0x00);
	T_EQ("kein Fehler", g_out[8], 0x00);
	T_EQ("ATR kommt mit", g_out_len, OSK_CCID_HEADER_LEN + atr_len);
	T_TRUE("ATR gleich", memcmp(g_out + OSK_CCID_HEADER_LEN, atr, atr_len) == 0);

	send_msg(OSK_CCID_PC_GET_SLOT_STATUS, 0x12u, NULL, 0u);
	T_EQ("Statusantwort", g_out[0], OSK_CCID_RDR_SLOT_STATUS);
	T_EQ("Status ok", g_out[7], 0x00);
	T_EQ("Statusantwort ohne Daten", g_out_len, OSK_CCID_HEADER_LEN);

	send_msg(OSK_CCID_PC_ICC_POWER_OFF, 0x13u, NULL, 0u);
	T_EQ("aus: Statusantwort", g_out[0], OSK_CCID_RDR_SLOT_STATUS);
	T_EQ("aus: Karte da, aber nicht aktiv", g_out[7], 0x01);

	/* Im ausgeschalteten Zustand geht keine APDU mehr durch. */
	{
		static const uint8_t select[] = {0x00, 0xa4, 0x04, 0x00};

		send_msg(OSK_CCID_PC_XFR_BLOCK, 0x14u, select, sizeof(select));
		T_EQ("aus: Datenblock mit Fehler", g_out[0], OSK_CCID_RDR_DATA_BLOCK);
		T_EQ("aus: Kommando gescheitert", g_out[7] & 0xc0u, OSK_CCID_CMD_FAILED);
	}

	send_msg(OSK_CCID_PC_ICC_POWER_ON, 0x15u, NULL, 0u);
	T_EQ("wieder an", g_out[7], 0x00);
}

static void test_parameters(void)
{
	send_msg(OSK_CCID_PC_GET_PARAMETERS, 0x20u, NULL, 0u);
	T_EQ("Parameterantwort", g_out[0], OSK_CCID_RDR_PARAMETERS);
	T_EQ("Protokoll T=1", g_out[9], 0x01);
	T_EQ("sieben Parameterbytes", g_out_len, OSK_CCID_HEADER_LEN + 7u);
	T_EQ("IFSC", g_out[OSK_CCID_HEADER_LEN + 5u], 0xfe);

	send_msg(OSK_CCID_PC_RESET_PARAMETERS, 0x21u, NULL, 0u);
	T_EQ("Zuruecksetzen antwortet gleich", g_out[0], OSK_CCID_RDR_PARAMETERS);

	send_msg(OSK_CCID_PC_SET_RATE_AND_CLOCK, 0x22u, NULL, 0u);
	T_EQ("Rate und Takt", g_out[0], OSK_CCID_RDR_RATE_AND_CLOCK);
	T_EQ("acht Bytes", g_out_len, OSK_CCID_HEADER_LEN + 8u);
}

static void test_rejects(void)
{
	uint8_t msg[32];

	/* Falscher Schlitz. */
	memset(msg, 0, sizeof(msg));
	msg[0] = OSK_CCID_PC_GET_SLOT_STATUS;
	msg[5] = 0x01u;
	msg[6] = 0x30u;
	g_out_len = 0u;
	osk_ccid_rx(msg, OSK_CCID_HEADER_LEN);
	T_EQ("falscher Schlitz: Fehler", g_out[7] & 0xc0u, OSK_CCID_CMD_FAILED);
	T_EQ("falscher Schlitz: Grund", g_out[8], OSK_CCID_ERR_BAD_SLOT);

	/* Angekuendigte Laenge passt nicht zur gelieferten. */
	memset(msg, 0, sizeof(msg));
	msg[0] = OSK_CCID_PC_XFR_BLOCK;
	msg[1] = 0x08u;
	msg[6] = 0x31u;
	g_out_len = 0u;
	osk_ccid_rx(msg, OSK_CCID_HEADER_LEN + 2u);
	T_EQ("falsche Laenge: Grund", g_out[8], OSK_CCID_ERR_BAD_LENGTH);

	/* Kein Kopf: keine Antwort, weil nicht klar ist, an wen. */
	g_out_len = 0u;
	g_out_count = 0u;
	osk_ccid_rx(msg, 4u);
	T_EQ("halber Kopf bleibt unbeantwortet", g_out_count, 0);
	osk_ccid_rx(NULL, 20u);
	T_EQ("Nullzeiger bleibt unbeantwortet", g_out_count, 0);

	/* Unbekanntes Kommando. */
	send_msg(0x55u, 0x32u, NULL, 0u);
	T_EQ("unbekannt: Status", g_out[0], OSK_CCID_RDR_SLOT_STATUS);
	T_EQ("unbekannt: Grund", g_out[8], OSK_CCID_ERR_CMD_NOT_SUPPORTED);

	/* Escape und Secure lehnen wir ausdruecklich ab. */
	send_msg(OSK_CCID_PC_ESCAPE, 0x33u, NULL, 0u);
	T_EQ("Escape: eigene Antwortart", g_out[0], OSK_CCID_RDR_ESCAPE);
	T_EQ("Escape: nicht unterstuetzt", g_out[8], OSK_CCID_ERR_CMD_NOT_SUPPORTED);
	send_msg(OSK_CCID_PC_SECURE, 0x34u, NULL, 0u);
	T_EQ("Secure: Datenblock", g_out[0], OSK_CCID_RDR_DATA_BLOCK);
	T_EQ("Secure: nicht unterstuetzt", g_out[8], OSK_CCID_ERR_CMD_NOT_SUPPORTED);

	/* wLevelParameter muss bei kurzer APDU null sein. */
	memset(msg, 0, sizeof(msg));
	msg[0] = OSK_CCID_PC_XFR_BLOCK;
	msg[1] = 0x04u;
	msg[6] = 0x35u;
	msg[9] = 0x01u; /* obere Haelfte von wLevelParameter */
	msg[10] = 0x00u;
	msg[11] = 0xa4u;
	msg[12] = 0x04u;
	msg[13] = 0x00u;
	g_out_len = 0u;
	osk_ccid_rx(msg, OSK_CCID_HEADER_LEN + 4u);
	T_EQ("Stufenparameter: Grund", g_out[8], OSK_CCID_ERR_BAD_LEVEL_PARAM);

	/* Zu kurz fuer eine APDU. */
	{
		static const uint8_t stub[] = {0x00, 0xa4, 0x04};

		send_msg(OSK_CCID_PC_XFR_BLOCK, 0x36u, stub, sizeof(stub));
		T_EQ("drei Bytes: Grund", g_out[8], OSK_CCID_ERR_BAD_LENGTH);
	}
}

/* Der Weg von Ende zu Ende: eine echte SELECT-APDU hinein, das
 * Statuswort heraus. */
static void test_through_to_applet(void)
{
	uint8_t msg[64];
	size_t at = 0;

	send_msg(OSK_CCID_PC_ICC_POWER_ON, 0x40u, NULL, 0u);

	msg[at++] = 0x00u;
	msg[at++] = 0xa4u;
	msg[at++] = 0x04u;
	msg[at++] = 0x00u;
	msg[at++] = (uint8_t)OSK_PIV_AID_LEN;
	memcpy(msg + at, osk_piv_aid, OSK_PIV_AID_LEN);
	at += OSK_PIV_AID_LEN;
	msg[at++] = 0x00u; /* Le */

	send_msg(OSK_CCID_PC_XFR_BLOCK, 0x41u, msg, at);
	T_EQ("Datenblock zurueck", g_out[0], OSK_CCID_RDR_DATA_BLOCK);
	T_EQ("kein Fehler", g_out[7], 0x00);
	T_TRUE("etwas kam zurueck", g_out_len > OSK_CCID_HEADER_LEN + 2u);
	T_EQ("Statuswort hoch", g_out[g_out_len - 2u], 0x90);
	T_EQ("Statuswort tief", g_out[g_out_len - 1u], 0x00);
	/* Die Antwort beginnt mit dem Application Property Template. */
	T_EQ("Vorlage 0x61", g_out[OSK_CCID_HEADER_LEN], 0x61);

	/* Eine unbekannte Kennung wird abgewiesen. */
	at = 0;
	msg[at++] = 0x00u;
	msg[at++] = 0xa4u;
	msg[at++] = 0x04u;
	msg[at++] = 0x00u;
	msg[at++] = 0x05u;
	memset(msg + at, 0x11, 5u);
	at += 5u;
	send_msg(OSK_CCID_PC_XFR_BLOCK, 0x42u, msg, at);
	T_EQ("unbekannte Anwendung: 6A82 hoch", g_out[g_out_len - 2u], 0x6a);
	T_EQ("unbekannte Anwendung: 6A82 tief", g_out[g_out_len - 1u], 0x82);
}

int main(void)
{
	mock_reset_all();
	T_EQ("Leser gestartet", osk_ccid_init(capture, NULL), OSK_OK);
	T_EQ("ohne Sender kein Start", osk_ccid_init(NULL, NULL), OSK_ERR_ARG);

	test_descriptor();
	test_atr();
	test_power();
	test_parameters();
	test_rejects();
	test_through_to_applet();
	return t_report("test_ccid");
}
