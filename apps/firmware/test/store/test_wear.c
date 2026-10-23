/* Verschleiss.
 *
 * Ein Tresor schreibt immer dieselben paar Eintraege neu. Wuerde dabei immer
 * derselbe Sektor beschrieben, waere der Flash nach etwa 100 000 Aenderungen
 * an genau dieser Stelle hin - und der Tresor mit ihm. Darum wandert alles:
 * der Kopfsatz zwischen zwei Sektoren und innerhalb eines Sektors von Platz zu
 * Platz, der Index im Ring, und die Datenplaetze im Kreis.
 *
 * Gemessen wird, wie viele Sektoren ueberhaupt benutzt werden und wie oft der
 * am haeufigsten geloeschte Sektor dran war. */

#include <stdio.h>
#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "store_internal.h"

#define RUNS 400

static void case_spread(void)
{
	osk_item_id_t id = 0;
	char secret[32];

	osk_test_case("Viele Aenderungen verteilen sich auf viele Sektoren");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "Dauerlaeufer", "dauer.example", "start", false, &id));

	for (int i = 0; i < RUNS; i++) {
		snprintf(secret, sizeof(secret), "kennwort-%d", i);
		T_OK(osk_store_update(id, NULL, (const uint8_t *)secret,
				      strlen(secret)));
	}
	T_TRUE(t_secret_is(id, secret));

	uint32_t slots = fake_flash_sectors_touched(OSK_AREA_BLOB_OFF,
						    OSK_AREA_BLOB_LEN);
	uint32_t idx = fake_flash_sectors_touched(OSK_AREA_IDX_OFF,
						  OSK_AREA_IDX_LEN);
	uint32_t worst_blob = fake_flash_erase_max(OSK_AREA_BLOB_OFF,
						   OSK_AREA_BLOB_LEN);
	uint32_t worst_idx = fake_flash_erase_max(OSK_AREA_IDX_OFF,
						  OSK_AREA_IDX_LEN);
	uint32_t worst_hdr = fake_flash_erase_max(OSK_AREA_HDR_OFF,
						  OSK_AREA_HDR_LEN);

	printf("      %d Aenderungen: %u Datensektoren, %u Indexsektoren\n",
	       RUNS, slots, idx);
	printf("      haeufigster Sektor: Daten %u, Index %u, Kopf %u\n",
	       worst_blob, worst_idx, worst_hdr);

	/* Jede Aenderung nimmt einen neuen Datenplatz; ein Platz sind zwei
	 * Sektoren. Nach 400 Aenderungen muessen viele Plaetze dran gewesen
	 * sein - nicht immer derselbe. */
	T_TRUE(slots > 300u);
	T_TRUE(worst_blob <= 2u);

	/* Der Index wandert im Ring mit; auch dort darf kein Sektor
	 * herausstechen. */
	T_TRUE(idx > 100u);
	T_TRUE(worst_idx <= 4u);

	/* Der Kopf hat nur zwei Sektoren, aber 16 Plaetze je Sektor: er wird
	 * hoechstens jedes sechzehnte Mal geloescht. */
	uint32_t hdr_erases = fake_flash_erase_count(OSK_AREA_HDR_OFF) +
			      fake_flash_erase_count(OSK_AREA_HDR_OFF +
						     OSK_SECTOR);
	uint32_t writes = fake_flash_writes();

	printf("      Kopfsektoren %u mal geloescht bei %u Schreibvorgaengen\n",
	       hdr_erases, writes);
	T_TRUE(hdr_erases * 8u < writes);
}

static void case_slot_rotation(void)
{
	osk_item_id_t id = 0;

	osk_test_case("Ein freigewordener Platz wird nicht gleich wieder genommen");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "A", "a.example", "eins", false, &id));

	osk_store_state_t *st = osk_store_state();
	uint32_t first = 0;

	T_OK(osk_index_load(0));
	first = osk_index_page()->recs[0].slot;

	T_OK(osk_store_update(id, NULL, (const uint8_t *)"zwei", 4));
	uint32_t second = osk_index_page()->recs[0].slot;

	T_TRUE(second != first);
	T_TRUE(!osk_space_slot_is_used(first));
	T_TRUE(osk_space_slot_is_used(second));

	T_OK(osk_store_update(id, NULL, (const uint8_t *)"drei", 4));
	uint32_t third = osk_index_page()->recs[0].slot;

	T_TRUE(third != second);
	T_TRUE(third != first);
	(void)st;

	osk_test_case("Der alte Platz ist nach dem Aendern wirklich leer");
	/* Ein freier Platz darf keinen alten Kryptotext mehr enthalten. */
	uint8_t buf[OSK_PAGE];

	T_OK(osk_flash_read(OSK_AREA_BLOB_OFF + first * OSK_BLOB_SLOT, buf,
			    sizeof(buf)));
	for (size_t i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0xFFu) {
			T_TRUE(false);
			break;
		}
	}
}

void test_wear(void)
{
	case_spread();
	case_slot_rotation();
}
