/* Treiber der Anzeige: welche Bytes gehen hinaus, und landet das Bild an der
 * richtigen Stelle?
 *
 * Geprueft ist damit, was der Treiber SENDET. Was ein echter Controller
 * daraus macht, ist nicht geprueft - es hat noch keiner angeschlossen. */

#include "fake_panel.h"
#include "osk_display.h"
#include "ui_test.h"

static osk_gfx_t fb;

void test_panel(void)
{
	osk_test_case("ohne Bus keine Anzeige");
	T_RES(osk_display_init(NULL), OSK_ERR_ARG);
	T_TRUE(!osk_display_ready());
	T_RES(osk_display_flush(&fb), OSK_ERR_INTERNAL);
	T_RES(osk_display_contrast(0x80), OSK_ERR_INTERNAL);
	T_RES(osk_display_inverted(true), OSK_ERR_INTERNAL);

	osk_test_case("Start: Strom an, Reset, Startfolge");
	fake_panel_reset();
	T_OK(osk_display_init(&fake_panel_port()->display));
	T_TRUE(osk_display_ready());
	T_TRUE(fake_panel_powered());
	T_TRUE(fake_panel_reset_done());
	T_TRUE(fake_panel_sent_cmd(0xAE)); /* erst aus */
	T_TRUE(fake_panel_sent_cmd(0xAF)); /* am Ende an */
	T_TRUE(fake_panel_sent_cmd(0x8D)); /* Ladepumpe */
	T_TRUE(fake_panel_sent_cmd(0xA8)); /* Zeilenzahl */
	T_EQ(fake_panel_cmd(0), 0xAE);
	T_EQ(fake_panel_cmd(fake_panel_cmd_count() - 1), 0xAF);

	osk_test_case("die Zeilenzahl kommt aus der Panelgroesse");
	for (size_t i = 0; i + 1 < fake_panel_cmd_count(); i++) {
		if (fake_panel_cmd(i) == 0xA8) {
			T_EQ(fake_panel_cmd(i + 1), OSK_DISPLAY_HEIGHT - 1);
		}
	}

	osk_test_case("ein Bild geht seitenweise hinaus");
	osk_gfx_clear(&fb);
	osk_gfx_set(&fb, 0, 0, true);
	osk_gfx_set(&fb, (int16_t)(OSK_DISPLAY_WIDTH - 1), (int16_t)(OSK_DISPLAY_HEIGHT - 1), true);
	T_OK(osk_display_flush(&fb));
	T_EQ(fake_panel_flushes(), 1);
	T_EQ(fake_panel_data_bytes(), (size_t)OSK_DISPLAY_WIDTH * OSK_DISPLAY_PAGES);

	osk_test_case("der Spaltenversatz stimmt - sonst steht alles daneben");
	/* Die Attrappe zieht den Versatz wieder ab. Kommt das Bild Punkt fuer
	 * Punkt zurueck, war er richtig. */
	T_TRUE(fake_panel_pixel(0, 0));
	T_TRUE(
	    fake_panel_pixel((int16_t)(OSK_DISPLAY_WIDTH - 1), (int16_t)(OSK_DISPLAY_HEIGHT - 1)));
	T_EQ(fake_panel_ink(), 2);
	for (uint8_t page = 0; page < OSK_DISPLAY_PAGES; page++) {
		bool found = false;

		for (size_t i = 0; i + 2 < fake_panel_cmd_count(); i++) {
			if (fake_panel_cmd(i) == (uint8_t)(0xB0u + page)) {
				/* Spalte low und high zusammen ergeben den Versatz. */
				uint8_t low = (uint8_t)(fake_panel_cmd(i + 1) & 0x0Fu);
				uint8_t high = (uint8_t)(fake_panel_cmd(i + 2) & 0x0Fu);

				T_EQ((uint8_t)(low | (uint8_t)(high << 4)), OSK_DISPLAY_COL_OFFSET);
				found = true;
			}
		}
		T_TRUE(found);
	}

	osk_test_case("Helligkeit und Umkehren");
	T_OK(osk_display_contrast(0x40));
	T_TRUE(fake_panel_sent_cmd(0x81));
	T_OK(osk_display_inverted(true));
	T_TRUE(fake_panel_sent_cmd(0xA7));
	T_OK(osk_display_inverted(false));
	T_TRUE(fake_panel_sent_cmd(0xA6));
	T_RES(osk_display_flush(NULL), OSK_ERR_ARG);

	osk_test_case("Schlafen nimmt den Strom weg");
	T_OK(osk_display_sleep());
	T_TRUE(!fake_panel_powered());
	T_TRUE(!osk_display_ready());
	T_RES(osk_display_flush(&fb), OSK_ERR_INTERNAL);

	osk_test_case("ein gestoerter Bus wird gemeldet, nicht verschluckt");
	fake_panel_reset();
	fake_panel_fail_bus(true);
	T_RES(osk_display_init(&fake_panel_port()->display), OSK_ERR_IO);
	T_TRUE(!osk_display_ready());
	fake_panel_fail_bus(false);
	fake_panel_reset();
	T_OK(osk_display_init(&fake_panel_port()->display));
	fake_panel_fail_bus(true);
	T_RES(osk_display_flush(&fb), OSK_ERR_IO);
	fake_panel_fail_bus(false);
	T_OK(osk_display_sleep());
}
