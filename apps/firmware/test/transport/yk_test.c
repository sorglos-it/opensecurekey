/* Siehe yk_test.h. */
#include "yk_test.h"

int t_checks;
int t_fails;

void t_fail(const char *what, const char *file, int line, const char *detail)
{
	t_fails++;
	printf("FEHLER %s:%d  %s%s%s\n", file, line, what, detail ? " -> " : "",
	       detail ? detail : "");
}

const char *t_hex(const void *p, size_t len)
{
	static char bufs[4][1024];
	static unsigned which;
	char *out = bufs[which++ & 3u];
	const uint8_t *b = (const uint8_t *)p;
	size_t n = len;

	if (n > (sizeof(bufs[0]) - 1u) / 2u)
		n = (sizeof(bufs[0]) - 1u) / 2u;
	for (size_t i = 0; i < n; i++)
		snprintf(out + i * 2u, 3u, "%02x", b[i]);
	out[n * 2u] = 0;
	return out;
}

size_t t_unhex(const char *hex_in, uint8_t *out, size_t cap)
{
	char hex[2048];
	size_t n = 0;

	for (const char *s = hex_in; *s; s++) {
		if (*s == ' ' || *s == '\t' || *s == '\n')
			continue;
		if (n + 1u >= sizeof(hex))
			return 0;
		hex[n++] = *s;
	}
	hex[n] = 0;

	if ((n % 2u) != 0u || n / 2u > cap)
		return 0;
	for (size_t i = 0; i < n / 2u; i++) {
		unsigned v = 0;

		for (unsigned k = 0; k < 2u; k++) {
			const char c = hex[i * 2u + k];
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
	return n / 2u;
}

int t_report(const char *name)
{
	printf("%s: %d Faelle, %d Fehler\n", name, t_checks, t_fails);
	if (t_fails == 0)
		printf("pruefung: ok\n");
	else
		printf("PRUEFUNG ROT\n");
	return t_fails == 0 ? 0 : 1;
}
