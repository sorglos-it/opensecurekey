/* Die Schriften: jedes Zeichen der fuenf Sprachen hat ein Muster, und es
 * unterscheidet sich von den anderen. Eine Schrift, in der zwei Buchstaben
 * gleich aussehen, macht einen Empfaengernamen unlesbar. */

#include <string.h>
#include "osk_font.h"
#include "osk_gfx.h"
#include "osk_text.h"
#include "ui_test.h"

/* Alle Zeichen, die die fuenf Sprachen brauchen - deutsch, englisch,
 * franzoesisch, italienisch, spanisch. */
static const char *const NEEDED = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
				  " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
				  "äöüÄÖÜß"
				  "àáâèéêëìíîï"
				  "òóôùúûÿñçœ"
				  "ÀÁÂÈÉÊËÌÍÎÏ"
				  "ÒÓÔÙÚÛÑÇŒŸ"
				  "¡¿€«»";

static size_t glyph_bits(const osk_font_t *font, size_t index)
{
	size_t bits = 0;

	for (uint8_t row = 0; row < font->rows; row++) {
		uint8_t value = osk_font_row(font, index, row);

		for (uint8_t col = 0; col < 8; col++) {
			bits += ((unsigned)value >> col) & 1u;
		}
	}
	return bits;
}

static void check_font(const osk_font_t *font, const char *name)
{
	size_t len = strlen(NEEDED);
	size_t pos = 0;
	size_t question = osk_font_index('?');

	osk_test_case(name);
	while (pos < len) {
		uint32_t codepoint = 0;
		size_t step = osk_utf8_next(NEEDED, len, pos, &codepoint);
		size_t index;

		T_TRUE(step > 0);
		if (step == 0) {
			break;
		}
		index = osk_font_index(codepoint);
		pos += step;
		if (codepoint == '?') {
			continue;
		}
		/* Nicht das Fragezeichen: dann kennt die Schrift das Zeichen. */
		T_TRUE(index != question);
		if (codepoint != ' ') {
			T_TRUE(glyph_bits(font, index) > 0);
		}
		/* Kein Muster laeuft ueber die Breite der Schrift hinaus. */
		for (uint8_t row = 0; row < font->rows; row++) {
			uint8_t value = osk_font_row(font, index, row);

			T_EQ(value >> font->width, 0);
		}
	}
}

void test_font(void)
{
	check_font(&osk_font_big, "grosse Schrift kennt alle Zeichen der fuenf Sprachen");
	check_font(&osk_font_small, "kleine Schrift kennt alle Zeichen der fuenf Sprachen");

	osk_test_case("Masse passen zur Anzeige");
	T_EQ(osk_font_big.width, 5);
	T_EQ(osk_font_big.rows, 9);
	T_EQ(osk_font_small.width, 3);
	T_EQ(osk_font_small.rows, 7);
	/* Auf 48 Punkten: 8 Zeichen gross, 12 Zeichen klein. */
	T_EQ(OSK_DISPLAY_WIDTH / osk_font_big.advance, OSK_DISPLAY_WIDTH / 6);
	T_EQ(OSK_DISPLAY_WIDTH / osk_font_small.advance, OSK_DISPLAY_WIDTH / 4);

	osk_test_case("Buchstabe mit Zeichen ist nicht derselbe wie ohne");
	T_TRUE(osk_font_index(0x00E4u) != osk_font_index('a')); /* a-Umlaut */
	T_TRUE(osk_font_index(0x00E9u) != osk_font_index('e')); /* e mit Strich */
	for (uint8_t row = 0; row < osk_font_big.rows; row++) {
		if (osk_font_row(&osk_font_big, osk_font_index(0x00E4u), row) !=
		    osk_font_row(&osk_font_big, osk_font_index('a'), row)) {
			osk_test_checks++;
			return;
		}
	}
	T_TRUE(false); /* kein Unterschied gefunden - das Zeichen fehlt */
}
