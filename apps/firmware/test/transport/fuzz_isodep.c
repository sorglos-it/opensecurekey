/* Fuzzlauf ueber den ISO-DEP-Rahmen und die APDU-Zerlegung.
 *
 * Das ist die aeusserste Wand des Geraets gegen eine Antenne. Sie darf an
 * keiner Byte-Folge ueber einen Puffer laufen - mit ASan wird das sichtbar.
 *
 * Aufruf: fuzz_isodep [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Kein Rahmen darf den Prozess umbringen.
 *   2. Jeder hinausgehende Rahmen ist hoechstens FSC lang und traegt, wenn
 *      das Lesegeraet eine Kennung benutzt hat, dieselbe Kennung.
 *   3. Der Lauf muss echte APDU durchbekommen - sonst hat er nur die erste
 *      Pruefung getroffen. Der Zaehler am Ende belegt das.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "nfc_tag.h"
#include "nfc_ctap.h"
#include "nfc_ndef.h"
#include "nfc_apdu.h"
#include "osk_transport.h"

/* ---- gestellter Kern ---------------------------------------------------- */

osk_result_t osk_ctap_init(uint32_t extensions);
osk_result_t osk_ctap_cred_count(size_t *count_out);
static unsigned long g_core_calls;

osk_result_t osk_ctap_init(uint32_t extensions)
{
	(void)extensions;
	return OSK_OK;
}

osk_result_t osk_ctap_cred_count(size_t *count_out)
{
	if (count_out)
		*count_out = 0;
	return OSK_OK;
}

osk_result_t osk_ctap_handle(const osk_request_t *req, osk_response_t *res)
{
	/* Eine Antwort wechselnder Laenge, damit die Kettenbildung der
	 * Antwort auch getroffen wird. */
	const size_t want = 1u + (req->len * 37u) % 600u;

	g_core_calls++;
	if (want > res->cap)
		return OSK_ERR_INTERNAL;
	for (size_t i = 0; i < want; i++)
		res->data[i] = (uint8_t)(i & 0xffu);
	res->len = want;
	return OSK_OK;
}

osk_result_t osk_ctap_handle_u2f(const osk_request_t *req, osk_response_t *res)
{
	static const uint8_t version[] = {'U', '2', 'F', '_', 'V', '2', 0x90, 0x00};

	(void)req;
	g_core_calls++;
	if (res->cap < sizeof(version))
		return OSK_ERR_INTERNAL;
	memcpy(res->data, version, sizeof(version));
	res->len = sizeof(version);
	return OSK_OK;
}

/* ---- Lauf --------------------------------------------------------------- */

static uint64_t g_rng;
static unsigned long g_frames_out;
static unsigned long g_ok_answers;
static unsigned long g_too_long;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static void sink(const uint8_t *frame, size_t len, void *ctx)
{
	(void)ctx;
	g_frames_out++;
	if (len == 0 || len > OSK_ISODEP_FRAME_MAX) {
		g_too_long++;
		return;
	}
	/* Ein Statuswort 9000 am Ende eines nicht gekettenen I-Blocks heisst:
	 * der Lauf ist bis an die Anwendung gekommen. */
	if ((frame[0] & 0xc0u) == 0x00u && (frame[0] & OSK_ISODEP_CHAINING) == 0 && len >= 3 &&
	    frame[len - 2] == 0x90 && frame[len - 1] == 0x00)
		g_ok_answers++;
}

/* Die Auswahl des FIDO-Applets - ohne sie kaeme der Lauf nie ueber die
 * Weiche hinaus. */
static void select_fido(uint8_t bn)
{
	const uint8_t aid[] = OSK_NFC_FIDO_AID;
	uint8_t frame[32];
	size_t at = 0;

	frame[at++] = (uint8_t)(OSK_ISODEP_I_BLOCK | (bn & 1u));
	frame[at++] = 0x00;
	frame[at++] = 0xa4;
	frame[at++] = 0x04;
	frame[at++] = 0x00;
	frame[at++] = (uint8_t)sizeof(aid);
	memcpy(frame + at, aid, sizeof(aid));
	at += sizeof(aid);
	frame[at++] = 0x00;
	osk_nfc_tag_rx_frame(frame, at);
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x0f1ce11aULL;
	if (g_rng == 0)
		g_rng = 1;

	mock_reset_all();
	mock_clock_set(1000);
	mock_ui_set_confirm(OSK_OK);
	if (osk_nfc_tag_init(sink, NULL) != OSK_OK) {
		printf("FEHLER: Start misslungen\n");
		return 1;
	}

	/* Damit auch die NDEF-Anwendung erreichbar ist. */
	osk_ndef_config_t cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	cfg.uri_prefix = OSK_NDEF_URI_HTTPS;
	cfg.uri_without_button = true;
	memcpy(cfg.uri, "sorglos.org", 11);
	if (osk_ndef_configure(&cfg) != OSK_OK) {
		printf("FEHLER: NDEF nicht einstellbar\n");
		return 1;
	}

	osk_nfc_tag_field_on();
	select_fido(0);

	uint8_t frame[OSK_ISODEP_FRAME_MAX + 8];
	unsigned long runs = 0;
	uint8_t bn = 1;
	const time_t start = time(NULL);

	while (time(NULL) - start < secs) {
		size_t len;

		switch (rnd() % 10u) {
		case 0:
			/* Ein ganz zufaelliger Rahmen beliebiger Laenge, auch
			 * laenger als FSC. */
			len = (size_t)(rnd() % (OSK_ISODEP_FRAME_MAX + 8u));
			for (size_t i = 0; i < len; i++)
				frame[i] = (uint8_t)rnd();
			break;
		case 1:
			/* Ein R- oder S-Block. */
			len = 1 + (size_t)(rnd() % 3u);
			frame[0] =
			    (uint8_t)((rnd() % 2u) ? 0xa0u : 0xc0u) | (uint8_t)(rnd() & 0x3fu);
			for (size_t i = 1; i < len; i++)
				frame[i] = (uint8_t)rnd();
			break;
		case 2:
			/* Die Auswahl irgendeiner Anwendung. */
			len = 5 + (size_t)(rnd() % 10u);
			frame[0] = (uint8_t)(OSK_ISODEP_I_BLOCK | (bn & 1u));
			frame[1] = 0x00;
			frame[2] = 0xa4;
			frame[3] = 0x04;
			frame[4] = 0x00;
			frame[5] = (uint8_t)(len - 6u);
			for (size_t i = 6; i < len; i++)
				frame[i] = (uint8_t)rnd();
			break;
		default: {
			/* Ein I-Block mit einer halb glaubwuerdigen APDU: der
			 * Kopf stimmt, die Laengenfelder luegen oft. */
			static const uint8_t cla[] = {0x00, 0x80, 0x90, 0x10, 0x40};
			static const uint8_t ins[] = {0xa4, 0xb0, 0xd6, 0xc0, 0x10,
						      0x11, 0x12, 0x01, 0x02, 0x03};
			size_t at = 0;

			frame[at++] = (uint8_t)(OSK_ISODEP_I_BLOCK | (bn & 1u) |
						((rnd() % 8u) == 0u ? OSK_ISODEP_CHAINING : 0u));
			frame[at++] = cla[rnd() % sizeof(cla)];
			frame[at++] = ins[rnd() % sizeof(ins)];
			frame[at++] = (uint8_t)((rnd() % 4u) ? 0x00u : rnd());
			frame[at++] = (uint8_t)((rnd() % 4u) ? 0x00u : rnd());
			const size_t body = (size_t)(rnd() % 40u);

			if ((rnd() % 4u) == 0u) {
				/* erweiterte Laengen */
				frame[at++] = 0x00;
				frame[at++] = (uint8_t)(rnd() & 0x01u);
				frame[at++] = (uint8_t)body;
			} else {
				frame[at++] = (uint8_t)((rnd() % 8u) ? body : rnd());
			}
			for (size_t i = 0; i < body && at < OSK_ISODEP_FRAME_MAX; i++)
				frame[at++] = (uint8_t)rnd();
			if ((rnd() % 2u) == 0u && at < OSK_ISODEP_FRAME_MAX)
				frame[at++] = (uint8_t)((rnd() % 2u) ? 0x00u : rnd());
			len = at;
			break;
		}
		}

		osk_nfc_tag_rx_frame(frame, len);
		bn ^= 1u;

		if ((rnd() % 32u) == 0u) {
			mock_clock_advance(OSK_ISODEP_TIMEOUT_MS + 1u);
			osk_nfc_tag_tick();
		}
		if ((rnd() % 128u) == 0u) {
			osk_nfc_tag_field_off();
			osk_nfc_tag_field_on();
			bn = 0;
			select_fido(0);
			bn = 1;
		}
		if ((rnd() % 64u) == 0u)
			select_fido(bn);
		runs++;
	}

	printf("fuzz_isodep: %lu Rahmen hinein, %lu hinaus, %lu Aufrufe des Kerns, "
	       "%lu Antworten mit 9000\n",
	       runs, g_frames_out, g_core_calls, g_ok_answers);
	if (runs == 0 || g_frames_out == 0) {
		printf("FEHLER: kein Verkehr\n");
		return 1;
	}
	if (g_too_long != 0) {
		printf("FEHLER: %lu Rahmen waren zu lang oder leer\n", g_too_long);
		return 1;
	}
	if (g_core_calls == 0) {
		printf("FEHLER: der Kern wurde nie erreicht - der Lauf kam nicht "
		       "ueber den Rahmen hinaus\n");
		return 1;
	}
	if (g_ok_answers == 0) {
		printf("FEHLER: keine einzige Antwort mit 9000\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
