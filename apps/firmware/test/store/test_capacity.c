/* Die Kapazitaetsrechnung - nachgerechnet aus den Konstanten selbst.
 *
 * Dass 3072 Eintraege wirklich hineingehen, misst test_paging.c: dort wird
 * das Geraet voll geschrieben. Hier wird die andere Haelfte geprueft, die
 * sonst niemand nachzaehlt - ob die Zahlen, die im Plan, im Handbuch und in
 * shared/vault-format.md stehen, noch zu den Konstanten passen.
 *
 * Der Grund ist eine Regel dieses Projekts: eine Zahl steht an einer Stelle.
 * Kopien veralten still. Dieser Pruefsatz macht aus den Kopien in den
 * Dokumenten wenigstens gepruefte Kopien - aendert jemand OSK_IDX_BLOCKS_MAX,
 * wird es hier rot und nicht erst im Handbuch. */

#include <stdio.h>
#include "osk_test.h"
#include "test_util.h"
#include "osk_vault_format.h"

/* Plaetze im Index-Ring, genau wie store_space.c sie rechnet. */
#define IDX_PLACES ((OSK_AREA_IDX_LEN + OSK_AREA_IDX2_LEN) / OSK_IDX_BLOCK_SPAN)

void test_capacity(void)
{
	osk_test_case("4 Profile, 768 Eintraege je Profil, 3072 im Geraet");
	T_EQ(OSK_PROFILE_MAX, 4);
	T_EQ(OSK_IDX_BLOCK_RECS * OSK_IDX_BLOCKS_MAX, 768);
	T_EQ(OSK_ITEMS_MAX, 768);
	T_EQ(OSK_PROFILE_MAX * OSK_ITEMS_MAX, 3072);

	osk_test_case("Je Eintrag ein Datenplatz - die Plaetze reichen genau");
	T_EQ(OSK_BLOB_SLOTS, 3072);
	T_EQ(OSK_BLOB_SLOT, 2 * OSK_SECTOR);
	T_EQ((uint64_t)OSK_BLOB_SLOTS * OSK_BLOB_SLOT, 24u * 1024u * 1024u);

	osk_test_case("Die Bereiche liegen lueckenlos hintereinander");
	T_EQ(OSK_AREA_IDX_OFF, OSK_AREA_HDR_OFF + OSK_AREA_HDR_LEN);
	T_EQ(OSK_AREA_LOG_OFF, OSK_AREA_IDX_OFF + OSK_AREA_IDX_LEN);
	T_EQ(OSK_AREA_BLOB_OFF, OSK_AREA_LOG_OFF + OSK_AREA_LOG_LEN);
	T_EQ(OSK_AREA_HDR2_OFF, OSK_AREA_BLOB_OFF + OSK_AREA_BLOB_LEN);
	T_EQ(OSK_AREA_IDX2_OFF, OSK_AREA_HDR2_OFF + OSK_AREA_HDR2_LEN);
	T_EQ(OSK_AREA_BLOB2_OFF, OSK_AREA_IDX2_OFF + OSK_AREA_IDX2_LEN);

	osk_test_case("Die Summe ist 28,2 MiB - 29 564 928 Byte");
	{
		uint64_t sum = (uint64_t)OSK_AREA_HDR_LEN + OSK_AREA_IDX_LEN + OSK_AREA_LOG_LEN +
			       OSK_AREA_BLOB_LEN + OSK_AREA_HDR2_LEN + OSK_AREA_IDX2_LEN +
			       OSK_AREA_BLOB2_LEN;

		T_EQ(sum, OSK_VAULT_SIZE);
		T_EQ(OSK_VAULT_SIZE, 29564928u);
		printf("      Tresor %u Byte = %u,%02u MiB\n", (unsigned)OSK_VAULT_SIZE,
		       (unsigned)(OSK_VAULT_SIZE / (1024u * 1024u)),
		       (unsigned)((OSK_VAULT_SIZE % (1024u * 1024u)) * 100u / (1024u * 1024u)));
	}

	osk_test_case("Der Index-Ring hat Luft: mehr als doppelt so viele Plaetze wie Bloecke");
	{
		/* Ein neuer Block wird IMMER an einen freien Platz
		 * geschrieben, bevor der alte ungueltig wird - darum der
		 * Faktor zwei. 48 Bloecke sind ein volles Geraet. */
		unsigned blocks = OSK_PROFILE_MAX * OSK_IDX_BLOCKS_MAX;

		T_EQ(blocks, 48);
		T_TRUE(IDX_PLACES > 2u * blocks);
		printf("      %u Plaetze im Ring fuer %u Bloecke (%u Byte je Platz)\n",
		       (unsigned)IDX_PLACES, blocks, (unsigned)OSK_IDX_BLOCK_SPAN);
		/* Die Zahl, die in vault-format.md steht. */
		T_EQ(IDX_PLACES, 170);
		T_EQ(OSK_IDX_BLOCK_SPAN, 24u * 1024u);
	}

	osk_test_case("512 Protokollplaetze von einer Seite");
	T_EQ(OSK_LOG_SLOTS, 512);
	T_EQ(OSK_LOG_SLOT, OSK_PAGE);
	T_EQ(OSK_AREA_LOG_LEN, 128u * 1024u);
	T_TRUE(sizeof(osk_log_slot_t) <= OSK_LOG_SLOT);

	osk_test_case("Ein Kopfsatz passt in seinen Platz, auch mit dem Pruefmal der Wisch-PIN");
	{
		printf("      Kopfsatz %u von %u Byte, Belegung allein %u\n",
		       (unsigned)sizeof(osk_hdr_t), (unsigned)OSK_HDR_SLOT,
		       (unsigned)(OSK_BLOB_SLOTS / 8u));
		T_TRUE(sizeof(osk_hdr_t) <= OSK_HDR_SLOT);
		T_EQ(OSK_HDR_SLOTS, 64);
		/* Vier Saetze je Sektor - daran haengt die Verschleissrechnung
		 * in vault-format.md. */
		T_EQ(OSK_HDR_SLOTS_PER_SECTOR, 4);
	}

	osk_test_case("Ein Indexsatz ist weiterhin 320 Byte lang");
	T_EQ(sizeof(osk_idx_rec_t), 320);
	T_EQ(sizeof(osk_idx_head_t), 32);
	/* 64 Saetze zu 320 Byte plus Kopf: der Puffer, der im RAM liegt. */
	T_EQ(sizeof(osk_idx_page_t), 32 + 64 * 320);
	printf("      ein Indexblock %u Byte im RAM\n", (unsigned)sizeof(osk_idx_page_t));

	osk_test_case("4096 Eintraege wuerden nicht passen");
	{
		/* Die Zahl aus vault-format.md, hier nachgerechnet statt
		 * abgeschrieben: 4096 Plaetze zu 8 KiB sind 32 MiB, dazu der
		 * Index - zusammen mehr als die 34,5 MiB, die frei sind. */
		uint64_t slots = 4096ull * OSK_BLOB_SLOT;
		uint64_t idx = (4096ull / OSK_IDX_BLOCK_RECS) * OSK_IDX_BLOCK_SPAN;
		uint64_t free_bytes = 34ull * 1024u * 1024u + 512u * 1024u; /* 34,5 MiB */

		printf("      4096 Eintraege: %u MiB Plaetze + %u MiB Index (live)\n",
		       (unsigned)(slots / (1024u * 1024u)), (unsigned)(idx / (1024u * 1024u)));
		/* Schon die Plaetze allein lassen fuer einen Ring mit Luft
		 * nichts uebrig. */
		T_TRUE(slots + 3ull * idx > free_bytes);
	}
}
