/* Winziges Pruefgeruest. Kein Fremdcode, keine Abhaengigkeit: zaehlen,
 * melden, am Ende die Summe. */
#ifndef OSK_TEST_H
#define OSK_TEST_H

#include <stdio.h>
#include <string.h>
#include "osk_result.h"

extern int osk_test_fails;
extern int osk_test_checks;
extern const char *osk_test_current;

void osk_test_case(const char *name);
void osk_test_fail(const char *file, int line, const char *what);
const char *osk_test_rtext(osk_result_t r);

#define T_TRUE(cond)                                              \
	do {                                                      \
		osk_test_checks++;                                \
		if (!(cond)) {                                    \
			osk_test_fail(__FILE__, __LINE__, #cond); \
		}                                                 \
	} while (0)

#define T_EQ(a, b)                                                                                \
	do {                                                                                      \
		osk_test_checks++;                                                                \
		long long osk_t_a = (long long)(a);                                               \
		long long osk_t_b = (long long)(b);                                               \
		if (osk_t_a != osk_t_b) {                                                         \
			char osk_t_msg[160];                                                      \
			snprintf(osk_t_msg, sizeof(osk_t_msg), "%s == %s (%lld != %lld)", #a, #b, \
				 osk_t_a, osk_t_b);                                               \
			osk_test_fail(__FILE__, __LINE__, osk_t_msg);                             \
		}                                                                                 \
	} while (0)

/* Vergleicht Rueckgabewerte und sagt im Fehlerfall, was wirklich kam. */
#define T_RES(expr, want)                                                                        \
	do {                                                                                     \
		osk_test_checks++;                                                               \
		osk_result_t osk_t_got = (expr);                                                 \
		if (osk_t_got != (want)) {                                                       \
			char osk_t_msg[200];                                                     \
			snprintf(osk_t_msg, sizeof(osk_t_msg), "%s: %s erwartet, %s kam", #expr, \
				 osk_test_rtext(want), osk_test_rtext(osk_t_got));               \
			osk_test_fail(__FILE__, __LINE__, osk_t_msg);                            \
		}                                                                                \
	} while (0)

#define T_OK(expr) T_RES(expr, OSK_OK)

#define T_MEM(a, b, n)                                                                           \
	do {                                                                                     \
		osk_test_checks++;                                                               \
		if (memcmp((a), (b), (n)) != 0) {                                                \
			osk_test_fail(__FILE__, __LINE__, "Inhalt stimmt nicht: " #a " != " #b); \
		}                                                                                \
	} while (0)

#define T_STR(a, b)                                                                               \
	do {                                                                                      \
		osk_test_checks++;                                                                \
		if (strcmp((a), (b)) != 0) {                                                      \
			char osk_t_msg[200];                                                      \
			snprintf(osk_t_msg, sizeof(osk_t_msg), "%s == %s (\"%s\")", #a, #b, (a)); \
			osk_test_fail(__FILE__, __LINE__, osk_t_msg);                             \
		}                                                                                 \
	} while (0)

/* Die einzelnen Pruefsaetze. */
void test_core(void);
void test_power(void);
void test_migrate(void);
void test_backup(void);
void test_shamir(void);
void test_transfer(void);
void test_wear(void);
void test_wordlist(void);
void test_tag(void);
void test_paging(void);
void test_public(void);
void test_guard(void);
void test_logchain(void);
void test_wipe(void);
void test_sigcount(void);
void test_capacity(void);

#endif
