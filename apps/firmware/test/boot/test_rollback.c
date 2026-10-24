/* Rueckschritt gesperrt. Der Zaehler liegt im SE050, nicht im Flash -
 * und das ist hier gemessen, nicht behauptet: nach dem Loeschen des
 * gesamten Zustands im Flash wird ein altes Paket weiter abgelehnt. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

#define IMG_LEN 256

static const uint32_t V_ALT[4] = {0, 0, 1, 0};
static const uint32_t V_NEU[4] = {0, 0, 2, 0};

static size_t build(uint8_t *buf, size_t cap, const uint32_t version[4], uint32_t counter,
		    uint8_t *img)
{
	for (size_t i = 0; i < IMG_LEN; i++) {
		img[i] = (uint8_t)(i ^ 0x5Au);
	}
	return fake_pkg(buf, cap, OSK_BOARD_OSK_V1, version, counter, img, IMG_LEN);
}

void test_rollback(void)
{
	static uint8_t pkg[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + IMG_LEN];
	static uint8_t img[IMG_LEN];
	osk_boot_header_t head;
	fake_env_t env;
	size_t len;

	fake_env_init(&env, OSK_BOARD_OSK_V1, V_ALT);

	bt_case("hoeherer Zaehler: angenommen");
	fake_counter_set(3);
	len = build(pkg, sizeof(pkg), V_NEU, 4, img);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));

	bt_case("gleicher Zaehler, hoehere Fassung: angenommen");
	fake_counter_set(3);
	len = build(pkg, sizeof(pkg), V_NEU, 3, img);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));

	bt_case("gleicher Zaehler, gleiche Fassung: abgelehnt");
	len = build(pkg, sizeof(pkg), V_ALT, 3, img);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_EXISTS);

	bt_case("gleicher Zaehler, aeltere Fassung: abgelehnt");
	len = build(pkg, sizeof(pkg), (const uint32_t[4]){0, 0, 0, 9}, 3, img);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_EXISTS);

	bt_case("kleinerer Zaehler: abgelehnt, auch mit hoeherer Fassung");
	len = build(pkg, sizeof(pkg), (const uint32_t[4]){9, 9, 9, 9}, 2, img);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_EXISTS);

	bt_case("Zaehler 0 bei Zaehlerstand 0: nur mit hoeherer Fassung");
	fake_counter_set(0);
	len = build(pkg, sizeof(pkg), V_ALT, 0, img);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_EXISTS);
	len = build(pkg, sizeof(pkg), V_NEU, 0, img);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));

	bt_case("Flash ganz geloescht: das alte Paket bleibt abgelehnt");
	{
		/* Das ist der Grund, warum der Zaehler nicht im Flash liegt.
		 * Hier wird der gesamte Zustand im Flash auf 0xFF gesetzt -
		 * also geloescht - und das alte Paket danach erneut
		 * angeboten. */
		static osk_boot_slots_t slots;
		osk_boot_state_t st;

		fake_counter_set(7);
		memset(&slots, 0xFF, sizeof(slots));
		T_OK(osk_boot_state_load(&slots, &st));
		T_EQ(st.stage, (uint8_t)OSK_BOOT_IDLE); /* der Flash weiss nichts mehr */
		T_EQ(fake_counter_get(), 7);		/* der Chip schon */
		len = build(pkg, sizeof(pkg), (const uint32_t[4]){9, 9, 9, 9}, 6, img);
		T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_EXISTS);
	}

	bt_case("Zwischenpruefung: derselbe Zustand nimmt ein neues Paket an");
	/* Ohne diesen Fall koennte die Ablehnung oben auch von einer anderen
	 * Wand kommen - etwa einem kaputten Zaehler. */
	len = build(pkg, sizeof(pkg), (const uint32_t[4]){9, 9, 9, 9}, 8, img);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));

	bt_case("der Chip antwortet nicht: kein Urteil, nicht 'in Ordnung'");
	fake_counter_break(true);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_IO);
	fake_counter_break(false);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));

	bt_case("der Zaehler sinkt nie, auch nicht auf Befehl");
	fake_counter_set(5);
	T_RES(fake_counter_raise(4), OSK_ERR_EXISTS);
	T_RES(fake_counter_raise(5), OSK_ERR_EXISTS);
	T_OK(fake_counter_raise(6));
	T_EQ(fake_counter_get(), 6);

	bt_case("Fassungen vergleichen: Stelle fuer Stelle von links");
	T_EQ(osk_boot_version_cmp((const uint32_t[4]){0, 0, 1, 0}, (const uint32_t[4]){0, 0, 1, 0}),
	     0);
	T_TRUE(osk_boot_version_cmp((const uint32_t[4]){0, 0, 1, 0},
				    (const uint32_t[4]){0, 0, 0, 99}) > 0);
	T_TRUE(osk_boot_version_cmp((const uint32_t[4]){0, 1, 0, 0},
				    (const uint32_t[4]){1, 0, 0, 0}) < 0);
	T_TRUE(osk_boot_version_cmp((const uint32_t[4]){0, 0, 1, 1},
				    (const uint32_t[4]){0, 0, 1, 2}) < 0);
}
