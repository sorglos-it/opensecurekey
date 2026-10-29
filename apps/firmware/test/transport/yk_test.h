/* Pruefgeruest fuer Strang E. Kein Fremdcode, keine Abhaengigkeit.
 *
 * Warum nicht test_util.h aus test/ctap: das ist ein Kopfdateigeruest mit
 * __attribute__((unused)) darin und laeuft nur unter gcc. Die Pruefungen
 * dieses Strangs muessen unter MSVC laufen, weil auf diesem Rechner kein
 * gcc und kein make da ist (gemessen: which gcc/cc/clang/make - nichts).
 * Darum liegt das Geruest hier in einer .c-Datei, die beide Uebersetzer
 * annehmen, und heisst anders, damit es das andere nicht verdeckt.
 *
 * Wichtig fuer das Messen: t_report() druckt immer die Zahl der
 * gelaufenen Faelle. Ein Lauf ohne diese Zeile ist nicht durchgelaufen,
 * auch wenn der Ausstiegscode 0 ist. */
#ifndef YK_TEST_H
#define YK_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern int t_checks;
extern int t_fails;

void t_fail(const char *what, const char *file, int line, const char *detail);
const char *t_hex(const void *p, size_t len);
size_t t_unhex(const char *hex_in, uint8_t *out, size_t cap);
int t_report(const char *name);

#define T_TRUE(what, cond)                                         \
	do {                                                       \
		t_checks++;                                        \
		if (!(cond))                                       \
			t_fail((what), __FILE__, __LINE__, #cond); \
	} while (0)

#define T_EQ(what, got, want)                                                          \
	do {                                                                           \
		long long g_ = (long long)(got);                                       \
		long long w_ = (long long)(want);                                      \
		t_checks++;                                                            \
		if (g_ != w_) {                                                        \
			char d_[96];                                                   \
			snprintf(d_, sizeof(d_), "bekam %lld, erwartet %lld", g_, w_); \
			t_fail((what), __FILE__, __LINE__, d_);                        \
		}                                                                      \
	} while (0)

#define T_HEX(what, buf, len, want_hex)                                                      \
	do {                                                                                 \
		const char *got_ = t_hex((buf), (len));                                      \
		t_checks++;                                                                  \
		if (strcmp(got_, (want_hex)) != 0) {                                         \
			printf("FEHLER %s:%d  %s\n  bekam    %s\n  erwartet %s\n", __FILE__, \
			       __LINE__, (what), got_, (want_hex));                          \
			t_fails++;                                                           \
		}                                                                            \
	} while (0)

/* Die vier Pruefsaetze dieses Strangs. */
void suite_ykcrc(void);
void suite_ykframe(void);
void suite_ykhid(void);
void suite_ykkbd(void);

#endif
