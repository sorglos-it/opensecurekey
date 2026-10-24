/* Fuzzlauf ueber den CCID-Rahmen: Bulk-Nachrichten in beliebiger Form, mit
 * falschen Laengen, falschen Schlitzen und abgeschnittenen Koepfen.
 *
 * Das ist die Schicht davor: noch vor der APDU muss der Rahmen stimmen.
 *
 * Aufruf: fuzz_ccid [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Keine Nachricht darf den Prozess umbringen.
 *   2. Jede Antwort hat einen vollstaendigen Kopf, dieselbe Folgenummer wie
 *      die Anfrage, und die Laenge im Kopf passt zur gelieferten Laenge.
 *   3. Eine Nachricht ohne vollstaendigen Kopf bleibt unbeantwortet.
 *   4. Der Lauf muss echte Nachrichten durchbekommen - die Zaehler belegen es. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "usb_ccid.h"
#include "osk_piv.h"

static uint64_t g_rng;
static unsigned long g_answers;
static unsigned long g_data_blocks;
static uint8_t g_expect_seq;
static int g_bad;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static void sink(const uint8_t *msg, size_t len, void *ctx)
{
	(void)ctx;
	g_answers++;
	if (len < OSK_CCID_HEADER_LEN) {
		printf("FEHLER: Antwort ohne vollstaendigen Kopf (%zu Bytes)\n", len);
		g_bad = 1;
		return;
	}
	{
		const uint32_t announced = (uint32_t)msg[1] | ((uint32_t)msg[2] << 8) |
					   ((uint32_t)msg[3] << 16) | ((uint32_t)msg[4] << 24);

		if (announced != len - OSK_CCID_HEADER_LEN) {
			printf("FEHLER: angekuendigte Laenge passt nicht (%u gegen %zu)\n",
			       announced, len - OSK_CCID_HEADER_LEN);
			g_bad = 1;
		}
	}
	if (msg[6] != g_expect_seq) {
		printf("FEHLER: fremde Folgenummer in der Antwort\n");
		g_bad = 1;
	}
	if (msg[0] == OSK_CCID_RDR_DATA_BLOCK)
		g_data_blocks++;
}

/* Eine Nachricht wuerfeln. Jede dritte traegt einen echten Nachrichtentyp
 * und eine passende Laenge, damit der Lauf auch durchkommt. */
static size_t make_message(uint8_t *buf, size_t cap)
{
	static const uint8_t types[] = {
	    OSK_CCID_PC_SET_PARAMETERS,
	    OSK_CCID_PC_ICC_POWER_ON,
	    OSK_CCID_PC_ICC_POWER_OFF,
	    OSK_CCID_PC_GET_SLOT_STATUS,
	    OSK_CCID_PC_SECURE,
	    OSK_CCID_PC_T0_APDU,
	    OSK_CCID_PC_ESCAPE,
	    OSK_CCID_PC_GET_PARAMETERS,
	    OSK_CCID_PC_RESET_PARAMETERS,
	    OSK_CCID_PC_ICC_CLOCK,
	    OSK_CCID_PC_XFR_BLOCK,
	    OSK_CCID_PC_MECHANICAL,
	    OSK_CCID_PC_ABORT,
	    OSK_CCID_PC_SET_RATE_AND_CLOCK,
	};
	size_t data_len;
	size_t total;

	memset(buf, 0, OSK_CCID_HEADER_LEN);
	buf[0] =
	    (rnd() % 4u == 0u) ? (uint8_t)rnd() : types[rnd() % (sizeof(types) / sizeof(types[0]))];
	buf[5] = (rnd() % 8u == 0u) ? (uint8_t)rnd() : 0x00u;
	g_expect_seq = (uint8_t)rnd();
	buf[6] = g_expect_seq;

	data_len = (size_t)(rnd() % 40u);
	if (rnd() % 3u == 0u)
		data_len = (size_t)(rnd() % (OSK_CCID_MAX_DATA + 20u));
	if (OSK_CCID_HEADER_LEN + data_len > cap)
		data_len = cap - OSK_CCID_HEADER_LEN;

	/* Oft eine APDU hineinlegen, oft Unsinn. */
	if (rnd() % 2u == 0u && data_len >= 4u) {
		buf[OSK_CCID_HEADER_LEN] = 0x00u;
		buf[OSK_CCID_HEADER_LEN + 1u] = (uint8_t)((rnd() % 2u) ? 0xcau : 0xa4u);
		buf[OSK_CCID_HEADER_LEN + 2u] = (uint8_t)rnd();
		buf[OSK_CCID_HEADER_LEN + 3u] = (uint8_t)rnd();
		for (size_t i = 4u; i < data_len; i++)
			buf[OSK_CCID_HEADER_LEN + i] = (uint8_t)rnd();
	} else {
		for (size_t i = 0; i < data_len; i++)
			buf[OSK_CCID_HEADER_LEN + i] = (uint8_t)rnd();
	}

	/* Die angekuendigte Laenge stimmt meistens - und manchmal nicht. */
	{
		uint32_t announced = (uint32_t)data_len;

		if (rnd() % 5u == 0u)
			announced = rnd();
		buf[1] = (uint8_t)announced;
		buf[2] = (uint8_t)(announced >> 8);
		buf[3] = (uint8_t)(announced >> 16);
		buf[4] = (uint8_t)(announced >> 24);
	}

	total = OSK_CCID_HEADER_LEN + data_len;
	/* Jede zehnte Nachricht wird im Kopf abgeschnitten: darauf darf keine
	 * Antwort kommen. */
	if (rnd() % 10u == 0u)
		total = (size_t)(rnd() % OSK_CCID_HEADER_LEN);
	return total;
}

/* Eine richtige SELECT-APDU durch den Rahmen schicken. */
static void real_select(void)
{
	uint8_t msg[64];
	size_t at = OSK_CCID_HEADER_LEN;

	memset(msg, 0, sizeof(msg));
	msg[0] = OSK_CCID_PC_XFR_BLOCK;
	g_expect_seq = 0x7au;
	msg[6] = g_expect_seq;
	msg[at++] = 0x00u;
	msg[at++] = 0xa4u;
	msg[at++] = 0x04u;
	msg[at++] = 0x00u;
	msg[at++] = (uint8_t)OSK_PIV_AID_LEN;
	memcpy(msg + at, osk_piv_aid, OSK_PIV_AID_LEN);
	at += OSK_PIV_AID_LEN;
	msg[at++] = 0x00u;
	msg[1] = (uint8_t)(at - OSK_CCID_HEADER_LEN);
	osk_ccid_rx(msg, at);
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	const time_t stop = time(NULL) + (secs > 0 ? secs : 1);
	uint8_t msg[OSK_CCID_MAX_MSG + 32u];
	unsigned long rounds = 0;
	unsigned long short_heads = 0;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0xbeef0007ULL;
	if (g_rng == 0u)
		g_rng = 1u;

	mock_reset_all();
	if (osk_ccid_init(sink, NULL) != OSK_OK) {
		printf("FEHLER: Leser startet nicht\n");
		return 1;
	}

	while (time(NULL) < stop && !g_bad) {
		const size_t len = make_message(msg, sizeof(msg));
		const unsigned long before = g_answers;

		rounds++;
		osk_ccid_rx(msg, len);
		if (len < OSK_CCID_HEADER_LEN) {
			short_heads++;
			if (g_answers != before) {
				printf("FEHLER: halber Kopf wurde beantwortet\n");
				return 1;
			}
		}
		if (rounds % 100u == 0u)
			real_select();
	}

	if (g_bad)
		return 1;
	printf("fuzz_ccid: %lu Runden, %lu Antworten, %lu Datenbloecke, "
	       "%lu halbe Koepfe\n",
	       rounds, g_answers, g_data_blocks, short_heads);
	if (rounds < 1000u || g_answers < 100u) {
		printf("FEHLER: der Lauf hat zu wenig erreicht\n");
		return 1;
	}
	if (g_data_blocks == 0u || short_heads == 0u) {
		printf("FEHLER: ein Weg wurde nie getroffen\n");
		return 1;
	}
	printf("pruefung: ok\n");
	return 0;
}
