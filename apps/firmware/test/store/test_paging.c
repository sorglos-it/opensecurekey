/* Der Index in Bloecken - der Grund fuer Fassung 4.
 *
 * Bis Fassung 3 lag eine ganze Indexfassung im RAM: 64 Saetze zu 320 Byte,
 * rund 20 KiB. Daran - und nicht am Flash - hing die Grenze von 64 Eintraegen
 * je Profil. Seit Fassung 4 liegt der Index in Bloecken zu 64 Saetzen, und im
 * RAM ist immer nur einer. Hier wird geprueft, dass das wirklich so ist und
 * was es kostet.
 *
 * Geprueft wird vor allem das, was neu dazugekommen ist:
 *  - Eintraege ueber die Blockgrenze hinaus, auch genau AN der Grenze.
 *  - Loeschen, wenn die Luecke und der letzte Satz in verschiedenen Bloecken
 *    liegen: dann werden ZWEI Bloecke neu geschrieben, und das muss ebenso
 *    atomar sein wie einer. Darum auch hier ein Ausfall an jeder Schreibstelle.
 *  - Ein Block, der leer wird, faellt weg.
 *  - Das Geraet voll: 768 Eintraege je Profil, 3072 insgesamt.
 *  - Was eine Suche ueber die Kennung kostet. Gemessen wird die ARBEIT
 *    (entschluesselte Bloecke und Bytes), nicht die Laufzeit dieser Attrappe -
 *    die sagt ueber das Geraet nichts. */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

/* Die PINs der vier Profile. Nur Profil 0 wird je neu aufgeschlossen, darum
 * reicht eine Liste zum Anlegen. */
static const char *const PINS[OSK_PROFILE_MAX] = {T_PIN, T_PIN2, "drittes123", "viertes4567"};

static osk_item_id_t g_ids[OSK_ITEMS_MAX];

/* Legt Eintraege 0 bis n-1 in einem Profil an. Name und Geheimnis tragen die
 * Nummer, damit jeder Satz eindeutig wiederzuerkennen ist. */
static void fill(uint8_t profile, unsigned n, osk_item_id_t *ids)
{
	for (unsigned i = 0; i < n; i++) {
		char name[32];
		char secret[32];
		osk_item_id_t id = 0;

		snprintf(name, sizeof(name), "satz-%u", i);
		snprintf(secret, sizeof(secret), "geheim-%u-%u", profile, i);
		T_OK(t_add(profile, name, "x.example", secret, false, &id));
		if (ids != NULL) {
			ids[i] = id;
		}
	}
}

static bool secret_of_is(uint8_t profile, unsigned i, osk_item_id_t id)
{
	char secret[32];

	snprintf(secret, sizeof(secret), "geheim-%u-%u", profile, i);
	return t_secret_is(id, secret);
}

/* ---- Mehr als ein Block ----------------------------------------------- */

static void case_over_the_edge(void)
{
	osk_test_case("Die Grenze liegt nicht mehr bei 64 Eintraegen");
	t_device(0, 1u, T_PIN);
	fill(0, 100u, g_ids);
	T_EQ(t_count(0), 100);

	/* Jeder Satz ist ueber seine Nummer erreichbar, auch jenseits der
	 * Blockgrenze - und sein Geheimnis ist unversehrt. */
	for (unsigned i = 0; i < 100u; i++) {
		osk_item_meta_t meta;
		char name[32];

		snprintf(name, sizeof(name), "satz-%u", i);
		T_OK(osk_store_list(0, i, &meta));
		T_STR(meta.name, name);
		T_EQ(meta.id, g_ids[i]);
	}
	T_TRUE(secret_of_is(0, 0u, g_ids[0]));
	T_TRUE(secret_of_is(0, 63u, g_ids[63]));
	T_TRUE(secret_of_is(0, 64u, g_ids[64]));
	T_TRUE(secret_of_is(0, 99u, g_ids[99]));

	osk_test_case("Nach dem Neustart auch");
	t_reboot();
	T_EQ(t_count(0), 100);
	T_TRUE(secret_of_is(0, 64u, g_ids[64]));
	T_TRUE(secret_of_is(0, 99u, g_ids[99]));
}

static void case_block_edges(void)
{
	osk_test_case("Genau an der Blockgrenze stimmt die Zahl");
	t_device(0, 1u, T_PIN);

	osk_store_state_t *st = osk_store_state();

	fill(0, 63u, NULL);
	T_EQ(t_count(0), 63);
	T_EQ(st->hdr.index_blocks[0], 1);

	fill(0, 1u, NULL); /* der 64. - der Block ist jetzt voll */
	T_EQ(t_count(0), 64);
	T_EQ(st->hdr.index_blocks[0], 1);

	fill(0, 1u, NULL); /* der 65. - ein zweiter Block entsteht */
	T_EQ(t_count(0), 65);
	T_EQ(st->hdr.index_blocks[0], 2);
	T_TRUE(st->hdr.index_block[0][1] != 0u);

	osk_test_case("Im RAM liegt immer nur EIN Block");
	/* Das ist die Zusage, um die es geht: der Arbeitspuffer ist so gross
	 * wie ein Block, nicht wie der ganze Index. */
	T_EQ(sizeof(osk_idx_page_t), sizeof(osk_idx_head_t) + 64u * sizeof(osk_idx_rec_t));

	osk_item_meta_t meta;

	T_OK(osk_store_list(0, 0, &meta));
	T_EQ(osk_index_loaded_block(), 0);
	T_OK(osk_store_list(0, 64, &meta));
	T_EQ(osk_index_loaded_block(), 1);
	T_OK(osk_store_list(0, 10, &meta));
	T_EQ(osk_index_loaded_block(), 0);
}

/* ---- Loeschen ueber die Blockgrenze ----------------------------------- */

#define D_COUNT 130u
#define D_HOLE 5u

static void build_for_delete(void)
{
	t_device(0, 1u, T_PIN);
	fill(0, D_COUNT, g_ids);
}

/* Was nach dem Loeschen gelten muss: ein Satz weniger, der geloeschte ist
 * weg, und jeder andere ist noch da. */
static void check_after_delete(void)
{
	T_EQ(t_count(0), D_COUNT - 1u);

	/* Der letzte Satz ist in die Luecke gerutscht. */
	osk_item_meta_t meta;

	T_OK(osk_store_list(0, D_HOLE, &meta));
	T_EQ(meta.id, g_ids[D_COUNT - 1u]);
	T_TRUE(secret_of_is(0, D_COUNT - 1u, g_ids[D_COUNT - 1u]));

	/* Und wirklich jeder andere Satz ist noch lesbar. */
	for (unsigned i = 0; i < D_COUNT; i++) {
		if (i == D_HOLE) {
			continue;
		}
		T_TRUE(secret_of_is(0, i, g_ids[i]));
	}
	T_RES(osk_store_delete(g_ids[D_HOLE]), OSK_ERR_NOT_FOUND);
}

static void case_delete_across_blocks(void)
{
	osk_test_case("Loeschen ueber die Blockgrenze: der letzte rutscht nach vorn");
	build_for_delete();

	/* Die Luecke liegt in Block 0, der letzte Satz in Block 2. Damit
	 * schreibt dieses Loeschen ZWEI Bloecke. */
	T_EQ(D_HOLE / 64u, 0);
	T_EQ((D_COUNT - 1u) / 64u, 2);

	T_OK(osk_store_delete(g_ids[D_HOLE]));
	check_after_delete();

	osk_test_case("Nach dem Neustart ebenso");
	t_reboot();
	check_after_delete();
}

static void case_delete_power(void)
{
	uint32_t steps;

	build_for_delete();
	fake_flash_count_reset();
	T_OK(osk_store_delete(g_ids[D_HOLE]));
	steps = fake_flash_writes();

	osk_test_case("Loeschen ueber zwei Bloecke: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build_for_delete();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_delete(g_ids[D_HOLE]);

		t_reboot();

		size_t n = t_count(0);

		/* Entweder alle oder einer weniger - nie ein halber Index mit
		 * einem Satz, der in zwei Bloecken steht oder in keinem. */
		T_TRUE(n == D_COUNT || n == D_COUNT - 1u);
		if (r == OSK_OK) {
			T_EQ(n, D_COUNT - 1u);
		}

		/* In jedem der beiden Zustaende muss JEDER gemeldete Satz
		 * lesbar sein, und kein Satz darf doppelt auftauchen. */
		static uint8_t seen[D_COUNT];

		memset(seen, 0, sizeof(seen));
		for (size_t k = 0; k < n; k++) {
			osk_item_meta_t meta;
			unsigned nr = 0;

			T_OK(osk_store_list(0, k, &meta));
			T_EQ(sscanf(meta.name, "satz-%u", &nr), 1);
			T_TRUE(nr < D_COUNT);
			if (nr < D_COUNT) {
				T_EQ(seen[nr], 0);
				seen[nr] = 1u;
				T_TRUE(secret_of_is(0, nr, meta.id));
			}
		}
		T_EQ(fake_crypto_nonce_dups(), 0);
		T_EQ(fake_crypto_vault_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen beim Loeschen ueber zwei Bloecke geprueft\n", steps + 1u);
}

static void case_block_falls_away(void)
{
	osk_test_case("Ein Block, der leer wird, faellt weg");
	t_device(0, 1u, T_PIN);
	fill(0, 65u, g_ids);

	osk_store_state_t *st = osk_store_state();

	T_EQ(st->hdr.index_blocks[0], 2);

	/* Den 65. loeschen: Block 1 wird leer. */
	T_OK(osk_store_delete(g_ids[64]));
	T_EQ(t_count(0), 64);
	T_EQ(st->hdr.index_blocks[0], 1);
	T_EQ(st->hdr.index_block[0][1], 0);

	t_reboot();
	T_EQ(t_count(0), 64);
	T_TRUE(secret_of_is(0, 0u, g_ids[0]));
	T_TRUE(secret_of_is(0, 63u, g_ids[63]));

	osk_test_case("Block 0 bleibt, auch wenn er leer wird");
	t_device(0, 1u, T_PIN);
	fill(0, 1u, g_ids);
	T_OK(osk_store_delete(g_ids[0]));
	T_EQ(t_count(0), 0);
	T_EQ(st->hdr.index_blocks[0], 1);
	T_TRUE(st->hdr.index_block[0][0] != 0u);
	t_reboot();
	T_EQ(t_count(0), 0);
}

/* ---- Das Geraet voll -------------------------------------------------- */

static void case_device_full(void)
{
	osk_store_state_t *st = osk_store_state();

	osk_test_case("Vier Profile mal 768 Eintraege - das Geraet ist voll");
	t_device(0, 1u, T_PIN);
	for (uint8_t p = 1; p < OSK_PROFILE_MAX; p++) {
		T_OK(osk_store_profile_create(p, (const uint8_t *)PINS[p], strlen(PINS[p])));
	}

	for (uint8_t p = 0; p < OSK_PROFILE_MAX; p++) {
		fill(p, OSK_ITEMS_MAX, (p == 0) ? g_ids : NULL);
		T_EQ(t_count(p), OSK_ITEMS_MAX);
	}

	/* Je Profil voll - und zusammen so viele, wie es Datenplaetze gibt. */
	for (uint8_t p = 0; p < OSK_PROFILE_MAX; p++) {
		T_RES(t_add(p, "einer-zuviel", "x.example", "geheim", false, NULL), OSK_ERR_FULL);
	}
	T_EQ(OSK_PROFILE_MAX * OSK_ITEMS_MAX, 3072);

	uint32_t used = 0;

	for (uint32_t s = 0; s < OSK_BLOB_SLOTS; s++) {
		if (osk_space_slot_is_used(s)) {
			used++;
		}
	}
	printf("      %u Eintraege im Geraet, %u von %u Datenplaetzen belegt\n",
	       (unsigned)(OSK_PROFILE_MAX * OSK_ITEMS_MAX), used, (unsigned)OSK_BLOB_SLOTS);
	T_EQ(used, OSK_PROFILE_MAX * OSK_ITEMS_MAX);
	T_EQ(st->hdr.index_blocks[0], OSK_IDX_BLOCKS_MAX);

	osk_test_case("Auch der letzte Eintrag ist lesbar");
	T_TRUE(secret_of_is(0, 0u, g_ids[0]));
	T_TRUE(secret_of_is(0, OSK_ITEMS_MAX / 2u, g_ids[OSK_ITEMS_MAX / 2u]));
	T_TRUE(secret_of_is(0, OSK_ITEMS_MAX - 1u, g_ids[OSK_ITEMS_MAX - 1u]));

	osk_test_case("Im Flash steht auch im neuen Bereich kein Klartext");
	/* 768 Bloecke geschrieben: der Index-Ring ist dabei in den zweiten
	 * Abschnitt gelaufen. Beide werden abgesucht. */
	static uint8_t area[OSK_AREA_IDX2_LEN];

	T_OK(osk_flash_read(OSK_AREA_IDX_OFF, area, OSK_AREA_IDX_LEN));
	T_TRUE(memmem_missing(area, OSK_AREA_IDX_LEN, "satz-767"));
	T_OK(osk_flash_read(OSK_AREA_IDX2_OFF, area, OSK_AREA_IDX2_LEN));
	T_TRUE(memmem_missing(area, OSK_AREA_IDX2_LEN, "satz-767"));
}

/* ---- Was eine Suche kostet -------------------------------------------- */

/* Eine Kennung, die zu keinem Eintrag passt: so laeuft die Suche durch ALLE
 * Bloecke und misst den schlechtesten Fall. */
static void measure_find(uint8_t profile, size_t count, const char *was)
{
	uint8_t tag[OSK_TAG_MAX];
	osk_item_id_t found[4];
	size_t len = 0;
	const unsigned ROUNDS = 20u;

	memset(tag, 0xAB, sizeof(tag));

	/* Erst EINE Suche, um die Arbeit zu zaehlen. */
	osk_index_drop();
	fake_crypto_open_reset();
	fake_ui_reset();
	T_OK(osk_store_find_tag(profile, OSK_ITEM_LOGIN, tag, sizeof(tag), found, 4, &len));
	T_EQ(len, 0);

	size_t calls = fake_crypto_open_calls();
	size_t bytes = fake_crypto_open_bytes();

	/* Dann mehrere, um eine Laufzeit zu haben. Sie gilt fuer diesen PC mit
	 * der Krypto-Attrappe - nicht fuer das Geraet. */
	clock_t t0 = clock();

	for (unsigned i = 0; i < ROUNDS; i++) {
		osk_index_drop();
		T_OK(osk_store_find_tag(profile, OSK_ITEM_LOGIN, tag, sizeof(tag), found, 4, &len));
	}
	clock_t t1 = clock();
	double us = (double)(t1 - t0) * 1e6 / (double)CLOCKS_PER_SEC / (double)ROUNDS;

	printf("      %s (%u Eintraege): %u Bloecke entschluesselt, %u KiB, %.0f us je Suche\n",
	       was, (unsigned)count, (unsigned)calls, (unsigned)(bytes / 1024u), us);

	/* Die Zusage, die ohne Uhr gilt: ein Block je 64 Eintraege, kein
	 * Geheimnis angefasst. */
	T_EQ(calls, (count + 63u) / 64u);
}

static void case_find_cost(void)
{
	osk_test_case("Suchen ueber die Kennung: gemessen, nicht angenommen");
	t_device(0, 1u, T_PIN);
	fill(0, 64u, NULL);
	measure_find(0, 64u, "ein Block");

	t_device(0, 1u, T_PIN);
	fill(0, OSK_ITEMS_MAX, NULL);
	measure_find(0, OSK_ITEMS_MAX, "ein voller Tresor");

	/* Und sie fragt dabei nie am Geraet nach Freigabe - genau darum gibt es
	 * die Kennung. */
	osk_test_case("Dabei wird nie nach Freigabe gefragt");
	T_EQ(fake_ui_asks(), 0);
}

/* ---- Verschleiss bei vollen Bloecken ---------------------------------- */

static void case_wear_full_block(void)
{
	osk_item_id_t id = 0;
	char secret[32];
	const int RUNS = 200;

	osk_test_case("Volle Bloecke verteilen sich auch im Ring");
	t_device(0, 1u, T_PIN);
	fill(0, 63u, NULL);
	T_OK(t_add(0, "Dauerlaeufer", "dauer.example", "start", false, &id));

	for (int i = 0; i < RUNS; i++) {
		snprintf(secret, sizeof(secret), "kennwort-%d", i);
		T_OK(osk_store_update(id, NULL, (const uint8_t *)secret, strlen(secret)));
	}
	T_TRUE(t_secret_is(id, secret));

	uint32_t idx = fake_flash_sectors_touched(OSK_AREA_IDX_OFF, OSK_AREA_IDX_LEN) +
		       fake_flash_sectors_touched(OSK_AREA_IDX2_OFF, OSK_AREA_IDX2_LEN);
	uint32_t worst = fake_flash_erase_max(OSK_AREA_IDX_OFF, OSK_AREA_IDX_LEN);
	uint32_t worst2 = fake_flash_erase_max(OSK_AREA_IDX2_OFF, OSK_AREA_IDX2_LEN);

	if (worst2 > worst) {
		worst = worst2;
	}
	printf("      %d Aenderungen an einem vollen Block: %u Indexsektoren, haeufigster %u\n",
	       RUNS, idx, worst);

	/* Ein voller Block ist 20 KiB gross und schmutzt alle sechs Sektoren
	 * seines Platzes an. Der Ring hat 170 Plaetze; beim ersten Durchlauf
	 * ist nichts zu loeschen, weil dort noch nichts stand. Gezaehlt werden
	 * darum die Sektoren des zweiten Durchlaufs - und keiner darf oft dran
	 * gewesen sein. */
	T_TRUE(idx > 300u);
	T_TRUE(worst <= 2u);
}

void test_paging(void)
{
	case_over_the_edge();
	case_block_edges();
	case_delete_across_blocks();
	case_delete_power();
	case_block_falls_away();
	case_device_full();
	case_find_cost();
	case_wear_full_block();
}
