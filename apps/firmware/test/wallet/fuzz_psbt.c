/* Fuzzlauf ueber den PSBT-Leser - und ueber alles, was danach kommt.
 *
 * Hier kommt ein Angreifer zuerst an: er schickt ein PSBT, und der Leser
 * muss es abweisen, ohne ueber einen Puffer zu laufen. Reiner Zufall kommt
 * an der Kennung "psbt\xff" nicht vorbei, darum werden vor allem echte
 * PSBT aus BIP174 verbogen.
 *
 * Aufruf: fuzz_psbt [Sekunden] [Startwert]
 * Mit -fsanitize=address,undefined uebersetzt wird jeder Ueberlauf zum
 * Abbruch, nicht zu einem stillen Fehler.
 *
 * Geprueft werden drei Zusicherungen:
 *  1. Kein Aufruf bringt den Prozess um oder schreibt ueber einen Puffer.
 *  2. Was gelesen wurde, laesst sich anzeigen - osk_psbt_view faellt nie.
 *  3. Was unterschrieben wurde, ist wieder ein lesbares PSBT. Ein
 *     Ergebnis, das unser eigener Leser ablehnt, waere ein Fehler.
 * Und: ein Lauf, in dem nichts durchkommt, ist wertlos und meldet das. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "psbt_vectors.h"
#include "wallet_psbt.h"
#include "wallet_seed.h"
#include "mocks.h"

static uint64_t g_rng;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static size_t rnd_below(size_t n)
{
	return (n == 0) ? 0 : (size_t)(rnd() % n);
}

static size_t unhex(const char *in, uint8_t *out, size_t cap)
{
	size_t n = 0;

	for (const char *s = in; *s != '\0'; s += 2) {
		unsigned value = 0;

		if (s[1] == '\0')
			return 0;
		for (unsigned k = 0; k < 2; k++) {
			char c = s[k];

			value <<= 4;
			if (c >= '0' && c <= '9')
				value |= (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				value |= (unsigned)(c - 'a') + 10u;
			else if (c >= 'A' && c <= 'F')
				value |= (unsigned)(c - 'A') + 10u;
			else
				return 0;
		}
		if (n >= cap)
			return 0;
		out[n++] = (uint8_t)value;
	}
	return n;
}

static size_t mutate(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
	size_t n = len;
	unsigned rounds;

	if (n > cap)
		n = cap;
	memcpy(out, in, n);
	rounds = 1u + (rnd() % 6u);
	for (unsigned i = 0; i < rounds; i++) {
		switch (rnd() % 6u) {
		case 0:
			if (n > 0)
				out[rnd_below(n)] = (uint8_t)rnd();
			break;
		case 1:
			if (n > 0)
				out[rnd_below(n)] ^= (uint8_t)(1u << (rnd() % 8u));
			break;
		case 2:
			if (n > 1u)
				n = 1u + rnd_below(n - 1u);
			break;
		case 3:
			if (n < cap)
				out[n++] = (uint8_t)rnd();
			break;
		case 4:
			/* Eine Laengenangabe gross machen - das ist der Weg,
			 * auf dem ein Leser ueber das Ende hinausliest. */
			if (n > 0)
				out[rnd_below(n)] = 0xFDu;
			break;
		default:
			/* Ein Trennbyte einfuegen oder loeschen bringt die
			 * Abschnitte durcheinander. */
			if (n > 0)
				out[rnd_below(n)] = 0x00u;
			break;
		}
	}
	return n;
}

/* Das eigene PSBT aus test_psbt.c - mit ihm kommt der Lauf bis ins
 * Unterschreiben, denn der Seed dazu liegt im Tresor. */
static const char *const OWN_PSBT =
    "70736274ff0100710200000001aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aa0000000000feffffff0260ea000000000000160014751e76e8199196d454941c45d1b3a323f1433bd63075"
    "0000000000001600143e34985dca6fddc9fb369940e4c7d8e2873f529c000000000001011fa0860100000000"
    "00160014c0cebcd6c3d3ca8c75dc5ec62ebe55330ef910e222060330d54fd0dd420a6e5f8d3624f5f3482cae"
    "350f79d5f0753bf5beef9c2d91af3c1873c5da0a540000800000008000000080000000000000000000002202"
    "03025324888e429ab8e3dbaf1f7802648b9cd01e9b418485c5fa4c1b9b5700e1a61873c5da0a540000800000"
    "008000000080010000000000000000";
static const char *const MNEMONIC =
    "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " about";

#define TEMPLATES (sizeof(PSBT_VALID) / sizeof(PSBT_VALID[0]) + 1u)

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	static uint8_t templ[TEMPLATES][4096];
	static size_t templ_len[TEMPLATES];
	static uint8_t msg[4096];
	static uint8_t out[4096];
	osk_item_id_t seed_id = 0;
	unsigned long runs = 0, parsed = 0, viewed = 0, signed_ok = 0, pure_random = 0;
	time_t start;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x5eed5eed5eedULL;
	if (g_rng == 0)
		g_rng = 1;

	mock_store_reset();
	mock_se050_reset();
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	if (osk_wallet_seed_store(0, "fuzz", MNEMONIC, NULL, &seed_id) != OSK_OK) {
		printf("FEHLER: Seed laesst sich nicht ablegen\n");
		return 1;
	}

	for (size_t i = 0; i + 1u < TEMPLATES; i++)
		templ_len[i] = unhex(PSBT_VALID[i].hex, templ[i], sizeof(templ[i]));
	templ_len[TEMPLATES - 1u] = unhex(OWN_PSBT, templ[TEMPLATES - 1u], sizeof(templ[0]));
	for (size_t i = 0; i < TEMPLATES; i++) {
		if (templ_len[i] == 0) {
			printf("FEHLER: Vorlage %zu ist leer - der Lauf wuerde nichts pruefen\n",
			       i);
			return 1;
		}
	}

	start = time(NULL);
	while (time(NULL) - start < secs) {
		osk_psbt_t psbt;
		size_t len;

		if ((rnd() % 16u) == 0u) {
			/* Jeder sechzehnte Durchgang nimmt das eigene PSBT
			 * UNVERAENDERT. Ohne das kommt der Lauf nie bis zum
			 * Unterschreiben: eine verbogene Ableitungsangabe
			 * zeigt nicht mehr auf unseren Seed, und dann hoert
			 * es vorher auf. Geprueft wird hier der tiefe Weg,
			 * nicht nur die Wand davor. */
			len = templ_len[TEMPLATES - 1u];
			memcpy(msg, templ[TEMPLATES - 1u], len);
		} else if ((rnd() % 8u) == 0u) {
			len = 1u + rnd_below(200);
			for (size_t i = 0; i < len; i++)
				msg[i] = (uint8_t)rnd();
			pure_random++;
		} else {
			size_t which = rnd_below(TEMPLATES);

			len = mutate(templ[which], templ_len[which], msg, sizeof(msg));
		}
		runs++;

		if (osk_psbt_parse(msg, len, &psbt) != OSK_OK)
			continue;
		parsed++;

		{
			osk_wallet_view_t view;

			if (osk_psbt_view(&psbt, 0x73c5da0au, "bc", 0x00u, 0x05u, &view) !=
			    OSK_OK) {
				printf("FEHLER: osk_psbt_view faellt bei %zu Bytes\n", len);
				return 1;
			}
			viewed++;
		}
		{
			size_t out_len = 0, signed_count = 0;
			osk_result_t res =
			    osk_psbt_sign(&psbt, seed_id, "fuzz", "bc", 0x00u, 0x05u, true, out,
					  sizeof(out), &out_len, &signed_count);

			if (res == OSK_OK && signed_count > 0) {
				osk_psbt_t again;

				signed_ok++;
				if (osk_psbt_parse(out, out_len, &again) != OSK_OK) {
					printf("FEHLER: das Ergebnis des Unterschreibens ist "
					       "kein lesbares PSBT (%zu Bytes hinein, %zu "
					       "heraus)\n",
					       len, out_len);
					return 1;
				}
			}
		}
	}

	printf("fuzz_psbt: %lu Durchgaenge, %lu reiner Zufall, %lu gelesen, %lu angezeigt, "
	       "%lu unterschrieben\n",
	       runs, pure_random, parsed, viewed, signed_ok);
	if (runs == 0) {
		printf("FEHLER: kein einziger Durchgang\n");
		return 1;
	}
	/* Ohne ein einziges gelesenes PSBT haette der Lauf nur die Kennung
	 * geprueft - das ist keine Messung. */
	if (parsed == 0) {
		printf("FEHLER: kein einziges PSBT kam durch den Leser\n");
		return 1;
	}
	if (signed_ok == 0) {
		printf("FEHLER: kein einziges Mal wurde unterschrieben - der Lauf hat den "
		       "tiefen Teil nie erreicht\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
