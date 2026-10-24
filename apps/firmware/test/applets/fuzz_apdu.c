/* Fuzzlauf ueber den APDU-Parser und die Kartenanwendungen.
 *
 * Alles, was ueber den Kartenleser hereinkommt, laeuft durch
 * osk_apdu_parse() und danach durch PIV oder OpenPGP. Das ist die aeusserste
 * Wand der Karte: sie darf an keiner Byte-Folge ueber einen Puffer laufen.
 * Mit ASan und UBSan wird das sichtbar.
 *
 * Aufruf: fuzz_apdu [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Kein Kommando darf den Prozess umbringen.
 *   2. Jede Antwort endet auf ein Statuswort und ist nie laenger als der
 *      Puffer, den der Aufrufer gegeben hat.
 *   3. Was der Parser annimmt, muss er auch wieder hergeben: Lc und Le
 *      duerfen nie ueber die Grenzen zeigen.
 *   4. Der Lauf muss echte Kommandos durchbekommen - sonst hat er nur die
 *      erste Pruefung getroffen. Die Zaehler am Ende belegen das. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "osk_apdu.h"
#include "osk_applets.h"
#include "osk_piv.h"
#include "osk_openpgp.h"

static uint64_t g_rng;
static unsigned long g_parsed;
static unsigned long g_answered_ok;
static unsigned long g_selected;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

/* Ein Kommando wuerfeln. Oft wird ein echter Kopf genommen, damit der Lauf
 * nicht nur an der ersten Pruefung haengen bleibt. */
static size_t make_command(uint8_t *buf, size_t cap)
{
	const uint32_t shape = rnd() % 10u;
	size_t len;

	if (shape < 3u) {
		/* Ganz zufaellig. */
		len = (size_t)(rnd() % 40u);
		if (len > cap)
			len = cap;
		for (size_t i = 0; i < len; i++)
			buf[i] = (uint8_t)rnd();
		return len;
	}

	/* Ein plausibler Kopf, dann gewuerfelte Laengen und Daten. */
	static const uint8_t ins[] = {0xa4, 0x20, 0x24, 0x2c, 0xca, 0xcb, 0xda, 0xdb,
				      0x47, 0x87, 0x2a, 0x88, 0xc0, 0xe6, 0x44};

	len = 0;
	buf[len++] = (uint8_t)((rnd() % 4u == 0u) ? (rnd() & 0xffu) : 0x00u);
	buf[len++] = ins[rnd() % (sizeof(ins) / sizeof(ins[0]))];
	buf[len++] = (uint8_t)rnd();
	buf[len++] = (uint8_t)rnd();

	if (rnd() % 3u != 0u) {
		const uint32_t pick = rnd() % 6u;
		size_t lc;

		switch (pick) {
		case 0:
			lc = 0u;
			break;
		case 1:
			lc = 1u + (size_t)(rnd() % 32u);
			break;
		case 2:
			lc = 255u;
			break;
		case 3:
			lc = 256u;
			break;
		case 4:
			lc = (size_t)(rnd() % 600u);
			break;
		default:
			lc = OSK_APDU_MAX_DATA + 1u;
			break;
		}
		if (lc < 256u && lc != 0u) {
			buf[len++] = (uint8_t)lc;
		} else {
			buf[len++] = 0x00u;
			buf[len++] = (uint8_t)(lc >> 8);
			buf[len++] = (uint8_t)lc;
		}
		/* Absichtlich oft NICHT so viele Daten anhaengen, wie Lc sagt. */
		{
			size_t have = lc;

			if (rnd() % 4u == 0u)
				have = (size_t)(rnd() % (lc + 2u));
			if (len + have > cap)
				have = cap - len;
			for (size_t i = 0; i < have; i++) {
				/* Halb Zufall, halb TLV-aehnliches Material. */
				if (rnd() % 3u == 0u)
					buf[len + i] =
					    (uint8_t)"\x5c\x53\x7c\x81\x82\x86\xac\xa6\x7f\x49"
						[rnd() % 10u];
				else
					buf[len + i] = (uint8_t)rnd();
			}
			len += have;
		}
	}
	if (rnd() % 2u == 0u && len < cap)
		buf[len++] = (uint8_t)rnd();
	return len;
}

/* Zwischendurch eine der beiden Anwendungen richtig auswaehlen - sonst
 * antwortet der Verteiler immer nur mit 6985. */
static void select_sometimes(void)
{
	uint8_t buf[32];
	uint8_t out[64];
	osk_request_t req;
	osk_response_t res;
	size_t at = 0;
	const bool piv = (rnd() % 2u) == 0u;
	const uint8_t *aid = piv ? osk_piv_aid : osk_openpgp_aid();
	const size_t aid_len = piv ? OSK_PIV_AID_LEN : 6u;

	buf[at++] = 0x00u;
	buf[at++] = 0xa4u;
	buf[at++] = 0x04u;
	buf[at++] = 0x00u;
	buf[at++] = (uint8_t)aid_len;
	memcpy(buf + at, aid, aid_len);
	at += aid_len;
	buf[at++] = 0x00u;

	req.link = OSK_LINK_USB_CCID;
	req.data = buf;
	req.len = at;
	res.data = out;
	res.cap = sizeof(out);
	res.len = 0u;
	if (osk_applets_handle(&req, &res) == OSK_OK && res.len >= 2u)
		g_selected++;
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	const time_t stop = time(NULL) + (secs > 0 ? secs : 1);
	uint8_t cmd[OSK_APDU_MAX_FRAME];
	uint8_t out[300];
	unsigned long rounds = 0;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0xc0ffee11ULL;
	if (g_rng == 0u)
		g_rng = 1u;

	mock_reset_all();
	if (osk_applets_init() != OSK_OK) {
		printf("FEHLER: Anwendungen starten nicht\n");
		return 1;
	}

	while (time(NULL) < stop) {
		osk_apdu_t apdu;
		osk_request_t req;
		osk_response_t res;
		size_t len;

		rounds++;
		if (rounds % 50u == 0u)
			select_sometimes();
		if (rounds % 500u == 0u) {
			/* Zwischendurch alles auf Anfang, damit auch der
			 * Zustand nach dem Einschalten getroffen wird. */
			osk_applets_reset();
			mock_reset_all();
			(void)osk_applets_init();
		}

		len = make_command(cmd, sizeof(cmd));

		/* Erst der Parser allein: was er annimmt, muss stimmen. */
		if (osk_apdu_parse(cmd, len, &apdu) == OSK_OK) {
			g_parsed++;
			if (apdu.lc > OSK_APDU_MAX_DATA) {
				printf("FEHLER: Lc ueber der Grenze angenommen\n");
				return 1;
			}
			if (apdu.lc != 0u && (apdu.data < cmd || apdu.data + apdu.lc > cmd + len)) {
				printf("FEHLER: Daten zeigen aus dem Puffer heraus\n");
				return 1;
			}
			if (apdu.has_le && apdu.le > OSK_APDU_MAX_RESP && apdu.le != 65536u) {
				printf("FEHLER: Le ueber der Grenze angenommen\n");
				return 1;
			}
		}

		/* Dann der ganze Weg durch die Anwendungen. */
		req.link = OSK_LINK_USB_CCID;
		req.data = cmd;
		req.len = len;
		res.data = out;
		res.cap = (size_t)2u + (size_t)(rnd() % (sizeof(out) - 2u));
		res.len = 0u;
		if (osk_applets_handle(&req, &res) != OSK_OK)
			continue; /* zu kurz oder kein Puffer - das darf er sagen */

		if (res.len < 2u) {
			printf("FEHLER: Antwort ohne Statuswort\n");
			return 1;
		}
		if (res.len > res.cap) {
			printf("FEHLER: Antwort laenger als der Puffer\n");
			return 1;
		}
		if (res.data[res.len - 2u] == 0x90u && res.data[res.len - 1u] == 0x00u)
			g_answered_ok++;
	}

	printf("fuzz_apdu: %lu Runden, %lu gueltige Kommandos, %lu Antworten 9000, "
	       "%lu Auswahlen\n",
	       rounds, g_parsed, g_answered_ok, g_selected);
	if (rounds < 1000u || g_parsed < 100u) {
		printf("FEHLER: der Lauf hat zu wenig erreicht\n");
		return 1;
	}
	if (g_answered_ok == 0u) {
		printf("FEHLER: kein einziges Kommando kam durch - der Lauf hat nur die "
		       "Abweisung geprueft\n");
		return 1;
	}
	printf("pruefung: ok\n");
	return 0;
}
