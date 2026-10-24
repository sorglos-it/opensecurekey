/* Fuzzlauf ueber den NDEF-Parser und die NDEF-Anwendung.
 *
 * Der NDEF-Parser ist der Teil, den ein Angreifer als Erstes nimmt: er
 * liest geschachtelte Laengenfelder, und jedes einzelne kann luegen.
 *
 * Aufruf: fuzz_ndef [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Keine Nachricht darf den Prozess umbringen.
 *   2. Jeder gelesene Datensatz liegt VOLLSTAENDIG innerhalb der Nachricht -
 *      das wird fuer Typ, Kennung und Nutzlast einzeln nachgerechnet.
 *   3. Die Summe der gelesenen Teile ist nie groesser als die Nachricht.
 *   4. Der Lauf muss echte Nachrichten durchbekommen - sonst hat er nur
 *      die erste Pruefung getroffen.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "nfc_ndef.h"
#include "nfc_apdu.h"

static uint64_t g_rng;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

/* Prueft, dass jeder Zeiger in die Nachricht zeigt und jede Laenge darin
 * endet. Liefert false, wenn der Parser etwas ausserhalb gemeldet hat. */
static bool inside(const uint8_t *msg, size_t len, const osk_ndef_record_t *rec, size_t count)
{
	size_t sum = 0;

	for (size_t i = 0; i < count; i++) {
		const struct {
			const uint8_t *p;
			size_t n;
		} parts[3] = {
		    {rec[i].type, rec[i].type_len},
		    {rec[i].id, rec[i].id_len},
		    {rec[i].payload, rec[i].payload_len},
		};

		for (unsigned k = 0; k < 3; k++) {
			if (parts[k].n == 0) {
				/* Laenge 0 darf einen Zeiger haben oder nicht. */
				continue;
			}
			if (parts[k].p == NULL)
				return false;
			if (parts[k].p < msg || parts[k].p > msg + len)
				return false;
			if ((size_t)(parts[k].p - msg) + parts[k].n > len)
				return false;
			sum += parts[k].n;
		}
		/* Jeder Datensatz traegt mindestens zwei Byte Kopf. */
		sum += 2;
	}
	return sum <= len;
}

/* Baut eine Nachricht, die oft fast richtig ist. */
static size_t build(uint8_t *out, size_t cap)
{
	const unsigned kind = rnd() % 4u;
	size_t at = 0;

	if (kind == 0) {
		/* ganz zufaellig */
		const size_t n = (size_t)(rnd() % cap);

		for (size_t i = 0; i < n; i++)
			out[i] = (uint8_t)rnd();
		return n;
	}

	const unsigned records = 1u + rnd() % 4u;

	for (unsigned r = 0; r < records && at + 16 < cap; r++) {
		const bool first = (r == 0);
		const bool last = (r + 1u == records);
		uint8_t flags = (uint8_t)(rnd() % 7u); /* TNF, manchmal 0..6 */

		if ((rnd() % 16u) == 0u)
			flags = 0x07u; /* nicht vergeben */
		if (first)
			flags |= 0x80u;
		if (last)
			flags |= 0x40u;
		if ((rnd() % 16u) == 0u)
			flags |= 0x20u; /* geteilt */
		const bool short_len = (rnd() % 4u) != 0u;

		if (short_len)
			flags |= 0x10u;
		const bool with_id = (rnd() % 3u) == 0u;

		if (with_id)
			flags |= 0x08u;

		const size_t type_len = (rnd() % 3u);
		const size_t id_len = with_id ? (rnd() % 4u) : 0u;
		size_t payload_len = (rnd() % 20u);
		/* In jedem vierten Fall luegt die Laenge. */
		const size_t written = ((rnd() % 4u) == 0u) ? (payload_len / 2u) : payload_len;

		out[at++] = flags;
		out[at++] = (uint8_t)type_len;
		if (short_len) {
			out[at++] = (uint8_t)payload_len;
		} else {
			out[at++] = (uint8_t)(rnd() % 2u);
			out[at++] = 0;
			out[at++] = 0;
			out[at++] = (uint8_t)payload_len;
		}
		if (with_id)
			out[at++] = (uint8_t)id_len;
		for (size_t i = 0; i < type_len && at < cap; i++)
			out[at++] = (uint8_t)(rnd() % 2u ? 'T' : 'U');
		for (size_t i = 0; i < id_len && at < cap; i++)
			out[at++] = (uint8_t)rnd();
		for (size_t i = 0; i < written && at < cap; i++)
			out[at++] = (uint8_t)rnd();
	}
	return at;
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x0dfe1234ULL;
	if (g_rng == 0)
		g_rng = 1;

	mock_reset_all();
	mock_ui_set_confirm(OSK_OK);

	uint8_t msg[OSK_NDEF_FILE_MAX];
	uint8_t out[OSK_NDEF_FILE_MAX + 8];
	osk_ndef_record_t rec[OSK_NDEF_MAX_RECORDS];
	unsigned long runs = 0;
	unsigned long parsed = 0;
	unsigned long records = 0;
	unsigned long answers = 0;
	const time_t start = time(NULL);

	/* Die Anwendung soll antworten, damit auch sie beschossen wird. */
	osk_ndef_config_t cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	cfg.uri_prefix = OSK_NDEF_URI_HTTPS;
	cfg.uri_without_button = true;
	memcpy(cfg.uri, "sorglos.org", 11);
	if (osk_ndef_configure(&cfg) != OSK_OK) {
		printf("FEHLER: nicht einstellbar\n");
		return 1;
	}

	while (time(NULL) - start < secs) {
		const size_t len = build(msg, sizeof(msg));
		size_t count = 99;

		if (osk_ndef_parse(msg, len, rec, OSK_NDEF_MAX_RECORDS, &count)) {
			parsed++;
			records += count;
			if (count > OSK_NDEF_MAX_RECORDS) {
				printf("FEHLER: %zu Datensaetze gemeldet\n", count);
				return 1;
			}
			if (!inside(msg, len, rec, count)) {
				printf("FEHLER: ein Datensatz liegt ausserhalb der "
				       "Nachricht (Laenge %zu)\n",
				       len);
				return 1;
			}
		} else if (count != 0) {
			printf("FEHLER: Fehlschlag meldet %zu Datensaetze\n", count);
			return 1;
		}

		/* Dieselben Bytes als APDU an die Anwendung. */
		const size_t n = osk_ndef_apdu(msg, len, out, sizeof(out));

		if (n > sizeof(out)) {
			printf("FEHLER: Antwort zu lang\n");
			return 1;
		}
		if (n >= 2)
			answers++;
		runs++;
	}

	printf("fuzz_ndef: %lu Nachrichten, %lu gelesen, %lu Datensaetze, "
	       "%lu Antworten der Anwendung\n",
	       runs, parsed, records, answers);
	if (runs == 0) {
		printf("FEHLER: kein Lauf\n");
		return 1;
	}
	if (parsed == 0 || records == 0) {
		printf("FEHLER: keine einzige Nachricht wurde gelesen - der Lauf "
		       "hat nur die erste Pruefung getroffen\n");
		return 1;
	}
	if (answers == 0) {
		printf("FEHLER: die Anwendung hat nie geantwortet\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
