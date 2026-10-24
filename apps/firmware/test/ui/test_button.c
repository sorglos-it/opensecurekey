/* Taster: entprellen, kurz, lang, doppelt.
 *
 * Die Zeit ist hier eine Zahl, darum ist jeder Fall wiederholbar - am Geraet
 * waere er von der Hand des Nutzers abhaengig. */

#include "osk_button.h"
#include "ui_test.h"

/* Haelt den Taster `ms` lang in einem Zustand und liefert das letzte
 * Ereignis, das dabei herauskam. Die Uhr geht in Schritten von 5 ms. */
static osk_btn_event_t hold(osk_button_t *btn, bool pressed, uint32_t ms, uint32_t *now)
{
	osk_btn_event_t last = OSK_BTN_NONE;

	for (uint32_t i = 0; i < ms; i += 5) {
		osk_btn_event_t event = osk_button_feed(btn, pressed, *now);

		if (event != OSK_BTN_NONE) {
			last = event;
		}
		*now += 5;
	}
	return last;
}

void test_button(void)
{
	osk_button_t btn;
	uint32_t now = 10000;

	osk_test_case("ein kurzer Druck wird ein kurzer Druck");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 100, &now), OSK_BTN_NONE); /* beim Druecken noch nichts */
	T_EQ(hold(&btn, false, OSK_BTN_DOUBLE_MS + 50, &now), OSK_BTN_SHORT);

	osk_test_case("Prellen ist kein Druck");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 10, &now), OSK_BTN_NONE); /* unter 15 ms */
	T_EQ(hold(&btn, false, 500, &now), OSK_BTN_NONE);

	osk_test_case("ein langer Druck meldet sich, sobald die Zeit reicht");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, OSK_BTN_LONG_MS - 100, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, true, 200, &now), OSK_BTN_LONG);
	/* Beim Loslassen kommt nichts mehr - der lange Druck ist gezaehlt. */
	T_EQ(hold(&btn, false, 1000, &now), OSK_BTN_NONE);

	osk_test_case("zwei kurze Druecke werden ein Doppeldruck");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 60, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, 100, &now), OSK_BTN_NONE); /* im Fenster */
	T_EQ(hold(&btn, true, 60, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, 100, &now), OSK_BTN_DOUBLE);
	T_EQ(hold(&btn, false, 1000, &now), OSK_BTN_NONE); /* und nichts hinterher */

	osk_test_case("zwei Druecke mit Pause sind zwei kurze Druecke");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 60, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, OSK_BTN_DOUBLE_MS + 50, &now), OSK_BTN_SHORT);
	T_EQ(hold(&btn, true, 60, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, OSK_BTN_DOUBLE_MS + 50, &now), OSK_BTN_SHORT);

	osk_test_case("kurz, dann lang: der lange gilt");
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 60, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, 100, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, true, OSK_BTN_LONG_MS + 50, &now), OSK_BTN_LONG);
	T_EQ(hold(&btn, false, 1000, &now), OSK_BTN_NONE); /* kein kurzer hinterher */

	osk_test_case("der Ueberlauf der Uhr nach 49 Tagen stoert nicht");
	now = 0xFFFFFF00u;
	osk_button_init(&btn, now);
	T_EQ(hold(&btn, true, 100, &now), OSK_BTN_NONE);
	T_EQ(hold(&btn, false, OSK_BTN_DOUBLE_MS + 50, &now), OSK_BTN_SHORT);
	T_TRUE(now < 0xFFFFFF00u); /* die Uhr ist wirklich uebergelaufen */

	osk_test_case("NULL ist kein Absturz");
	osk_button_init(NULL, 0);
	T_EQ(osk_button_feed(NULL, true, 0), OSK_BTN_NONE);
}
