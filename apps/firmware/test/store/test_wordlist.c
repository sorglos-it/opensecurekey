/* Die Woerterliste fuer den Kennwortvorschlag. Sie ist Teil der Sicherheit:
 * 256 Woerter sind genau 8 Bit, und acht Woerter sind 64 Bit. Fehlt ein Wort
 * oder steht eines doppelt, waere die Rechnung falsch. */

#include <string.h>
#include "osk_test.h"
#include "osk_backup.h"

void test_wordlist(void)
{
	osk_test_case("256 Woerter, jedes genau einmal");
	for (size_t i = 0; i < OSK_WORDLIST_LEN; i++) {
		T_TRUE(osk_wordlist[i] != NULL);
		for (size_t j = i + 1u; j < OSK_WORDLIST_LEN; j++) {
			if (strcmp(osk_wordlist[i], osk_wordlist[j]) == 0) {
				T_TRUE(false);
			}
		}
	}

	osk_test_case("Nur Kleinbuchstaben a-z, 3 bis 7 Zeichen");
	for (size_t i = 0; i < OSK_WORDLIST_LEN; i++) {
		size_t n = strlen(osk_wordlist[i]);

		T_TRUE(n >= 3u && n <= 7u);
		for (size_t c = 0; c < n; c++) {
			char ch = osk_wordlist[i][c];

			T_TRUE(ch >= 'a' && ch <= 'z');
		}
	}

	osk_test_case("Kein Wort ist der Anfang eines anderen");
	for (size_t i = 0; i < OSK_WORDLIST_LEN; i++) {
		for (size_t j = 0; j < OSK_WORDLIST_LEN; j++) {
			if (i == j) {
				continue;
			}
			size_t n = strlen(osk_wordlist[i]);

			if (strncmp(osk_wordlist[i], osk_wordlist[j], n) == 0) {
				T_TRUE(false);
			}
		}
	}

	osk_test_case("Acht Woerter passen in den Vorschlagspuffer");
	size_t longest = 0;

	for (size_t i = 0; i < OSK_WORDLIST_LEN; i++) {
		size_t n = strlen(osk_wordlist[i]);

		if (n > longest) {
			longest = n;
		}
	}
	T_TRUE(OSK_PASS_WORDS * (longest + 1u) < OSK_PASS_SUGGEST_MAX);

	osk_test_case("Acht Woerter sind lang genug fuer die Mindestlaenge");
	size_t shortest = 99u;

	for (size_t i = 0; i < OSK_WORDLIST_LEN; i++) {
		size_t n = strlen(osk_wordlist[i]);

		if (n < shortest) {
			shortest = n;
		}
	}
	/* Auch der kuerzeste Vorschlag erfuellt die Regel von selbst. */
	T_TRUE(OSK_PASS_WORDS * shortest + (OSK_PASS_WORDS - 1u) >= OSK_PASS_MIN);
}
