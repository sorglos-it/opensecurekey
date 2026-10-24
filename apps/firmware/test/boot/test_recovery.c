/* Notfallweg: Bootloader-Betrieb per Tastendruck beim Einstecken.
 *
 * Er muss in JEDEM Zustand offen sein - auch dann, wenn der Zustand des
 * Updates unlesbar ist. Sonst gibt es Faelle, in denen nur noch der
 * Debugger hilft, und den hat niemand im Feld. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"
#include "osk_boot_recovery.h"

void test_recovery(void)
{
	osk_boot_state_t st;
	static osk_boot_slots_t slots;

	bt_case("alle Messungen 'gedrueckt': Bootloader");
	{
		uint8_t all[OSK_BOOT_BUTTON_SAMPLES];

		memset(all, 1, sizeof(all));
		T_TRUE(osk_boot_button_held(all, sizeof(all)));
	}

	bt_case("eine einzige Messung 'nicht gedrueckt': Firmware");
	for (size_t bad = 0; bad < OSK_BOOT_BUTTON_SAMPLES; bad++) {
		uint8_t s[OSK_BOOT_BUTTON_SAMPLES];

		memset(s, 1, sizeof(s));
		s[bad] = 0;
		T_TRUE(!osk_boot_button_held(s, sizeof(s)));
	}

	bt_case("zu wenig gemessen ist kein Ergebnis");
	{
		uint8_t few[OSK_BOOT_BUTTON_SAMPLES - 1];

		memset(few, 1, sizeof(few));
		T_TRUE(!osk_boot_button_held(few, sizeof(few)));
		T_TRUE(!osk_boot_button_held(NULL, OSK_BOOT_BUTTON_SAMPLES));
		T_TRUE(!osk_boot_button_held(few, 0));
	}

	bt_case("nur im Fenster direkt nach dem Start");
	T_TRUE(osk_boot_in_window(0));
	T_TRUE(osk_boot_in_window(OSK_BOOT_BUTTON_WINDOW_MS));
	T_TRUE(!osk_boot_in_window(OSK_BOOT_BUTTON_WINDOW_MS + 1));
	T_TRUE(!osk_boot_in_window(60000));
	/* Das Fenster muss laenger sein als die Messreihe, sonst endet sie
	 * nie innerhalb des Fensters. */
	T_TRUE(OSK_BOOT_BUTTON_WINDOW_MS > OSK_BOOT_BUTTON_SAMPLES * OSK_BOOT_BUTTON_STEP_MS);

	bt_case("die Taste schlaegt jeden Zustand - auch READY");
	{
		int which;

		memset(&slots, 0xFF, sizeof(slots));
		memset(&st, 0, sizeof(st));
		st.stage = (uint8_t)OSK_BOOT_READY;
		st.image_len = 1024;
		st.written = 1024;
		T_OK(osk_boot_state_store(&slots, &st, &which));
		T_OK(osk_boot_state_load(&slots, &st));
		T_EQ(osk_boot_decide(&st, false), OSK_START_SWAP);
		T_EQ(osk_boot_decide(&st, true), OSK_START_RECOVERY);
	}

	bt_case("auch TESTING mit verbrauchten Versuchen");
	memset(&st, 0, sizeof(st));
	st.stage = (uint8_t)OSK_BOOT_TESTING;
	st.tries = OSK_BOOT_MAX_TRIES;
	T_EQ(osk_boot_decide(&st, false), OSK_START_ROLLBACK);
	T_EQ(osk_boot_decide(&st, true), OSK_START_RECOVERY);

	bt_case("und ein Zustand, den es gar nicht gibt");
	/* Kein Zeiger: ohne Taste startet die Firmware, mit Taste der
	 * Bootloader. Beides muss eine Antwort geben. */
	T_EQ(osk_boot_decide(NULL, false), OSK_START_PRIMARY);
	T_EQ(osk_boot_decide(NULL, true), OSK_START_RECOVERY);

	bt_case("beide Abdruecke zerstoert: Bootloader bleibt erreichbar");
	memset(&slots, 0x3C, sizeof(slots));
	T_OK(osk_boot_state_load(&slots, &st));
	T_EQ(osk_boot_decide(&st, true), OSK_START_RECOVERY);
	T_EQ(osk_boot_decide(&st, false), OSK_START_PRIMARY);
}
