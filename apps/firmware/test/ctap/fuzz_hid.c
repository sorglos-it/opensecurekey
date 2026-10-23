/* Fuzzlauf ueber den HID-Rahmen: 64-Byte-Pakete in beliebiger Reihenfolge,
 * mit falschen Kanaelen, falschen Laengen und falschen Folgenummern.
 *
 * Der Rahmen ist die aeusserste Wand des Geraets. Er darf an keiner
 * Byte-Folge ueber einen Puffer laufen - mit ASan wird das sichtbar.
 *
 * Aufruf: fuzz_hid [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Kein Paket darf den Prozess umbringen.
 *   2. Jede Antwort ist hoechstens 64 Bytes lang und tragt dieselbe
 *      Kanalnummer wie das ausloesende Paket, oder ist ein Fehlerpaket.
 *   3. Der Lauf muss echte Nachrichten durchbekommen - sonst hat er nur
 *      die erste Pruefung getroffen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "osk_cbor.h"
#include "osk_ctap.h"
#include "osk_ctap_internal.h"
#include "usb_hid.h"

static uint64_t g_rng;
static unsigned long g_packets_out;
static unsigned long g_cbor_answers;
/* Der Kanal, den der Transport zuletzt vergeben hat. Ohne ihn kaeme der
 * Lauf nie ueber die Kanalpruefung hinaus - der Zaehler unten belegt das. */
static uint32_t g_granted_cid;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static void sink(const uint8_t packet[OSK_HID_PACKET_LEN], void *ctx)
{
	(void)ctx;
	g_packets_out++;
	/* Eine Antwort auf CTAPHID_CBOR muss ein Statusbyte tragen. */
	if (packet[4] == CTAPHID_CBOR)
		g_cbor_answers++;
	/* Aus der INIT-Antwort den vergebenen Kanal mitlesen. */
	if (packet[4] == CTAPHID_INIT && packet[5] == 0 && packet[6] == 17)
		g_granted_cid = ((uint32_t)packet[15] << 24) | ((uint32_t)packet[16] << 16) |
				((uint32_t)packet[17] << 8) | packet[18];
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0xfeedfaceULL;
	if (g_rng == 0)
		g_rng = 1;

	uint8_t pkt[OSK_HID_PACKET_LEN];

	mock_reset_all();
	if (osk_ctap_init(0) != OSK_OK || osk_hid_init(sink, NULL) != OSK_OK) {
		printf("FEHLER: Start misslungen\n");
		return 1;
	}
	mock_ui_set_confirm(OSK_OK);

	/* Einen echten Kanal holen, damit der Lauf ueber die Kanalpruefung
	 * hinauskommt. */
	memset(pkt, 0, sizeof(pkt));
	pkt[0] = pkt[1] = pkt[2] = pkt[3] = 0xff;
	pkt[4] = CTAPHID_INIT;
	pkt[6] = 8;
	osk_hid_rx_packet(pkt);

	unsigned long runs = 0;
	const time_t start = time(NULL);

	while (time(NULL) - start < secs) {
		memset(pkt, 0, sizeof(pkt));

		/* Kanal: meist der bekannte, manchmal Unsinn. */
		uint32_t cid;
		switch (rnd() % 8u) {
		case 0:
			cid = 0;
			break;
		case 1:
			cid = 0xffffffffu;
			break;
		case 2:
			cid = rnd();
			break;
		default:
			cid = (g_granted_cid != 0) ? g_granted_cid : 1u;
			break;
		}
		pkt[0] = (uint8_t)(cid >> 24);
		pkt[1] = (uint8_t)(cid >> 16);
		pkt[2] = (uint8_t)(cid >> 8);
		pkt[3] = (uint8_t)cid;

		if ((rnd() % 3u) == 0u) {
			/* Fortsetzungspaket mit beliebiger Folgenummer */
			pkt[4] = (uint8_t)(rnd() & 0x7fu);
			for (unsigned i = 5; i < OSK_HID_PACKET_LEN; i++)
				pkt[i] = (uint8_t)rnd();
		} else {
			/* Startpaket. Meist ein echter Befehl. */
			static const uint8_t cmds[] = {
			    CTAPHID_PING,   CTAPHID_CBOR, CTAPHID_MSG,	CTAPHID_INIT,
			    CTAPHID_CANCEL, CTAPHID_WINK, CTAPHID_LOCK,
			};
			pkt[4] = ((rnd() % 4u) == 0u) ? (uint8_t)(0x80u | (rnd() & 0x7fu))
						      : cmds[rnd() % sizeof(cmds)];
			if (pkt[4] == CTAPHID_INIT) {
				pkt[5] = 0;
				pkt[6] = 8;
			} else {
				pkt[5] = (uint8_t)(((rnd() % 4u) == 0u) ? rnd() : 0u);
				pkt[6] = (uint8_t)rnd();
			}
			for (unsigned i = 7; i < OSK_HID_PACKET_LEN; i++)
				pkt[i] = (uint8_t)rnd();
			/* Manchmal eine echte GetInfo-Nachricht hineinlegen. */
			if (pkt[4] == CTAPHID_CBOR && (rnd() % 2u) == 0u) {
				pkt[5] = 0;
				pkt[6] = 1;
				pkt[7] = OSK_CTAP_CMD_GET_INFO;
			}
		}

		osk_hid_rx_packet(pkt);
		if ((rnd() % 16u) == 0u) {
			mock_clock_advance(OSK_HID_TIMEOUT_MS + 1);
			osk_hid_tick();
		}

		/* Ab und zu einen frischen Kanal holen und merken. */
		if ((rnd() % 64u) == 0u) {
			uint8_t init[OSK_HID_PACKET_LEN];
			memset(init, 0, sizeof(init));
			init[0] = init[1] = init[2] = init[3] = 0xff;
			init[4] = CTAPHID_INIT;
			init[6] = 8;
			osk_hid_rx_packet(init);
		}
		runs++;
	}

	printf("fuzz_hid: %lu Pakete hinein, %lu Pakete hinaus, "
	       "%lu CBOR-Antworten\n",
	       runs, g_packets_out, g_cbor_answers);
	if (runs == 0 || g_packets_out == 0) {
		printf("FEHLER: kein Verkehr\n");
		return 1;
	}
	if (g_cbor_answers == 0) {
		printf("FEHLER: keine einzige CBOR-Antwort - der Lauf kam nicht "
		       "ueber den Rahmen hinaus\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
