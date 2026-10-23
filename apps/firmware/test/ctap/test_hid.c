/* CTAPHID: Kanal holen, Rahmen, Kettenbildung, Zeitgrenze, Abbruch.
 * Spezifikation: CTAP 2.2, Abschnitt 11.2. */
#include "ctap_helper.h"
#include "usb_hid.h"

#define MAX_SENT 64

static uint8_t g_sent[MAX_SENT][OSK_HID_PACKET_LEN];
static size_t g_sent_count;

static void sink(const uint8_t packet[OSK_HID_PACKET_LEN], void *ctx)
{
	(void)ctx;
	if (g_sent_count < MAX_SENT)
		memcpy(g_sent[g_sent_count], packet, OSK_HID_PACKET_LEN);
	g_sent_count++;
}

static void clear_sent(void)
{
	g_sent_count = 0;
	memset(g_sent, 0, sizeof(g_sent));
}

static void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(0), OSK_OK);
	T_EQ("Transport startet", osk_hid_init(sink, NULL), OSK_OK);
	clear_sent();
}

static uint32_t cid_of(const uint8_t *pkt)
{
	return ((uint32_t)pkt[0] << 24) | ((uint32_t)pkt[1] << 16) | ((uint32_t)pkt[2] << 8) |
	       pkt[3];
}

static uint16_t bcnt_of(const uint8_t *pkt)
{
	return (uint16_t)(((uint16_t)pkt[5] << 8) | pkt[6]);
}

/* Schickt eine Nachricht als Paketkette hinein. */
static void send_in(uint32_t cid, uint8_t cmd, const uint8_t *data, size_t len)
{
	uint8_t pkt[OSK_HID_PACKET_LEN];
	size_t at = 0;

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
	osk_hid_rx_packet(pkt);
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
		osk_hid_rx_packet(pkt);
		at += take;
	}
}

/* Setzt eine Antwort aus den gesendeten Paketen wieder zusammen. */
static size_t collect(uint8_t cmd, uint8_t *out, size_t cap)
{
	size_t at = 0;
	size_t total = 0;
	bool started = false;

	for (size_t i = 0; i < g_sent_count && i < MAX_SENT; i++) {
		const uint8_t *p = g_sent[i];
		if (!started) {
			if (p[4] != cmd)
				continue;
			total = bcnt_of(p);
			started = true;
			const size_t take = (total < OSK_HID_INIT_DATA) ? total : OSK_HID_INIT_DATA;
			if (take > cap)
				return 0;
			memcpy(out, p + 7, take);
			at = take;
		} else {
			if ((p[4] & 0x80) != 0)
				break;
			const size_t left = total - at;
			const size_t take = (left < OSK_HID_CONT_DATA) ? left : OSK_HID_CONT_DATA;
			if (at + take > cap)
				return 0;
			memcpy(out + at, p + 5, take);
			at += take;
		}
		if (started && at >= total)
			break;
	}
	return at;
}

static uint32_t do_init(void)
{
	const uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};

	clear_sent();
	send_in(OSK_HID_BROADCAST_CID, CTAPHID_INIT, nonce, sizeof(nonce));
	T_EQ("ein Paket zurueck", (long long)g_sent_count, 1);
	if (g_sent_count != 1)
		return 0;
	T_EQ("Antwort auf dem Rundrufkanal", (long long)cid_of(g_sent[0]),
	     (long long)OSK_HID_BROADCAST_CID);
	T_EQ("Befehl INIT", g_sent[0][4], CTAPHID_INIT);
	T_EQ("17 Bytes", (long long)bcnt_of(g_sent[0]), 17);
	T_TRUE("Zufallszahl kommt zurueck", memcmp(g_sent[0] + 7, nonce, 8) == 0);
	T_EQ("Protokollfassung 2", g_sent[0][7 + 12], 2);
	T_TRUE("CBOR wird gekonnt", (g_sent[0][7 + 16] & OSK_HID_CAP_CBOR) != 0);

	const uint8_t *d = g_sent[0] + 7;
	return ((uint32_t)d[8] << 24) | ((uint32_t)d[9] << 16) | ((uint32_t)d[10] << 8) | d[11];
}

static void test_init(void)
{
	start();
	const uint32_t cid = do_init();
	T_TRUE("Kanal ist nicht 0", cid != 0);
	T_TRUE("Kanal ist nicht der Rundruf", cid != OSK_HID_BROADCAST_CID);

	/* Zwei Programme bekommen verschiedene Kanaele. */
	const uint32_t cid2 = do_init();
	T_TRUE("zweiter Kanal ist anders", cid2 != cid);

	/* INIT mit falscher Laenge */
	clear_sent();
	const uint8_t short_nonce[4] = {1, 2, 3, 4};
	send_in(OSK_HID_BROADCAST_CID, CTAPHID_INIT, short_nonce, sizeof(short_nonce));
	T_EQ("Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("ERROR", g_sent[0][4], CTAPHID_ERROR);
	T_EQ("ungueltige Laenge", g_sent[0][7], OSK_HID_ERR_INVALID_LEN);
}

static void test_ping(void)
{
	start();
	const uint32_t cid = do_init();

	/* Kurzes Ping passt in ein Paket. */
	const uint8_t data[8] = {9, 8, 7, 6, 5, 4, 3, 2};
	clear_sent();
	send_in(cid, CTAPHID_PING, data, sizeof(data));
	T_EQ("ein Paket", (long long)g_sent_count, 1);
	T_EQ("PING zurueck", g_sent[0][4], CTAPHID_PING);
	T_TRUE("gleicher Inhalt", memcmp(g_sent[0] + 7, data, sizeof(data)) == 0);

	/* Langes Ping braucht eine Kette - hin und zurueck. */
	uint8_t big[1000];
	for (size_t i = 0; i < sizeof(big); i++)
		big[i] = (uint8_t)(i * 7u);
	clear_sent();
	send_in(cid, CTAPHID_PING, big, sizeof(big));
	T_TRUE("mehrere Pakete zurueck", g_sent_count > 1);

	uint8_t back[1200];
	const size_t n = collect(CTAPHID_PING, back, sizeof(back));
	T_EQ("gleiche Laenge", (long long)n, (long long)sizeof(big));
	T_TRUE("gleicher Inhalt", n == sizeof(big) && memcmp(back, big, n) == 0);
}

static void test_cbor_ueber_hid(void)
{
	start();
	const uint32_t cid = do_init();

	/* GetInfo ueber den Transport. */
	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	clear_sent();
	send_in(cid, CTAPHID_CBOR, msg, sizeof(msg));

	/* Vor der Antwort darf ein Lebenszeichen kommen. */
	bool saw_keepalive = false;
	for (size_t i = 0; i < g_sent_count; i++) {
		if (g_sent[i][4] == CTAPHID_KEEPALIVE)
			saw_keepalive = true;
	}
	T_TRUE("Lebenszeichen gesendet", saw_keepalive);

	uint8_t back[1200];
	const size_t n = collect(CTAPHID_CBOR, back, sizeof(back));
	T_TRUE("Antwort da", n > 1);
	T_EQ("Statusbyte ok", back[0], CTAP2_OK);
	T_EQ("CBOR kanonisch", osk_cbor_validate(back + 1, n - 1), OSK_OK);

	/* CBOR ohne Befehlsbyte */
	clear_sent();
	send_in(cid, CTAPHID_CBOR, NULL, 0);
	T_EQ("Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("ERROR", g_sent[0][4], CTAPHID_ERROR);
	T_EQ("ungueltige Laenge", g_sent[0][7], OSK_HID_ERR_INVALID_LEN);
}

static void test_msg_ueber_hid(void)
{
	start();
	const uint32_t cid = do_init();

	/* U2F_VERSION als CTAPHID_MSG */
	const uint8_t apdu[7] = {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
	clear_sent();
	send_in(cid, CTAPHID_MSG, apdu, sizeof(apdu));

	uint8_t back[64];
	const size_t n = collect(CTAPHID_MSG, back, sizeof(back));
	T_EQ("acht Bytes", (long long)n, 8);
	T_TRUE("U2F_V2", n == 8 && memcmp(back, "U2F_V2", 6) == 0);
	T_EQ("SW1", back[6], 0x90);
	T_EQ("SW2", back[7], 0x00);
}

static void test_kanalfehler(void)
{
	start();
	const uint32_t cid = do_init();

	/* Kanal 0 ist reserviert. */
	clear_sent();
	send_in(0, CTAPHID_PING, NULL, 0);
	T_EQ("ein Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("ungueltiger Kanal", g_sent[0][7], OSK_HID_ERR_INVALID_CHANNEL);

	/* Unbekannter Kanal */
	clear_sent();
	send_in(0x12345678u, CTAPHID_PING, NULL, 0);
	T_EQ("ein Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("ungueltiger Kanal", g_sent[0][7], OSK_HID_ERR_INVALID_CHANNEL);

	/* Unbekannter Befehl */
	clear_sent();
	send_in(cid, 0xf0, NULL, 0);
	T_EQ("ein Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("ungueltiger Befehl", g_sent[0][7], OSK_HID_ERR_INVALID_CMD);

	/* Nachricht, die groesser ist als unser Puffer */
	clear_sent();
	uint8_t pkt[OSK_HID_PACKET_LEN];
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = CTAPHID_PING;
	pkt[5] = 0x1f;
	pkt[6] = 0xff; /* 8191 Bytes angekuendigt */
	osk_hid_rx_packet(pkt);
	T_EQ("ein Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("zu lang", g_sent[0][7], OSK_HID_ERR_INVALID_LEN);
}

static void test_reihenfolge(void)
{
	start();
	const uint32_t cid = do_init();
	uint8_t pkt[OSK_HID_PACKET_LEN];

	/* Startpaket mit 200 Bytes ankuendigen, dann ein Fortsetzungspaket mit
	 * falscher Nummer. */
	clear_sent();
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = CTAPHID_PING;
	pkt[5] = 0;
	pkt[6] = 200;
	osk_hid_rx_packet(pkt);
	T_EQ("noch keine Antwort", (long long)g_sent_count, 0);

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = 5; /* erwartet waere 0 */
	osk_hid_rx_packet(pkt);
	T_EQ("Fehlerpaket", (long long)g_sent_count, 1);
	T_EQ("falsche Reihenfolge", g_sent[0][7], OSK_HID_ERR_INVALID_SEQ);

	/* Ein Fortsetzungspaket ohne Startpaket wird stillschweigend verworfen. */
	clear_sent();
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = 0;
	osk_hid_rx_packet(pkt);
	T_EQ("keine Antwort", (long long)g_sent_count, 0);
}

static void test_belegt_und_zeitgrenze(void)
{
	start();
	const uint32_t cid1 = do_init();
	const uint32_t cid2 = do_init();
	uint8_t pkt[OSK_HID_PACKET_LEN];

	/* Kanal 1 faengt eine lange Nachricht an. */
	clear_sent();
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid1 >> 24);
	pkt[1] = (uint8_t)(cid1 >> 16);
	pkt[2] = (uint8_t)(cid1 >> 8);
	pkt[3] = (uint8_t)cid1;
	pkt[4] = CTAPHID_PING;
	pkt[5] = 0;
	pkt[6] = 200;
	osk_hid_rx_packet(pkt);

	/* Kanal 2 kommt dazwischen und wird abgewiesen. */
	send_in(cid2, CTAPHID_PING, NULL, 0);
	T_EQ("Fehlerpaket fuer den zweiten Kanal", (long long)g_sent_count, 1);
	T_EQ("belegt", g_sent[0][7], OSK_HID_ERR_CHANNEL_BUSY);
	T_EQ("auf dem zweiten Kanal gemeldet", (long long)cid_of(g_sent[0]), (long long)cid2);

	/* Die halbe Nachricht laeuft ab. */
	clear_sent();
	osk_hid_tick();
	T_EQ("noch nichts", (long long)g_sent_count, 0);
	mock_clock_advance(OSK_HID_TIMEOUT_MS + 1);
	osk_hid_tick();
	T_EQ("Zeitgrenze gemeldet", (long long)g_sent_count, 1);
	T_EQ("Zeitfehler", g_sent[0][7], OSK_HID_ERR_MSG_TIMEOUT);

	/* Danach ist der Transport wieder frei. */
	clear_sent();
	send_in(cid2, CTAPHID_PING, NULL, 0);
	T_EQ("Antwort", (long long)g_sent_count, 1);
	T_EQ("PING", g_sent[0][4], CTAPHID_PING);
}

static void test_init_raeumt_auf(void)
{
	start();
	const uint32_t cid = do_init();
	uint8_t pkt[OSK_HID_PACKET_LEN];

	/* Halbe Nachricht, dann INIT auf demselben Kanal. */
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(cid >> 24);
	pkt[1] = (uint8_t)(cid >> 16);
	pkt[2] = (uint8_t)(cid >> 8);
	pkt[3] = (uint8_t)cid;
	pkt[4] = CTAPHID_PING;
	pkt[5] = 0;
	pkt[6] = 200;
	osk_hid_rx_packet(pkt);

	clear_sent();
	const uint8_t nonce[8] = {9, 9, 9, 9, 9, 9, 9, 9};
	send_in(cid, CTAPHID_INIT, nonce, sizeof(nonce));
	T_EQ("INIT-Antwort", (long long)g_sent_count, 1);
	T_EQ("auf demselben Kanal", (long long)cid_of(g_sent[0]), (long long)cid);

	/* Und der Kanal ist wieder frei. */
	clear_sent();
	send_in(cid, CTAPHID_PING, nonce, sizeof(nonce));
	T_EQ("PING geht", (long long)g_sent_count, 1);
	T_EQ("PING", g_sent[0][4], CTAPHID_PING);
}

static void test_abbruch(void)
{
	start();
	const uint32_t cid = do_init();

	/* CANCEL ohne laufende Anfrage wird uebergangen - keine Antwort. */
	clear_sent();
	send_in(cid, CTAPHID_CANCEL, NULL, 0);
	T_EQ("keine Antwort auf CANCEL", (long long)g_sent_count, 0);

	/* WINK wird gekonnt. */
	clear_sent();
	send_in(cid, CTAPHID_WINK, NULL, 0);
	T_EQ("eine Antwort", (long long)g_sent_count, 1);
	T_EQ("WINK", g_sent[0][4], CTAPHID_WINK);

	/* LOCK koennen wir nicht und sagen das. */
	clear_sent();
	const uint8_t secs[1] = {5};
	send_in(cid, CTAPHID_LOCK, secs, 1);
	T_EQ("eine Antwort", (long long)g_sent_count, 1);
	T_EQ("ERROR", g_sent[0][4], CTAPHID_ERROR);
	T_EQ("Befehl gibt es nicht", g_sent[0][7], OSK_HID_ERR_INVALID_CMD);
}

int main(void)
{
	test_init();
	test_ping();
	test_cbor_ueber_hid();
	test_msg_ueber_hid();
	test_kanalfehler();
	test_reihenfolge();
	test_belegt_und_zeitgrenze();
	test_init_raeumt_auf();
	test_abbruch();
	return t_report("hid");
}
