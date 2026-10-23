/* Winziger Pruefrahmen - absichtlich keine Fremdbibliothek.
 *
 * Er kann genau drei Dinge: eine Bedingung pruefen, zwei Puffer
 * vergleichen und am Ende sagen, wie viele Pruefungen durchgefallen sind.
 * Mehr braucht dieser Treiber nicht, und ein Pruefrahmen, der selbst
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

/* Jede Pruefdatei bringt eine davon mit. */
void test_crc(void);
void test_tlv(void);
void test_t1(void);
void test_scp03(void);
void test_errors(void);
void test_contract(void);

#endif
