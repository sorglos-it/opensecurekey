/* Umbruch, Seiten und UTF-8.
 *
 * Die eine Zusicherung, auf die es ankommt: was hineingeht, kommt auch
 * heraus. Ueber alle Seiten zusammengesetzt steht jedes Zeichen des Textes
 * wieder da - nur die Leerzeichen, an denen umbrochen wurde, fehlen. Ein
 * abgeschnittener Empfaenger waere bei einer Ueberweisung gefaehrlicher als
 * gar keine Anzeige. */

#include <string.h>
#include "osk_text.h"
#include "ui_test.h"

/* Alles ohne Leerzeichen hintereinander - so laesst sich vergleichen,
 * unabhaengig davon, wo umbrochen wurde. */
static void squash(const char *text, size_t len, char *out, size_t cap)
{
	size_t used = 0;

	for (size_t i = 0; i < len && used + 1 < cap; i++) {
		if (text[i] != ' ') {
			out[used++] = text[i];
		}
	}
	out[used] = '\0';
}

/* Umbricht den ganzen Text ueber beliebig viele Seiten und legt alle Zeilen
 * hintereinander ab. Liefert die Zahl der Zeilen, 0 bei einem Stillstand. */
static size_t all_lines(const char *text, uint8_t columns, size_t rows, char *out, size_t cap,
			size_t *max_chars_out)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t pos = 0;
	size_t len = strlen(text);
	size_t total = 0;
	size_t used = 0;
	size_t max_chars = 0;

	out[0] = '\0';
	while (pos < len) {
		size_t next = pos;
		size_t count = osk_text_wrap(text, pos, columns, lines, rows, &next);

		if (count == 0 || next <= pos) {
			return 0; /* kein Fortschritt - waere ein Fehler */
		}
		for (size_t i = 0; i < count; i++) {
			size_t chars = osk_text_chars(text + lines[i].start, lines[i].len);

			if (chars > max_chars) {
				max_chars = chars;
			}
			for (size_t b = 0; b < lines[i].len && used + 1 < cap; b++) {
				out[used++] = text[lines[i].start + b];
			}
			total++;
		}
		pos = next;
	}
	out[used] = '\0';
	if (max_chars_out != NULL) {
		*max_chars_out = max_chars;
	}
	return total;
}

/* Prueft fuer einen Text: nichts verloren, keine Zeile zu lang. */
static void nothing_lost(const char *text, uint8_t columns, size_t rows)
{
	char joined[512];
	char want[512];
	char got[512];
	size_t max_chars = 0;
	size_t lines = all_lines(text, columns, rows, joined, sizeof(joined), &max_chars);

	T_TRUE(lines > 0 || text[0] == '\0');
	squash(text, strlen(text), want, sizeof(want));
	squash(joined, strlen(joined), got, sizeof(got));
	T_STR(got, want);
	T_TRUE(max_chars <= columns);
}

void test_text(void)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t next = 0;
	size_t count;
	uint32_t codepoint = 0;

	osk_test_case("UTF-8 lesen: ein Byte, zwei Bytes, drei Bytes");
	T_EQ(osk_utf8_next("a", 1, 0, &codepoint), 1);
	T_EQ(codepoint, 'a');
	T_EQ(osk_utf8_next("ä", 2, 0, &codepoint), 2);
	T_EQ(codepoint, 0x00E4u);
	T_EQ(osk_utf8_next("€", 3, 0, &codepoint), 3);
	T_EQ(codepoint, 0x20ACu);

	osk_test_case("kaputtes UTF-8 haengt nicht und verschluckt nichts");
	T_EQ(osk_utf8_next("\xC3", 1, 0, &codepoint), 1); /* Kopf ohne Rest */
	T_EQ(codepoint, 0xFFFDu);
	T_EQ(osk_utf8_next("\x80", 1, 0, &codepoint), 1); /* Rest ohne Kopf */
	T_EQ(codepoint, 0xFFFDu);
	T_EQ(osk_utf8_next("\xC3\x28", 2, 0, &codepoint), 1); /* falscher Rest */
	T_EQ(osk_utf8_next("abc", 3, 3, &codepoint), 0);      /* hinter dem Ende */

	osk_test_case("Zeichen zaehlen, nicht Bytes");
	T_EQ(osk_text_chars("abc", 3), 3);
	T_EQ(osk_text_chars("äöü", 6), 3);

	osk_test_case("kurzer Text bleibt eine Zeile");
	count = osk_text_wrap("Github", 0, 12, lines, OSK_TEXT_MAX_LINES, &next);
	T_EQ(count, 1);
	T_EQ(lines[0].len, 6);
	T_EQ(next, 6);

	osk_test_case("umbrochen wird am Leerzeichen");
	count = osk_text_wrap("Sorglos Thomas", 0, 8, lines, OSK_TEXT_MAX_LINES, &next);
	T_EQ(count, 2);
	T_EQ(lines[0].len, 7); /* "Sorglos" - das Leerzeichen entfaellt */
	T_EQ(lines[1].len, 6); /* "Thomas" */

	osk_test_case("ein Wort, das allein zu lang ist, wird hart geteilt");
	count = osk_text_wrap("0x1234567890abcdef", 0, 8, lines, OSK_TEXT_MAX_LINES, &next);
	T_TRUE(count >= 3);
	T_EQ(lines[0].len, 8);

	osk_test_case("leerer Text ist kein Absturz");
	count = osk_text_wrap("", 0, 8, lines, OSK_TEXT_MAX_LINES, &next);
	T_EQ(count, 0);
	T_EQ(next, 0);
	T_EQ(osk_text_wrap(NULL, 0, 8, lines, OSK_TEXT_MAX_LINES, &next), 0);
	T_EQ(osk_text_wrap("abc", 0, 0, lines, OSK_TEXT_MAX_LINES, &next), 0);
	T_EQ(osk_text_wrap("abc", 0, 8, lines, 0, &next), 0);
	T_EQ(osk_text_wrap("abc", 99, 8, lines, OSK_TEXT_MAX_LINES, &next), 0);

	osk_test_case("nichts geht verloren - kurze Namen");
	nothing_lost("login.example.com", 12, 4);
	nothing_lost("Sorglos Thomas Weirich", 12, 4);
	nothing_lost("a", 12, 4);

	osk_test_case("nichts geht verloren - eine Krypto-Adresse");
	nothing_lost("0x71C7656EC7ab88b098defB751B7401B5f6d8976F", 12, 4);
	nothing_lost("bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq", 12, 4);

	osk_test_case("nichts geht verloren - sehr lange Angaben");
	nothing_lost("ein-sehr-langer-dienstname-der-nicht-in-eine-zeile-passt.example.org", 12, 4);
	nothing_lost("Zahlung an Hans Mueller GmbH und Co KG, Rechnung 2026-10-24, 1234,56 EUR", 12,
		     4);

	osk_test_case("nichts geht verloren - auch bei einer Spalte Breite");
	nothing_lost("abcdef ghij", 1, 4);

	osk_test_case("Buchstaben mit Zeichen werden nicht mitten im Byte geteilt");
	nothing_lost("äöüäöüäöüä", 4, 2);
	count = osk_text_wrap("äöüä", 0, 2, lines, OSK_TEXT_MAX_LINES, &next);
	T_EQ(count, 2);
	T_EQ(lines[0].len, 4); /* zwei Zeichen, vier Bytes */

	osk_test_case("Seiten zaehlen und anspringen");
	T_EQ(osk_text_pages("kurz", 12, 4), 1);
	T_TRUE(osk_text_pages("0x71C7656EC7ab88b098defB751B7401B5f6d8976F", 8, 2) > 2);
	T_EQ(osk_text_page_start("abcdefgh", 4, 1, 0), 0);
	T_EQ(osk_text_page_start("abcdefgh", 4, 1, 1), 4);
	T_EQ(osk_text_page_start("abcdefgh", 4, 1, 99), 8); /* hinter dem Ende: Schluss */

	osk_test_case("Breite rechnen und NULL aushalten");
	T_EQ(osk_text_width(&osk_font_small, "abc", 3), 3 * osk_font_small.advance);
	T_EQ(osk_text_width(NULL, "abc", 3), 0);
	T_EQ(osk_text_draw(NULL, &osk_font_small, "abc", 3, 0, 0), 3 * osk_font_small.advance);
	T_EQ(osk_text_draw(NULL, NULL, "abc", 3, 0, 0), 0);
}
