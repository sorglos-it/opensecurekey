/* Nur unterschriebene Pakete. Falsche Signatur, fehlende Signatur,
 * verstuemmelter Kopf - jedes davon muss abgelehnt werden, und zwar mit
 * dem Grund, der dazu passt.
 *
 * Erste Pruefung ist bewusst die GUTE: ohne sie wuesste man nicht, ob die
 * Ablehnungen von der Signatur kommen oder von einer frueheren Wand. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

#define IMG_LEN 1024

static const uint32_t RUNNING[4] = {0, 0, 1, 0};

static void fill(uint8_t *img, size_t len, uint8_t seed)
{
	for (size_t i = 0; i < len; i++) {
		img[i] = (uint8_t)(seed + i * 7u);
	}
}

void test_signature(void)
{
	static uint8_t pkg[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + IMG_LEN];
	static uint8_t img[IMG_LEN];
	osk_boot_header_t head;
	fake_env_t env;
	size_t len;

	fill(img, sizeof(img), 0x40);
	fake_counter_set(0);
	fake_env_init(&env, OSK_BOARD_OSK_V1, RUNNING);

	bt_case("ein richtig unterschriebenes Paket wird angenommen");
	len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_OSK_V1, (const uint32_t[4]){0, 0, 2, 0}, 1, img,
		       IMG_LEN);
	T_TRUE(len == sizeof(pkg));
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head));
	T_EQ(head.image_len, IMG_LEN);
	T_EQ(head.counter, 1);

	bt_case("ein Byte der Signatur verdreht: abgelehnt");
	pkg[OSK_BOOT_HEADER_LEN + 7] ^= 0x01u;
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_DENIED);
	pkg[OSK_BOOT_HEADER_LEN + 7] ^= 0x01u;
	T_OK(osk_boot_header_check(&env.be, pkg, len, &head)); /* wieder gut */

	bt_case("ein Byte des Kopfes verdreht: Signatur passt nicht mehr");
	pkg[36] ^= 0x10u; /* image_len veraendert */
	T_RES(osk_boot_header_check(&env.be, pkg, len, &head), OSK_ERR_DENIED);
	pkg[36] ^= 0x10u;

	bt_case("Signatur aus Nullen: abgelehnt");
	{
		static uint8_t zeroed[sizeof(pkg)];

		memcpy(zeroed, pkg, len);
		memset(zeroed + OSK_BOOT_HEADER_LEN, 0, OSK_BOOT_SIG_LEN);
		T_RES(osk_boot_header_check(&env.be, zeroed, len, &head), OSK_ERR_DENIED);
	}

	bt_case("Signatur fehlt ganz: zu kurz, also Formatfehler");
	T_RES(osk_boot_header_check(&env.be, pkg, OSK_BOOT_HEADER_LEN, &head), OSK_ERR_FORMAT);
	T_RES(
	    osk_boot_header_check(&env.be, pkg, OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN - 1, &head),
	    OSK_ERR_FORMAT);
	T_RES(osk_boot_header_check(&env.be, pkg, 0, &head), OSK_ERR_FORMAT);

	bt_case("gerade eben lang genug: der Kopf allein reicht zum Urteil");
	T_OK(osk_boot_header_check(&env.be, pkg, OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN, &head));

	bt_case("fremde Kennung: kein Paket");
	{
		static uint8_t bad[sizeof(pkg)];

		memcpy(bad, pkg, len);
		bad[0] = 'X';
		T_RES(osk_boot_header_check(&env.be, bad, len, &head), OSK_ERR_FORMAT);
	}

	bt_case("Kopflaenge und Format werden geglaubt, aber nicht ungeprueft");
	{
		static uint8_t bad[sizeof(pkg)];

		memcpy(bad, pkg, len);
		bad[8] = 80; /* header_len 80 statt 72 */
		T_RES(osk_boot_header_check(&env.be, bad, len, &head), OSK_ERR_FORMAT);
		memcpy(bad, pkg, len);
		bad[10] = 2; /* Format 2 */
		T_RES(osk_boot_header_check(&env.be, bad, len, &head), OSK_ERR_FORMAT);
		memcpy(bad, pkg, len);
		memset(bad + 36, 0, 4); /* Abbildlaenge 0 */
		T_RES(osk_boot_header_check(&env.be, bad, len, &head), OSK_ERR_FORMAT);
	}

	bt_case("ohne Pruefer gibt es kein Urteil, nicht 'in Ordnung'");
	{
		osk_boot_backend_t blind = env.be;

		blind.verify = NULL;
		T_RES(osk_boot_header_check(&blind, pkg, len, &head), OSK_ERR_ARG);
		blind = env.be;
		blind.counter_read = NULL;
		T_RES(osk_boot_header_check(&blind, pkg, len, &head), OSK_ERR_ARG);
		T_RES(osk_boot_header_check(NULL, pkg, len, &head), OSK_ERR_ARG);
		T_RES(osk_boot_header_check(&env.be, NULL, len, &head), OSK_ERR_ARG);
		T_RES(osk_boot_header_check(&env.be, pkg, len, NULL), OSK_ERR_ARG);
	}

	bt_case("ein verdrehtes Abbild faellt am Hash, nicht an der Signatur");
	{
		/* Die Signatur deckt nur den Kopf - ein veraendertes Abbild
		 * kommt also durch osk_boot_header_check() und muss beim
		 * Schreiben auffallen. Genau das wird hier gemessen, damit
		 * niemand die Luecke fuer eine haelt. */
		static fake_flash_t flash;
		static osk_boot_slots_t slots;
		osk_boot_session_t se;
		static uint8_t broken[sizeof(pkg)];

		memcpy(broken, pkg, len);
		broken[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + 5] ^= 0xFFu;
		fake_flash_reset(&flash);
		memset(&slots, 0xFF, sizeof(slots));
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, broken, len, &se));
		T_OK(osk_boot_write(&slots, &se, 0, broken + OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN,
				    IMG_LEN));
		T_RES(osk_boot_finish(&slots, &se), OSK_ERR_FORMAT);

		/* Und danach startet die alte Fassung - nicht das Abbild. */
		{
			osk_boot_state_t st;

			T_OK(osk_boot_state_load(&slots, &st));
			T_TRUE(st.stage != (uint8_t)OSK_BOOT_READY);
			T_EQ(osk_boot_decide(&st, false), OSK_START_PRIMARY);
		}
	}
}
