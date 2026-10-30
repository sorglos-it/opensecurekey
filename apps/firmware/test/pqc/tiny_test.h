/* Winziger Pruefrahmen - absichtlich keine Fremdbibliothek.
 *
 * Derselbe Rahmen wie unter test/se050/, erweitert um eine Hilfe fuer
 * Hexwerte. Zwei Kopien statt eines gemeinsamen Kopfes: ein Pruefsatz,
 * der am Rahmen eines anderen haengt, bricht mit dessen Aenderungen.
 *
 * Er kann genau drei Dinge: eine Bedingung pruefen, zwei Puffer
 * vergleichen und am Ende sagen, wie viele Pruefungen durchgefallen sind.
 * Mehr braucht dieses Modul nicht, und ein Pruefrahmen, der selbst
 * gepflegt werden muss, waere hier teurer als nuetzlich. */
#ifndef TINY_TEST_H
#define TINY_TEST_H

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

extern int g_tt_failed;
extern int g_tt_checks;

static inline void tt_hex(const char *label, const uint8_t *p, size_t n)
{
	printf("    %s (%u Byte): ", label, (unsigned)n);
	for (size_t i = 0; i < n && i < 64u; i++) {
		printf("%02x", p[i]);
	}
	if (n > 64u) {
		printf("...");
	}
	printf("\n");
}

#define TT_CASE(name) printf("  %s\n", (name))

#define TT_TRUE(cond, what)                                                         \
	do {                                                                        \
		g_tt_checks++;                                                      \
		if (!(cond)) {                                                      \
			g_tt_failed++;                                              \
			printf("  FEHLER %s:%d  %s\n", __FILE__, __LINE__, (what)); \
		}                                                                   \
	} while (0)

#define TT_EQ_INT(got, want, what)                                  \
	do {                                                        \
		g_tt_checks++;                                      \
		const long long _g = (long long)(got);              \
		const long long _w = (long long)(want);             \
		if (_g != _w) {                                     \
			g_tt_failed++;                              \
			printf("  FEHLER %s:%d  %s\n"               \
			       "    ist %lld, soll %lld\n",         \
			       __FILE__, __LINE__, (what), _g, _w); \
		}                                                   \
	} while (0)

#define TT_EQ_MEM(got, want, n, what)                                               \
	do {                                                                        \
		g_tt_checks++;                                                      \
		if (memcmp((got), (want), (n)) != 0) {                              \
			g_tt_failed++;                                              \
			printf("  FEHLER %s:%d  %s\n", __FILE__, __LINE__, (what)); \
			tt_hex("ist ", (const uint8_t *)(got), (n));                \
			tt_hex("soll", (const uint8_t *)(want), (n));               \
		}                                                                   \
	} while (0)

/* Hexwert in Byte verwandeln. Die amtlichen Testwerte stehen als
 * Zeichenkette da, weil 2400 Byte als Bytetabelle das Dreifache an
 * Platz braeuchten. Gibt die Anzahl der Byte zurueck, 0 bei kaputter
 * Eingabe oder wenn out_max nicht reicht. */
size_t tt_unhex(uint8_t *out, size_t out_max, const char *hex);

/* Jede Pruefdatei bringt eine davon mit. */
void test_keccak(void);
void test_poly(void);
void test_mlkem(void);
void test_hybrid(void);

#endif
