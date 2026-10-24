/* Der Tresor bleibt erhalten. Ein Update darf die Tresor-Partition nicht
 * anfassen - kein Schreiben, kein Loeschen, kein Byte.
 *
 * Gemessen wird zweierlei:
 *  1. Die Grenze selbst, mit den echten Adressen aus boot/pm_static.yml.
 *  2. Ein vollstaendiger Durchlauf, bei dem jeder Zugriff durch die Grenze
 *     geht und der Tresor danach Byte fuer Byte derselbe ist.
 *
 * Die Wanderung des Datenformats (osk_store_migrate) ist ausdruecklich NICHT
 * Sache des Updates: sie laeuft beim ersten Start der neuen Firmware. Das
 * Update weiss nicht, ob es ueberhaupt bestaetigt wird - es darf den Tresor
 * also gar nicht umbauen. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

/* Die Adressen aus boot/pm_static.yml. Stehen hier noch einmal, damit eine
 * Verschiebung dort diesen Satz rot macht, statt still durchzugehen. */
#define SEC_START 0x000000u
#define SEC_LEN 0x0f0000u
#define SPARE_START 0x0f0000u
#define SPARE_LEN 0x0f0000u
#define VAULT_START 0x1e0000u
#define VAULT_LEN 0x1e20000u
#define LOG_START 0x2000000u
#define DRIVE_START 0x2200000u

static const osk_boot_area_t ALLOWED[] = {
    {SEC_START, SEC_LEN},
};

/* --- Durchlauf mit Grenzwache -------------------------------------------- */

#define VAULT_PROBE 4096u
#define IMG_LEN 4096

typedef struct {
	fake_flash_t slot;
	uint8_t vault[VAULT_PROBE];
	uint32_t guard_calls;
	uint32_t refused;
} guarded_t;

static guarded_t g;

/* Der Flash-Treiber, wie er im Geraet aussieht: absolute Adressen, und VOR
 * jedem Zugriff die Frage, ob der Bereich dem Update gehoert. */
static osk_result_t guarded_write_abs(uint32_t addr, const uint8_t *data, size_t len)
{
	g.guard_calls++;
	if (!osk_boot_may_write(ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0]), addr,
				(uint32_t)len)) {
		g.refused++;
		return OSK_ERR_DENIED;
	}
	if (addr >= VAULT_START) {
		/* Darf nicht vorkommen - wenn doch, soll der Satz es zeigen. */
		memset(g.vault, 0, sizeof(g.vault));
		return OSK_ERR_INTERNAL;
	}
	return fake_flash_write(&g.slot, addr - SEC_START, data, len);
}

static osk_result_t guarded_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len)
{
	(void)ctx;
	return guarded_write_abs(SEC_START + offset, data, len);
}

static osk_result_t guarded_erase(void *ctx, uint32_t offset, uint32_t len)
{
	(void)ctx;
	g.guard_calls++;
	if (!osk_boot_may_write(ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0]), SEC_START + offset,
				len)) {
		g.refused++;
		return OSK_ERR_DENIED;
	}
	return fake_flash_erase(&g.slot, offset, len);
}

void test_vault(void)
{
	static uint8_t pkg[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + IMG_LEN];
	static uint8_t img[IMG_LEN];
	static uint8_t vault_before[VAULT_PROBE];
	fake_env_t env;
	osk_boot_session_t se;
	static osk_boot_slots_t slots;
	size_t len;

	bt_case("der zweite Platz darf beschrieben werden");
	T_TRUE(osk_boot_may_write(ALLOWED, 1, SEC_START, 4096));
	T_TRUE(osk_boot_may_write(ALLOWED, 1, SEC_START, SEC_LEN));
	T_TRUE(osk_boot_may_write(ALLOWED, 1, SEC_START + SEC_LEN - 1, 1));

	bt_case("der Tresor nicht - kein Byte davon");
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, VAULT_START, 1));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, VAULT_START, 4096));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, VAULT_START + VAULT_LEN - 1, 1));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, VAULT_START + VAULT_LEN / 2, 65536));

	bt_case("auch das Protokoll und das Laufwerk gehoeren nicht zum Update");
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, LOG_START, 256));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, DRIVE_START, 256));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, SPARE_START, 256));

	bt_case("ein Zugriff, der am Ende des Platzes ueberlaeuft, wird abgelehnt");
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, SEC_START + SEC_LEN - 1, 2));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, SEC_START + SEC_LEN, 1));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, SEC_START, SEC_LEN + 1));

	bt_case("Ueberlauf des Adressraums hebelt die Grenze nicht aus");
	/* start + len kippt um und laege wieder im erlaubten Bereich. */
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, 0xFFFFFFF0u, 0x20u));
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, 0xFFFFFFFFu, 2u));

	bt_case("Laenge 0, keine Bereiche, kein Zeiger: nein, nicht ja");
	T_TRUE(!osk_boot_may_write(ALLOWED, 1, SEC_START, 0));
	T_TRUE(!osk_boot_may_write(ALLOWED, 0, SEC_START, 16));
	T_TRUE(!osk_boot_may_write(NULL, 1, SEC_START, 16));

	bt_case("ein Bereich der Laenge 0 erlaubt nichts");
	{
		const osk_boot_area_t empty[] = {{SEC_START, 0}};

		T_TRUE(!osk_boot_may_write(empty, 1, SEC_START, 1));
	}

	bt_case("ein vollstaendiges Update laesst den Tresor Byte fuer Byte gleich");
	for (size_t i = 0; i < IMG_LEN; i++) {
		img[i] = (uint8_t)(i * 11u + 3u);
	}
	for (size_t i = 0; i < VAULT_PROBE; i++) {
		g.vault[i] = (uint8_t)(i * 17u + 7u);
	}
	memcpy(vault_before, g.vault, VAULT_PROBE);
	g.guard_calls = 0;
	g.refused = 0;
	fake_flash_reset(&g.slot);
	fake_counter_set(0);
	fake_env_init(&env, OSK_BOARD_OSK_V1, (const uint32_t[4]){0, 0, 1, 0});
	memset(&slots, 0xFF, sizeof(slots));
	len = fake_pkg(pkg, sizeof(pkg), OSK_BOARD_OSK_V1, (const uint32_t[4]){0, 0, 2, 0}, 1, img,
		       IMG_LEN);

	memset(&se, 0, sizeof(se));
	se.ctx = &g;
	se.slot_len = FAKE_SLOT_LEN;
	se.erase = guarded_erase;
	se.write = guarded_write;
	T_OK(osk_boot_begin(&env.be, &slots, pkg, len, &se));
	for (uint32_t off = 0; off < IMG_LEN; off += 1024) {
		T_OK(osk_boot_write(&slots, &se, off,
				    pkg + OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + off, 1024));
	}
	T_OK(osk_boot_finish(&slots, &se));

	/* Die Grenze wurde wirklich befragt - sonst waere der Vergleich
	 * unten wertlos, weil nie jemand in ihre Naehe kam. */
	T_TRUE(g.guard_calls > 0);
	T_EQ(g.refused, 0);
	T_MEM(g.vault, vault_before, VAULT_PROBE);
	T_MEM(g.slot.data, pkg + OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN, IMG_LEN);

	bt_case("und die Grenze haelt auch den Zugriff auf, der sie treffen soll");
	/* Gegenprobe: ohne sie wuesste man nicht, ob die Wache ueberhaupt
	 * etwas ablehnen kann. */
	T_RES(guarded_write_abs(VAULT_START, img, 16), OSK_ERR_DENIED);
	T_EQ(g.refused, 1);
	T_MEM(g.vault, vault_before, VAULT_PROBE);

	bt_case("der Zustand des Updates liegt nicht im Tresor");
	/* Er liegt im internen Flash (Partition osk_boot_state), und das ist
	 * hier daran zu sehen, dass der ganze Durchlauf oben ohne einen
	 * einzigen Zugriff auf den externen Tresorbereich auskam. */
	T_EQ(g.refused, 1); /* nur die Gegenprobe, nichts aus dem Durchlauf */
}
