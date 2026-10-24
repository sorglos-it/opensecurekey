/* Abbruch an jeder Stelle des Updates - und danach muss das Geraet noch
 * starten. Das ist die eigentliche Anforderung; alles andere in diesem
 * Ordner dient ihr.
 *
 * Nachgestellt wird der Abbruch auf zwei Weisen, weil beide vorkommen:
 *  - Der Flash nimmt ab Byte N nichts mehr an (Stecker gezogen waehrend
 *    eines Schreibvorgangs). Dann ist ein Teil drin und ein Teil nicht.
 *  - Es wird einfach aufgehoert (Strom weg zwischen zwei Aufrufen). Dann
 *    steht der Zustand, wie er zuletzt geschrieben wurde.
 *
 * Nach JEDEM Abbruch wird dieselbe Frage gestellt: was startet der
 * Bootloader? Eine Antwort gibt es immer - "nichts" ist kein erlaubtes
 * Ergebnis, und genau darauf wird geprueft. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

#define IMG_LEN 8192
#define CHUNK 512

static const uint32_t RUNNING[4] = {0, 0, 1, 0};
static const uint32_t NEXT[4] = {0, 0, 2, 0};

static uint8_t g_pkg[OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + IMG_LEN];
static uint8_t g_img[IMG_LEN];
static size_t g_len;

static const uint8_t *image_bytes(void)
{
	return g_pkg + OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN;
}

/* Das Geraet startet immer irgendetwas - und nie das halbe neue Abbild. */
static void startet_noch(const osk_boot_slots_t *slots, osk_boot_start_t erwartet)
{
	osk_boot_state_t st;
	osk_boot_start_t got;

	T_OK(osk_boot_state_load(slots, &st));
	got = osk_boot_decide(&st, false);
	T_EQ(got, erwartet);
	/* Und mit gedrueckter Taste kommt man in jedem Zustand in den
	 * Bootloader - auch das ist ein Weg, der immer offen sein muss. */
	T_EQ(osk_boot_decide(&st, true), OSK_START_RECOVERY);
}

static void prepare(fake_env_t *env, fake_flash_t *flash, osk_boot_slots_t *slots)
{
	for (size_t i = 0; i < IMG_LEN; i++) {
		g_img[i] = (uint8_t)(i * 3u + 1u);
	}
	fake_counter_set(1);
	fake_env_init(env, OSK_BOARD_OSK_V1, RUNNING);
	fake_flash_reset(flash);
	memset(slots, 0xFF, sizeof(*slots));
	g_len = fake_pkg(g_pkg, sizeof(g_pkg), OSK_BOARD_OSK_V1, NEXT, 2, g_img, IMG_LEN);
}

/* Schreibt das Abbild in Stuecken, hoert aber nach `chunks` Stuecken auf.
 * Gibt zurueck, ob alles durchlief. */
static bool write_chunks(osk_boot_slots_t *slots, osk_boot_session_t *se, int chunks)
{
	for (int i = 0; i < chunks; i++) {
		uint32_t off = (uint32_t)i * CHUNK;

		if (off >= IMG_LEN) {
			return true;
		}
		if (osk_boot_write(slots, se, off, image_bytes() + off, CHUNK) != OSK_OK) {
			return false;
		}
	}
	return true;
}

void test_abort(void)
{
	static fake_flash_t flash;
	static osk_boot_slots_t slots;
	fake_env_t env;
	osk_boot_session_t se;

	bt_case("Zwischenpruefung: der ganze Weg laeuft durch");
	/* Ohne diesen Fall koennte jeder spaetere "startet die alte Fassung"
	 * daran liegen, dass das Update ueberhaupt nicht funktioniert. */
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
	T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
	T_OK(osk_boot_finish(&slots, &se));
	startet_noch(&slots, OSK_START_SWAP);
	T_MEM(flash.data, image_bytes(), IMG_LEN);

	bt_case("Abbruch VOR dem Anfang: die alte Fassung startet");
	prepare(&env, &flash, &slots);
	startet_noch(&slots, OSK_START_PRIMARY);

	bt_case("Abbruch beim Leeren des zweiten Platzes");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	flash.erase_fails = true;
	T_RES(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se), OSK_ERR_IO);
	T_EQ(flash.erases, 0);
	startet_noch(&slots, OSK_START_PRIMARY);

	bt_case("Abbruch nach jedem einzelnen Stueck: immer die alte Fassung");
	for (int chunks = 0; chunks <= IMG_LEN / CHUNK; chunks++) {
		prepare(&env, &flash, &slots);
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
		write_chunks(&slots, &se, chunks);
		/* Jetzt ist der Strom weg. Kein finish(), kein abort() -
		 * so sieht ein gezogener Stecker aus. */
		if (chunks >= IMG_LEN / CHUNK) {
			/* Alle Stuecke da, aber nicht bestaetigt: trotzdem
			 * noch kein READY, weil finish() nie lief. */
			startet_noch(&slots, OSK_START_PRIMARY);
		} else {
			startet_noch(&slots, OSK_START_PRIMARY);
		}
	}

	bt_case("Abbruch MITTEN in einem Schreibvorgang, an vielen Stellen");
	for (uint32_t cut = 1; cut < IMG_LEN; cut += 257) {
		prepare(&env, &flash, &slots);
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
		flash.fail_after = cut;
		write_chunks(&slots, &se, IMG_LEN / CHUNK);
		/* Der Flash hat ab `cut` nichts mehr angenommen - ein Teil
		 * des Abbilds steht drin, der Rest nicht. */
		T_TRUE(flash.written_bytes <= cut);
		startet_noch(&slots, OSK_START_PRIMARY);
		/* Und ein nachtraegliches finish() macht daraus kein READY. */
		T_TRUE(osk_boot_finish(&slots, &se) != OSK_OK);
		startet_noch(&slots, OSK_START_PRIMARY);
	}

	bt_case("Loecher und Rueckspruenge werden abgelehnt");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
	T_OK(osk_boot_write(&slots, &se, 0, image_bytes(), CHUNK));
	T_RES(osk_boot_write(&slots, &se, 2 * CHUNK, image_bytes(), CHUNK), OSK_ERR_FORMAT);
	T_RES(osk_boot_write(&slots, &se, 0, image_bytes(), CHUNK), OSK_ERR_FORMAT);
	T_RES(osk_boot_write(&slots, &se, CHUNK, image_bytes(), IMG_LEN), OSK_ERR_FORMAT);
	startet_noch(&slots, OSK_START_PRIMARY);

	bt_case("schreiben ohne Anfang geht nicht");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	T_RES(osk_boot_write(&slots, &se, 0, image_bytes(), CHUNK), OSK_ERR_NOT_FOUND);
	T_RES(osk_boot_finish(&slots, &se), OSK_ERR_NOT_FOUND);

	bt_case("zu grosses Abbild: abgelehnt, bevor etwas uebertragen wird");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	se.slot_len = IMG_LEN - 1;
	T_RES(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se), OSK_ERR_FULL);
	T_EQ(flash.erases, 0);
	startet_noch(&slots, OSK_START_PRIMARY);

	bt_case("Abbruch NACH dem Pruefen, VOR dem Umschalten: wieder dasselbe Urteil");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
	T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
	T_OK(osk_boot_finish(&slots, &se));
	startet_noch(&slots, OSK_START_SWAP);
	startet_noch(&slots, OSK_START_SWAP); /* nochmal eingesteckt: gleich */

	bt_case("READY mit unvollstaendigem Abbild wird nicht umgeschaltet");
	{
		osk_boot_state_t st;
		int which;

		T_OK(osk_boot_state_load(&slots, &st));
		st.written = IMG_LEN - 1;
		T_OK(osk_boot_state_store(&slots, &st, &which));
		startet_noch(&slots, OSK_START_PRIMARY);
	}

	bt_case("Abbruch waehrend des Umschaltens: Startversuche werden gezaehlt");
	{
		/* Das Umschalten selbst macht MCUboot. Hier wird gemessen,
		 * was der Zustand danach hergibt: die neue Fassung darf
		 * dreimal starten, ohne sich zu melden, dann geht es
		 * zurueck. */
		osk_boot_state_t st;
		int which;

		prepare(&env, &flash, &slots);
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
		T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
		T_OK(osk_boot_finish(&slots, &se));

		/* MCUboot hat umgeschaltet: */
		T_OK(osk_boot_state_load(&slots, &st));
		st.stage = (uint8_t)OSK_BOOT_TESTING;
		st.tries = 0;
		T_OK(osk_boot_state_store(&slots, &st, &which));

		for (int try = 1; try < OSK_BOOT_MAX_TRIES; try++) {
			T_OK(osk_boot_mark_try(&slots));
			startet_noch(&slots, OSK_START_PRIMARY);
		}
		T_OK(osk_boot_mark_try(&slots));
		startet_noch(&slots, OSK_START_ROLLBACK);
		/* Und es bleibt dabei, egal wie oft noch eingesteckt wird. */
		T_OK(osk_boot_mark_try(&slots));
		startet_noch(&slots, OSK_START_ROLLBACK);
	}

	bt_case("waehrend TESTING darf nicht abgebrochen werden");
	T_RES(osk_boot_abort(&slots, &se), OSK_ERR_DENIED);
	startet_noch(&slots, OSK_START_ROLLBACK);

	bt_case("die neue Fassung meldet sich: bestaetigt, Zaehler steigt");
	{
		osk_boot_state_t st;
		int which;

		prepare(&env, &flash, &slots);
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
		T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
		T_OK(osk_boot_finish(&slots, &se));
		T_OK(osk_boot_state_load(&slots, &st));
		st.stage = (uint8_t)OSK_BOOT_TESTING;
		T_OK(osk_boot_state_store(&slots, &st, &which));

		T_EQ(fake_counter_get(), 1); /* vor der Bestaetigung unveraendert */
		T_OK(osk_boot_confirm(&env.be, &slots));
		T_EQ(fake_counter_get(), 2); /* erst jetzt */
		startet_noch(&slots, OSK_START_PRIMARY);

		bt_case("zweimal bestaetigen richtet nichts an");
		T_OK(osk_boot_confirm(&env.be, &slots));
		T_EQ(fake_counter_get(), 2);
		startet_noch(&slots, OSK_START_PRIMARY);
	}

	bt_case("der Zaehler laesst sich nicht anheben: NICHT bestaetigen");
	{
		osk_boot_state_t st;
		int which;

		prepare(&env, &flash, &slots);
		fake_session_init(&se, &flash);
		T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
		T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
		T_OK(osk_boot_finish(&slots, &se));
		T_OK(osk_boot_state_load(&slots, &st));
		st.stage = (uint8_t)OSK_BOOT_TESTING;
		T_OK(osk_boot_state_store(&slots, &st, &which));

		fake_counter_break(true);
		T_RES(osk_boot_confirm(&env.be, &slots), OSK_ERR_IO);
		fake_counter_break(false);
		/* Der Stick steht weiter auf TESTING: der Rueckweg ist noch
		 * da, und ein naechster Versuch ist moeglich. Waere hier
		 * bestaetigt worden, gaebe es beides nicht. */
		T_OK(osk_boot_state_load(&slots, &st));
		T_EQ(st.stage, (uint8_t)OSK_BOOT_TESTING);
		T_OK(osk_boot_confirm(&env.be, &slots));
	}

	bt_case("bestaetigen ohne Umschalten: nichts zu bestaetigen");
	prepare(&env, &flash, &slots);
	T_RES(osk_boot_confirm(&env.be, &slots), OSK_ERR_NOT_FOUND);
	T_RES(osk_boot_mark_try(&slots), OSK_ERR_NOT_FOUND);
	T_RES(osk_boot_confirm(NULL, &slots), OSK_ERR_ARG);
	T_RES(osk_boot_confirm(&env.be, NULL), OSK_ERR_ARG);
	startet_noch(&slots, OSK_START_PRIMARY);

	bt_case("ausdruecklich abbrechen: wieder sauber, alte Fassung");
	prepare(&env, &flash, &slots);
	fake_session_init(&se, &flash);
	T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
	T_TRUE(write_chunks(&slots, &se, 4));
	T_OK(osk_boot_abort(&slots, &se));
	startet_noch(&slots, OSK_START_PRIMARY);
	T_RES(osk_boot_write(&slots, &se, 4 * CHUNK, image_bytes(), CHUNK), OSK_ERR_NOT_FOUND);

	bt_case("nach dem Abbrechen laeuft ein neuer Versuch durch");
	fake_session_init(&se, &flash);
	T_OK(osk_boot_begin(&env.be, &slots, g_pkg, g_len, &se));
	T_TRUE(write_chunks(&slots, &se, IMG_LEN / CHUNK));
	T_OK(osk_boot_finish(&slots, &se));
	startet_noch(&slots, OSK_START_SWAP);
	T_MEM(flash.data, image_bytes(), IMG_LEN);
}
