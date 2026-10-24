/* Winziges Pruefgeruest fuer die Update-Pruefungen. Eigener Code, keine
 * Abhaengigkeit: zaehlen, melden, am Ende die Summe - dieselbe Form, die
 * tools/check.py bei den anderen Saetzen liest ("N Pruefungen, M Fehler"). */
#ifndef BOOT_TEST_H
#define BOOT_TEST_H

#include <stdio.h>
#include <string.h>
#include "osk_result.h"

extern int bt_fails;
extern int bt_checks;
extern const char *bt_current;

void bt_case(const char *name);
void bt_fail(const char *file, int line, const char *what);
const char *bt_rtext(osk_result_t r);

#define T_TRUE(cond)                                        \
	do {                                                \
		bt_checks++;                                \
		if (!(cond)) {                              \
			bt_fail(__FILE__, __LINE__, #cond); \
		}                                           \
	} while (0)

#define T_EQ(a, b)                                                                                \
	do {                                                                                      \
		bt_checks++;                                                                      \
		long long bt_a = (long long)(a);                                                  \
		long long bt_b = (long long)(b);                                                  \
		if (bt_a != bt_b) {                                                               \
			char bt_msg[160];                                                         \
			snprintf(bt_msg, sizeof(bt_msg), "%s == %s (%lld != %lld)", #a, #b, bt_a, \
				 bt_b);                                                           \
			bt_fail(__FILE__, __LINE__, bt_msg);                                      \
		}                                                                                 \
	} while (0)

/* Vergleicht Rueckgabewerte und sagt im Fehlerfall, was wirklich kam. */
#define T_RES(expr, want)                                                                  \
	do {                                                                               \
		bt_checks++;                                                               \
		osk_result_t bt_got = (expr);                                              \
		if (bt_got != (want)) {                                                    \
			char bt_msg[200];                                                  \
			snprintf(bt_msg, sizeof(bt_msg), "%s: %s erwartet, %s kam", #expr, \
				 bt_rtext(want), bt_rtext(bt_got));                        \
			bt_fail(__FILE__, __LINE__, bt_msg);                               \
		}                                                                          \
	} while (0)

#define T_OK(expr) T_RES(expr, OSK_OK)

#define T_MEM(a, b, n)                                                                     \
	do {                                                                               \
		bt_checks++;                                                               \
		if (memcmp((a), (b), (n)) != 0) {                                          \
			bt_fail(__FILE__, __LINE__, "Inhalt stimmt nicht: " #a " != " #b); \
		}                                                                          \
	} while (0)

/* Die einzelnen Pruefsaetze. */
void test_signature(void);
void test_rollback(void);
void test_board(void);
void test_state(void);
void test_abort(void);
void test_vault(void);
void test_recovery(void);

#endif
