/* Ein Abbild fuer ein anderes Brett wird abgelehnt - auch wenn es richtig
 * unterschrieben ist. Die Anschluesse liegen anders; eine Firmware, die
 * den falschen Anschluss fuer den Taster haelt, kann nichts mehr freigeben,
 * und dann ist der Stick zu. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

#define IMG_LEN 128

static const uint32_t V[4] = {0, 0, 2, 0};
static const uint32_t RUNNING[4] = {0, 0, 1, 0};

void test_board(void)
{
	static uint8_t pkg[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + IMG_LEN];
	static uint8_t img[IMG_LEN];
	osk_boot_header_t head;
	fake_env_t env;
	size_t len;

	memset(img, 0xA5, sizeof(img));
	fake_counter_set(0);
	fake_env_init(&env, OSK_BOARD_OSK_V1, RUNNING);

	bt_case("Abbild fuer dieses Brett: angenommen");
	len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_OSK_V1, V, 1, img, IMG_LEN);
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));
	T_EQ(head.board_id, (uint32_t)OSK_BOARD_OSK_V1);

	bt_case("Abbild fuer das DK, richtig unterschrieben: UNSUPPORTED");
	/* Der Grund ist wichtig: UNSUPPORTED und nicht DENIED beweist, dass
	 * die Signatur geprueft und als gut befunden wurde - die Ablehnung
	 * kommt also wirklich von der Brettkennung und nicht von einer
	 * frueheren Wand. */
	len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_NRF52840DK, V, 1, img, IMG_LEN);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_UNSUPPORTED);

	bt_case("Abbild fuer den Dongle: UNSUPPORTED");
	len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_NRF52840DONGLE, V, 1, img, IMG_LEN);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_UNSUPPORTED);

	bt_case("Brettkennung 0 ist nie gueltig - auch nicht auf einem Brett ohne Kennung");
	{
		osk_boot_backend_t unknown = env.be;

		len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_UNKNOWN, V, 1, img, IMG_LEN);
		T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_UNSUPPORTED);
		unknown.board_id = OSK_BOARD_UNKNOWN;
		T_RES(osk_boot_header_check(&unknown, pkg, len, &head), OSK_ERR_UNSUPPORTED);
	}

	bt_case("unbekannte, hohe Kennung: abgelehnt, nicht geraten");
	len = fake_pkg(pkg, sizeof(pkg), 0xDEADBEEFu, V, 1, img, IMG_LEN);
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_UNSUPPORTED);

	bt_case("dasselbe Abbild auf dem passenden Brett: angenommen");
	{
		osk_boot_backend_t dk = env.be;

		dk.board_id = OSK_BOARD_NRF52840DK;
		len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_NRF52840DK, V, 1, img, IMG_LEN);
		T_OK(osk_boot_header_check(&dk, pkg, len, &head));
	}

	bt_case("Brett wird VOR dem Rueckschritt geprueft - und nach der Signatur");
	{
		/* Ein altes Paket fuers falsche Brett: der Grund muss die
		 * Brettkennung sein, damit die Meldung am Geraet stimmt. */
		fake_counter_set(5);
		len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_NRF52840DK,
			       (const uint32_t[4]){0, 0, 0, 1}, 1, img, IMG_LEN);
		T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_UNSUPPORTED);
		/* Und falsch unterschrieben schlaegt beides. */
		pkg[OSK_BOOT_HEADER_LEN] ^= 0xFFu;
		T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_DENIED);
		fake_counter_set(0);
	}
}
