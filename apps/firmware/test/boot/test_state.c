/* Der Zustand des Updates muss jeden Abbruch ueberleben. Zwei Abdruecke,
 * abwechselnd beschrieben, je mit Pruefsumme: der halb geschriebene faellt
 * durch, der andere gilt weiter. */

#include <string.h>
#include "boot_test.h"
#include "fakes.h"

static void set(osk_boot_state_t *st, osk_boot_stage_t stage, uint32_t counter)
{
	memset(st, 0, sizeof(*st));
	st->stage = (uint8_t)stage;
	st->counter = counter;
	st->image_len = 4096;
	st->written = 4096;
	st->version[2] = 1;
	memset(st->hash, 0x11, OSK_BOOT_HASH_LEN);
}

void test_state(void)
{
	static osk_boot_slots_t slots;
	osk_boot_state_t st, back;
	int which;

	bt_case("geloeschter Flash: nichts im Gange, kein Fehler");
	memset(&slots, 0xFF, sizeof(slots));
	T_OK(osk_boot_state_load(&slots, &st));
	T_EQ(st.stage, (uint8_t)OSK_BOOT_IDLE);
	T_EQ(st.seq, 0);
	T_EQ(osk_boot_decide(&st, false), OSK_START_PRIMARY);

	bt_case("Flash voller Nullen: ebenso");
	memset(&slots, 0x00, sizeof(slots));
	T_OK(osk_boot_state_load(&slots, &st));
	T_EQ(st.stage, (uint8_t)OSK_BOOT_IDLE);

	bt_case("schreiben und zurueckholen");
	memset(&slots, 0xFF, sizeof(slots));
	set(&st, OSK_BOOT_READY, 5);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_EQ(which, 0); /* der erste ist der aeltere */
	T_EQ(st.seq, 1);
	T_OK(osk_boot_state_load(&slots, &back));
	T_EQ(back.stage, (uint8_t)OSK_BOOT_READY);
	T_EQ(back.counter, 5);
	T_EQ(back.image_len, 4096);
	T_MEM(back.hash, st.hash, OSK_BOOT_HASH_LEN);

	bt_case("der zweite Schreibvorgang trifft den anderen Abdruck");
	set(&st, OSK_BOOT_TESTING, 6);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_EQ(which, 1);
	T_EQ(st.seq, 2);
	set(&st, OSK_BOOT_CONFIRMED, 7);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_EQ(which, 0); /* wieder der aeltere */
	T_EQ(st.seq, 3);
	T_OK(osk_boot_state_load(&slots, &back));
	T_EQ(back.counter, 7);

	bt_case("und weiter abwechselnd - nie zweimal derselbe");
	/* Dieser Fall kommt von einem Mutanten: ohne ihn blieb unbemerkt,
	 * dass immer Abdruck A beschrieben wird. Die ersten drei
	 * Schreibvorgaenge sehen bei beiden Fassungen gleich aus; erst der
	 * vierte trennt sie. */
	set(&st, OSK_BOOT_READY, 8);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_EQ(which, 1);
	T_EQ(st.seq, 4);
	set(&st, OSK_BOOT_READY, 9);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_EQ(which, 0);
	T_OK(osk_boot_state_load(&slots, &back));
	T_EQ(back.counter, 9);
	/* Und der jeweils andere haelt noch den vorigen Stand - das ist der
	 * ganze Zweck: ein abgebrochener Schreibvorgang kostet nur den
	 * neuen Abdruck, nie den gueltigen. */
	{
		osk_boot_slots_t only_b = slots;

		memset(only_b.a, 0xFF, OSK_BOOT_SLOT_LEN);
		T_OK(osk_boot_state_load(&only_b, &back));
		T_EQ(back.counter, 8);
	}

	bt_case("der neuere ist halb geschrieben: der aeltere gilt weiter");
	{
		/* Abbruch mitten im Abdruck nachgestellt: die Haelfte der
		 * Byte steht, der Rest ist geloescht. Genau dafuer gibt es
		 * die Pruefsumme. */
		memset(slots.a + 40, 0xFF, OSK_BOOT_SLOT_LEN - 40);
		T_OK(osk_boot_state_load(&slots, &back));
		T_EQ(back.counter, 8); /* der aeltere, vollstaendige */
		T_EQ(back.stage, (uint8_t)OSK_BOOT_READY);
	}

	bt_case("ein einziges verdrehtes Bit laesst einen Abdruck durchfallen");
	{
		memset(&slots, 0xFF, sizeof(slots));
		set(&st, OSK_BOOT_READY, 2);
		T_OK(osk_boot_state_store(&slots, &st, &which));
		T_OK(osk_boot_state_load(&slots, &back));
		T_EQ(back.counter, 2);
		for (int byte = 0; byte < 72; byte++) {
			slots.a[byte] ^= 0x01u;
			T_OK(osk_boot_state_load(&slots, &back));
			T_EQ(back.stage, (uint8_t)OSK_BOOT_IDLE); /* nichts gueltig */
			slots.a[byte] ^= 0x01u;
		}
		T_OK(osk_boot_state_load(&slots, &back));
		T_EQ(back.counter, 2); /* wieder heil */
	}

	bt_case("beide Abdruecke hin: das Geraet startet trotzdem");
	memset(&slots, 0x5A, sizeof(slots));
	T_OK(osk_boot_state_load(&slots, &st));
	T_EQ(st.stage, (uint8_t)OSK_BOOT_IDLE);
	T_EQ(osk_boot_decide(&st, false), OSK_START_PRIMARY);

	bt_case("Folgenummer 0 gilt als leer und wird nie geschrieben");
	memset(&slots, 0xFF, sizeof(slots));
	set(&st, OSK_BOOT_READY, 1);
	T_OK(osk_boot_state_store(&slots, &st, &which));
	T_TRUE(st.seq != 0);

	bt_case("unbekannte Stufe wird nicht gespeichert und nicht geglaubt");
	set(&st, OSK_BOOT_READY, 1);
	st.stage = 99;
	T_RES(osk_boot_state_store(&slots, &st, &which), OSK_ERR_ARG);
	slots.a[8] = 99; /* von Hand in den Flash geschrieben */
	T_OK(osk_boot_state_load(&slots, &back));
	T_TRUE(back.stage <= (uint8_t)OSK_BOOT_CONFIRMED);
	{
		/* Der Fall oben faellt schon an der Pruefsumme - er misst
		 * also die fruehere Wand. Hier derselbe Abdruck MIT richtig
		 * nachgerechneter Pruefsumme: nur so wird wirklich geprueft,
		 * dass eine unbekannte Stufe nicht geglaubt wird. Auch
		 * dieser Fall kommt von einem Mutanten. */
		uint8_t forged[OSK_BOOT_SLOT_LEN];
		uint32_t crc;

		/* Erst einen sicher gueltigen Abdruck herstellen - sonst
		 * faellt die Faelschung an der Folgenummer und nicht an der
		 * Stufe, und der Fall misst wieder die fruehere Wand.
		 * (Genau das ist beim ersten Lauf passiert.) */
		memset(&slots, 0xFF, sizeof(slots));
		set(&st, OSK_BOOT_READY, 4);
		T_OK(osk_boot_state_store(&slots, &st, &which));
		memset(forged, 0, sizeof(forged));
		memcpy(forged, slots.a, OSK_BOOT_SLOT_LEN); /* ein gueltiger Abdruck */
		forged[8] = 99;				    /* Stufe verbogen */
		crc = osk_boot_crc32(forged, 72);
		forged[72] = (uint8_t)(crc & 0xFFu);
		forged[73] = (uint8_t)((crc >> 8) & 0xFFu);
		forged[74] = (uint8_t)((crc >> 16) & 0xFFu);
		forged[75] = (uint8_t)((crc >> 24) & 0xFFu);

		memcpy(slots.a, forged, OSK_BOOT_SLOT_LEN);
		memset(slots.b, 0xFF, OSK_BOOT_SLOT_LEN);
		T_OK(osk_boot_state_load(&slots, &back));
		T_EQ(back.stage, (uint8_t)OSK_BOOT_IDLE); /* verworfen */
		T_EQ(osk_boot_decide(&back, false), OSK_START_PRIMARY);

		/* Zwischenpruefung: derselbe Weg mit gueltiger Stufe kommt
		 * durch - die Ablehnung oben kommt also von der Stufe. */
		forged[8] = (uint8_t)OSK_BOOT_READY;
		crc = osk_boot_crc32(forged, 72);
		forged[72] = (uint8_t)(crc & 0xFFu);
		forged[73] = (uint8_t)((crc >> 8) & 0xFFu);
		forged[74] = (uint8_t)((crc >> 16) & 0xFFu);
		forged[75] = (uint8_t)((crc >> 24) & 0xFFu);
		memcpy(slots.a, forged, OSK_BOOT_SLOT_LEN);
		T_OK(osk_boot_state_load(&slots, &back));
		T_EQ(back.stage, (uint8_t)OSK_BOOT_READY);
	}

	bt_case("Zeiger fehlt: ARG");
	T_RES(osk_boot_state_load(NULL, &st), OSK_ERR_ARG);
	T_RES(osk_boot_state_load(&slots, NULL), OSK_ERR_ARG);
	T_RES(osk_boot_state_store(NULL, &st, &which), OSK_ERR_ARG);
	T_RES(osk_boot_state_store(&slots, NULL, &which), OSK_ERR_ARG);
	T_RES(osk_boot_state_store(&slots, &st, NULL), OSK_ERR_ARG);

	bt_case("die Pruefsumme reagiert ueberhaupt");
	{
		/* Eine Grenze, die nur durch sich selbst geprueft wird, ist
		 * nicht geprueft: hier zwei bekannte Faelle von Hand. */
		const uint8_t a[4] = {0, 0, 0, 0};
		const uint8_t b[4] = {0, 0, 0, 1};

		T_TRUE(osk_boot_crc32(a, 4) != osk_boot_crc32(b, 4));
		T_EQ(osk_boot_crc32(NULL, 4), 0);
		/* CRC-32 von "123456789" ist 0xCBF43926 - der Wert, mit dem
		 * jede Umsetzung dieses Verfahrens geprueft wird. */
		T_EQ(osk_boot_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
	}
}
