/* Winziges Pruefgeruest der Anzeige - gleiche Bauart wie beim Tresor:
 * zaehlen, melden, am Ende die Summe. Kein Fremdcode. */
#ifndef UI_TEST_H
#define UI_TEST_H

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

#define T_STR(a, b)                                                                             \
	do {                                                                                    \
		osk_test_checks++;                                                              \
		if (strcmp((a), (b)) != 0) {                                                    \
			char osk_t_msg[200];                                                    \
			snprintf(osk_t_msg, sizeof(osk_t_msg), "%s == %s (\"%.100s\")", #a, #b, \
				 (a));                                                          \
			osk_test_fail(__FILE__, __LINE__, osk_t_msg);                           \
		}                                                                               \
	} while (0)

/* Die einzelnen Pruefsaetze. */
void test_gfx(void);
void test_font(void);
void test_text(void);
void test_lang(void);
void test_button(void);
void test_panel(void);
void test_confirm(void);
void test_flow(void);

#endif
