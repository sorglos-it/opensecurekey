/* Winziges Pruefgeruest: zaehlt Faelle, nennt jeden Fehlschlag mit Zeile und
 * liefert am Ende einen Ausstiegscode. Kein Fremdcode, keine Abhaengigkeit.
 *
 * Wichtig fuer das Messen: t_report() druckt immer die Zahl der gelaufenen
 * Faelle. Ein Lauf ohne diese Zeile ist nicht durchgelaufen, auch wenn der
 * Ausstiegscode 0 ist. */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static int t_checks;
static int t_fails;

static inline void t_fail(const char *what, const char *file, int line, const char *detail)
{
	t_fails++;
	printf("FEHLER %s:%d  %s%s%s\n", file, line, what,
	       detail ? " -> " : "", detail ? detail : "");
}

#define T_TRUE(what, cond)                                                    \
	do {                                                                  \
		t_checks++;                                                   \
		if (!(cond))                                                  \
			t_fail((what), __FILE__, __LINE__, #cond);             \
	} while (0)

#define T_EQ(what, got, want)                                                 \
	do {                                                                  \
		t_checks++;                                                   \
		const long long g_ = (long long)(got);                        \
		const long long w_ = (long long)(want);                       \
		if (g_ != w_) {                                               \
			char d_[96];                                          \
			snprintf(d_, sizeof(d_), "bekam %lld, erwartet %lld", g_, w_); \
			t_fail((what), __FILE__, __LINE__, d_);                \
		}                                                             \
	} while (0)

/* Wandelt Bytes in Kleinbuchstaben-Hex. Rueckgabe zeigt auf einen von vier
 * umlaufenden Puffern, damit zwei Aufrufe in einem printf gehen. */
static inline __attribute__((unused)) const char *t_hex(const void *p, size_t len)
{
	static char bufs[4][1024];
	static unsigned which;
	char *out = bufs[which++ & 3u];
	const uint8_t *b = (const uint8_t *)p;
	size_t n = len;

	if (n > (sizeof(bufs[0]) - 1) / 2)
		n = (sizeof(bufs[0]) - 1) / 2;
	for (size_t i = 0; i < n; i++)
		snprintf(out + i * 2, 3, "%02x", b[i]);
	out[n * 2] = 0;
	return out;
}

#define T_HEX(what, buf, len, want_hex)                                       \
	do {                                                                  \
		t_checks++;                                                   \
		const char *got_ = t_hex((buf), (len));                       \
		if (strcmp(got_, (want_hex)) != 0) {                          \
			printf("FEHLER %s:%d  %s\n  bekam    %s\n  erwartet %s\n",   \
			       __FILE__, __LINE__, (what), got_, (want_hex));  \
			t_fails++;                                            \
		}                                                             \
	} while (0)

static inline __attribute__((unused)) int t_report(const char *name)
{
	printf("%s: %d Faelle, %d Fehler\n", name, t_checks, t_fails);
	if (t_fails == 0)
		printf("pruefung: ok\n");
	return t_fails == 0 ? 0 : 1;
}

/* Hex-Text in Bytes. Leerzeichen sind erlaubt, damit lange Vorlagen
 * lesbar bleiben. Liefert die Zahl der Bytes, oder 0 bei krummem Text. */
static inline __attribute__((unused)) size_t t_unhex(const char *hex_in, uint8_t *out, size_t cap)
{
	char hex[2048];
	size_t n = 0;

	for (const char *s = hex_in; *s; s++) {
		if (*s == ' ' || *s == '\t' || *s == '\n')
			continue;
		if (n + 1 >= sizeof(hex))
			return 0;
		hex[n++] = *s;
	}
	hex[n] = 0;

	if ((n % 2) != 0 || n / 2 > cap)
		return 0;
	for (size_t i = 0; i < n / 2; i++) {
		unsigned v = 0;
		for (unsigned k = 0; k < 2; k++) {
			const char c = hex[i * 2 + k];
			unsigned d;
			if (c >= '0' && c <= '9')
				d = (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				d = (unsigned)(c - 'a') + 10u;
			else if (c >= 'A' && c <= 'F')
				d = (unsigned)(c - 'A') + 10u;
			else
				return 0;
			v = (v << 4) | d;
		}
		out[i] = (uint8_t)v;
	}
	return n / 2;
}

#endif
