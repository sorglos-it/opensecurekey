/* Bildspeicher: Punkte, Linien, Flaechen, Rand. */

#include "osk_gfx.h"
#include "ui_test.h"

static osk_gfx_t fb;

static size_t ink(void)
{
	size_t count = 0;

	for (int16_t y = 0; y < OSK_DISPLAY_HEIGHT; y++) {
		for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
			count += osk_gfx_get(&fb, x, y) ? 1u : 0u;
		}
	}
	return count;
}

void test_gfx(void)
{
	osk_test_case("leer ist leer, voll ist voll");
	osk_gfx_clear(&fb);
	T_EQ(ink(), 0);
	osk_gfx_fill(&fb);
	T_EQ(ink(), (size_t)OSK_DISPLAY_WIDTH * OSK_DISPLAY_HEIGHT);

	osk_test_case("ein Punkt bleibt ein Punkt");
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, 3, 9, true);
	T_TRUE(osk_gfx_get(&fb, 3, 9));
	T_EQ(ink(), 1);
	osk_gfx_set(&fb, 3, 9, false);
	T_EQ(ink(), 0);

	osk_test_case("die Bitlage passt zum Controller: Bit 0 ist oben");
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, 0, 0, true);
	T_EQ(fb.pixels[0], 0x01);
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, 0, 7, true);
	T_EQ(fb.pixels[0], 0x80);
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, 0, 8, true);
	T_EQ(fb.pixels[OSK_DISPLAY_WIDTH], 0x01);

	osk_test_case("ausserhalb wird nichts geschrieben und nichts gelesen");
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, -1, 0, true);
	osk_gfx_set(&fb, 0, -1, true);
	osk_gfx_set(&fb, OSK_DISPLAY_WIDTH, 0, true);
	osk_gfx_set(&fb, 0, OSK_DISPLAY_HEIGHT, true);
	osk_gfx_set(&fb, 30000, 30000, true);
	T_EQ(ink(), 0);
	T_TRUE(!osk_gfx_get(&fb, -5, -5));
	T_TRUE(!osk_gfx_get(&fb, OSK_DISPLAY_WIDTH, OSK_DISPLAY_HEIGHT));

	osk_test_case("NULL ist kein Absturz");
	osk_gfx_clear(NULL);
	osk_gfx_fill(NULL);
	osk_gfx_set(NULL, 0, 0, true);
	T_TRUE(!osk_gfx_get(NULL, 0, 0));

	osk_test_case("Linien sind so lang wie gesagt");
	osk_gfx_clear(&fb);
	osk_gfx_hline(&fb, 0, 0, 5, true);
	T_EQ(ink(), 5);
	osk_gfx_clear(&fb);
	osk_gfx_vline(&fb, 1, 2, 7, true);
	T_EQ(ink(), 7);

	osk_test_case("Linien ueber den Rand werden abgeschnitten, nicht umgebrochen");
	osk_gfx_clear(&fb);
	osk_gfx_hline(&fb, (int16_t)(OSK_DISPLAY_WIDTH - 2), 0, 10, true);
	T_EQ(ink(), 2);
	T_TRUE(!osk_gfx_get(&fb, 0, 1));

	osk_test_case("Rahmen und Flaeche");
	osk_gfx_clear(&fb);
	osk_gfx_rect(&fb, 0, 0, 4, 4, true);
	T_EQ(ink(), 12); /* 4*4 ohne die vier inneren Punkte */
	osk_gfx_clear(&fb);
	osk_gfx_fill_rect(&fb, 2, 2, 3, 4, true);
	T_EQ(ink(), 12);
	osk_gfx_rect(&fb, 0, 0, 0, 5, true);
	T_EQ(ink(), 12); /* Breite 0 zeichnet nichts */

	osk_test_case("Umkehren dreht nur die Flaeche");
	osk_gfx_clear(&fb);
	osk_gfx_fill_rect(&fb, 0, 0, 4, 4, true);
	osk_gfx_invert_rect(&fb, 0, 0, 2, 2);
	T_EQ(ink(), 12);
	T_TRUE(!osk_gfx_get(&fb, 0, 0));
	T_TRUE(osk_gfx_get(&fb, 3, 3));
}
