/* Fuzzlauf ueber den SCSI-Parser des Laufwerks.
 *
 * Warum gerade hier: diese Schnittstelle spricht jeder Rechner an, an dem der
 * Stick steckt, und zwar sofort und ohne dass ein Mensch zustimmt. Sie ist
 * damit die Stelle mit der groessten Angriffsflaeche und der kleinsten
 * Huerde - naeher kommt niemand an die Firmware heran, ohne etwas zu tun.
 *
 * Aufruf: fuzz_scsi [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Kein Befehl darf den Prozess umbringen.
 *   2. Auf einen gueltigen CBW folgt immer genau ein CSW - nie mehr, nie
 *      weniger, und das Kennzeichen darin ist das des Befehls.
 *   3. Es gehen nie mehr Daten hinaus, als der Rechner angefordert hat.
 *   4. Der Rueckstand im CSW passt zur Menge der gesendeten Daten.
 *   5. **Nach dem ganzen Lauf ist kein Byte des Laufwerks veraendert.**
 *   6. Der Lauf muss echte Befehle durchbekommen und echte Abweisungen
 *      erzeugt haben - sonst hat er nur die erste Wand getroffen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"

#define DRIVE_BLOCKS 64u

static uint64_t g_rng;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

/* Die Befehlscodes, die im Geraet wirklich vorkommen. Rein zufaellige Bytes
 * treffen fast immer "unbekannt" und pruefen dann nur eine Zeile. */
static const uint8_t KNOWN_OPS[] = {
    0x00, 0x03, 0x04, 0x08, 0x0a, 0x12, 0x15, 0x1a, 0x1b, 0x1e, 0x23, 0x25, 0x28, 0x2a, 0x2e, 0x2f,
    0x35, 0x3b, 0x41, 0x42, 0x48, 0x55, 0x5a, 0x88, 0x89, 0x8a, 0x93, 0x9e, 0xa8, 0xaa, 0xae};

/* Baut einen CBW, der oft fast richtig ist. */
static size_t build(uint8_t *out, size_t cap, uint32_t *tag_out, uint32_t *host_len_out,
		    bool *valid_out)
{
	const unsigned kind = rnd() % 8u;

	if (kind == 0u) {
		/* Ganz zufaellig und manchmal mit krummer Laenge: das ist der
		 * Fall, in dem nicht einmal der Rahmen stimmt. */
		const size_t n = (size_t)(rnd() % (cap + 1u));

		for (size_t i = 0; i < n; i++)
			out[i] = (uint8_t)rnd();
		*valid_out = false;
		*tag_out = 0;
		*host_len_out = 0;
		return n;
	}

	const uint32_t tag = rnd();
	uint32_t host_len;

	switch (rnd() % 4u) {
	case 0:
		host_len = 0;
		break;
	case 1:
		host_len = (rnd() % 4u) * OSK_MSC_BLOCK_LEN;
		break;
	case 2:
		host_len = rnd() % 600u;
		break;
	default:
		/* Unsinnig gross, aber begrenzt: der Lauf soll die Pruefungen
		 * treffen und nicht Stunden damit verbringen, eine erfundene
		 * Datenphase durchzuschieben. */
		host_len = rnd() % 65536u;
		break;
	}

	memset(out, 0, OSK_MSC_CBW_LEN);
	out[0] = (uint8_t)OSK_MSC_CBW_SIG;
	out[1] = (uint8_t)(OSK_MSC_CBW_SIG >> 8);
	out[2] = (uint8_t)(OSK_MSC_CBW_SIG >> 16);
	out[3] = (uint8_t)(OSK_MSC_CBW_SIG >> 24);
	/* In jedem achten Fall ist die Kennung kaputt. */
	if ((rnd() % 8u) == 0u) {
		out[rnd() % 4u] ^= 0xffu;
		*valid_out = false;
	} else {
		*valid_out = true;
	}
	out[4] = (uint8_t)tag;
	out[5] = (uint8_t)(tag >> 8);
	out[6] = (uint8_t)(tag >> 16);
	out[7] = (uint8_t)(tag >> 24);
	out[8] = (uint8_t)host_len;
	out[9] = (uint8_t)(host_len >> 8);
	out[10] = (uint8_t)(host_len >> 16);
	out[11] = (uint8_t)(host_len >> 24);
	out[12] = ((rnd() % 4u) == 0u) ? 0x00u : 0x80u;
	out[13] = ((rnd() % 16u) == 0u) ? (uint8_t)(rnd() % 16u) : 0u;
	/* Die Laengenangabe des Befehlsblocks luegt oft - das ist das Feld,
	 * das der Parser vor jedem Zugriff pruefen muss. */
	out[14] = ((rnd() % 8u) == 0u) ? (uint8_t)(rnd() % 32u) : (uint8_t)(1u + rnd() % 16u);

	uint8_t op = (uint8_t)rnd();

	if ((rnd() % 4u) != 0u)
		op = KNOWN_OPS[rnd() % (sizeof(KNOWN_OPS) / sizeof(KNOWN_OPS[0]))];
	out[15] = op;
	for (unsigned i = 16; i < OSK_MSC_CBW_LEN; i++)
		out[i] = (uint8_t)rnd();
	/* Oft eine Adresse, die wirklich auf dem Laufwerk liegt. */
	if ((rnd() % 2u) == 0u) {
		out[17] = 0;
		out[18] = 0;
		out[19] = 0;
		out[20] = (uint8_t)(rnd() % DRIVE_BLOCKS);
	}

	*tag_out = tag;
	*host_len_out = host_len;
	return OSK_MSC_CBW_LEN;
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	osk_msc_config_t cfg;
	uint8_t cbw[OSK_MSC_CBW_LEN];
	uint8_t image_before[DRIVE_BLOCKS][OSK_MSC_BLOCK_LEN];
	uint8_t block[OSK_MSC_BLOCK_LEN];

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x05c51234ULL;
	if (g_rng == 0)
		g_rng = 1;

	mock_transport_reset();
	fake_drive_init(DRIVE_BLOCKS);
	memset(&cfg, 0, sizeof(cfg));
	cfg.read = fake_drive_read;
	cfg.send = mock_sink_send;
	cfg.blocks = DRIVE_BLOCKS;
	if (osk_msc_init(&cfg) != OSK_OK) {
		printf("FEHLER: Laufwerk laesst sich nicht einrichten\n");
		return 1;
	}

	/* Der Zustand vor dem Beschuss. */
	for (uint32_t b = 0; b < DRIVE_BLOCKS; b++) {
		if (fake_drive_read(b, image_before[b], NULL) != OSK_OK) {
			printf("FEHLER: Laufwerk schon vor dem Lauf nicht lesbar\n");
			return 1;
		}
	}

	unsigned long runs = 0;
	unsigned long answered = 0;
	unsigned long passed = 0;
	unsigned long refused = 0;
	unsigned long data_bytes = 0;
	unsigned long discarded = 0;
	const time_t start = time(NULL);

	while (time(NULL) - start < secs) {
		uint32_t tag = 0;
		uint32_t host_len = 0;
		bool valid = false;
		const size_t len = build(cbw, sizeof(cbw), &tag, &host_len, &valid);

		/* Nach einem stehengebliebenen Endpunkt antwortet das Laufwerk
		 * bis zum Zuruecksetzen auf nichts mehr - das ist gewollt und
		 * wuerde den Lauf sonst nach dem ersten kaputten CBW beenden. */
		osk_msc_reset();
		mock_sink_reset();
		osk_msc_rx(cbw, len);

		/* Ein abgewiesener Schreibbefehl mit Datenphase wartet, bis die
		 * angekuendigten Daten durch sind - genau wie am echten Bus.
		 * Also schicken wir sie, so wie ein Rechner es taete. */
		if (valid && host_len > 0u && mock_sink_len() == 0u) {
			uint8_t chunk[OSK_MSC_BLOCK_LEN];
			uint32_t left = host_len;

			memset(chunk, 0xc3, sizeof(chunk));
			while (left > 0u && mock_sink_len() == 0u) {
				const uint32_t take =
				    (left > sizeof(chunk)) ? (uint32_t)sizeof(chunk) : left;

				osk_msc_rx(chunk, take);
				left -= take;
			}
			discarded++;
		}

		const size_t sent = mock_sink_len();

		if (!valid) {
			/* Auf einen unverstandenen Rahmen wird nicht geantwortet. */
			if (sent != 0u) {
				printf("FEHLER: auf einen kaputten CBW wurde geantwortet "
				       "(%zu Byte)\n",
				       sent);
				return 1;
			}
			runs++;
			continue;
		}

		uint32_t got_tag = 0;
		uint32_t residue = 0;
		uint8_t status = 0;

		if (!mock_sink_csw(&got_tag, &residue, &status)) {
			printf("FEHLER: kein CSW nach einem gueltigen CBW (Befehl 0x%02x)\n",
			       cbw[15]);
			return 1;
		}
		if (got_tag != tag) {
			printf("FEHLER: Kennzeichen %08x statt %08x\n", got_tag, tag);
			return 1;
		}

		const size_t payload = sent - OSK_MSC_CSW_LEN;

		if (payload > host_len) {
			printf("FEHLER: %zu Byte hinaus, der Rechner wollte %u (Befehl 0x%02x)\n",
			       payload, host_len, cbw[15]);
			return 1;
		}
		if (status == OSK_MSC_CSW_PASSED && residue != host_len - (uint32_t)payload) {
			printf("FEHLER: Rueckstand %u passt nicht zu %zu von %u (Befehl 0x%02x)\n",
			       residue, payload, host_len, cbw[15]);
			return 1;
		}
		if (residue > host_len) {
			printf("FEHLER: Rueckstand %u groesser als die Anforderung %u\n", residue,
			       host_len);
			return 1;
		}

		answered++;
		data_bytes += payload;
		if (status == OSK_MSC_CSW_PASSED)
			passed++;
		else
			refused++;
		runs++;
	}

	/* Die eigentliche Zusicherung: nach allem, was da hineinging, steht
	 * das Laufwerk unveraendert da. */
	for (uint32_t b = 0; b < DRIVE_BLOCKS; b++) {
		if (fake_drive_read(b, block, NULL) != OSK_OK) {
			printf("FEHLER: Block %u nach dem Lauf nicht mehr lesbar\n", b);
			return 1;
		}
		if (memcmp(block, image_before[b], OSK_MSC_BLOCK_LEN) != 0) {
			printf("FEHLER: Block %u hat sich veraendert\n", b);
			return 1;
		}
	}
	if (fake_drive_was_written()) {
		printf("FEHLER: es wurde geschrieben\n");
		return 1;
	}

	printf("fuzz_scsi: %lu Befehle, %lu beantwortet, %lu gut, %lu abgewiesen, "
	       "%lu Byte Daten, %lu Datenphasen verworfen, %u Schreibversuche abgewehrt\n",
	       runs, answered, passed, refused, data_bytes, discarded, osk_msc_write_attempts());

	if (runs == 0u) {
		printf("FEHLER: kein Lauf\n");
		return 1;
	}
	if (answered == 0u) {
		printf("FEHLER: kein einziger Befehl kam durch den Rahmen - der Lauf hat nur "
		       "die erste Wand getroffen\n");
		return 1;
	}
	if (passed == 0u || refused == 0u) {
		printf("FEHLER: nur eine Sorte Antwort (%lu gut, %lu abgewiesen)\n", passed,
		       refused);
		return 1;
	}
	if (data_bytes == 0u) {
		printf("FEHLER: es kamen nie Daten heraus - der Leseweg wurde nicht "
		       "getroffen\n");
		return 1;
	}
	if (osk_msc_write_attempts() == 0u) {
		printf("FEHLER: kein einziger Schreibversuch dabei\n");
		return 1;
	}

	printf("fuzzlauf: ok\n");
	fake_drive_free();
	return 0;
}
