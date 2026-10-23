/* Wanderung von Fassung 1 auf Fassung 2.
 *
 * Fuer den Test wird ein Tresor der Fassung 1 von Hand gebaut: dieselben
 * Geheimnisse, aber der Index liegt im alten Aufbau mit 240 Byte je Satz und
 * der Kopf nennt Fassung 1. So laesst sich pruefen, was nach einem
 * Firmware-Update wirklich passiert - ohne dass die alte Firmware noch
 * vorhanden sein muss. */

#include <string.h>
#include <stdio.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

/* Der alte Satz, wie ihn Fassung 1 geschrieben hat. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t  profile;
	uint8_t  flags;
	uint64_t changed_utc;
	uint32_t slot;
	uint32_t blob_ct_len;
	uint32_t secret_len;
	uint8_t  nonce[OSK_NONCE_LEN];
	char     name[OSK_NAME_MAX];
	char     site[OSK_SITE_MAX];
	uint8_t  reserved[8];
} rec_v1_t;

OSK_STATIC_ASSERT(sizeof(rec_v1_t) == 240u, test_rec_v1_240);

static uint8_t g_plain[sizeof(osk_idx_head_t) + OSK_ITEMS_MAX * 240u];
static uint8_t g_ct[sizeof(g_plain) + OSK_TAG_LEN];

/* Schreibt den Index eines Profils im alten Aufbau und setzt die Fassung im
 * Kopf auf 1. Alles andere - Geheimnisse, Schluessel, Protokoll - bleibt. */
static void make_v1(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_page_t *page;
	osk_idx_frame_t frame;
	osk_idx_head_t head;
	size_t ct_len = 0;

	T_OK(osk_index_load(profile));
	page = osk_index_page();
	head = page->head;
	head.format = OSK_FMT_V1;

	memcpy(g_plain, &head, sizeof(head));
	for (uint16_t i = 0; i < head.count; i++) {
		const osk_idx_rec_t *n = &page->recs[i];
		rec_v1_t o;

		memset(&o, 0, sizeof(o));
		o.id = n->id;
		o.type = n->type;
		o.profile = n->profile;
		/* Fassung 1 kannte nur zwei Kennzeichen. */
		o.flags = (uint8_t)(n->flags & (OSK_FLAG_MIRRORED |
						OSK_FLAG_PAIRED_ONLY));
		o.changed_utc = n->changed_utc;
		o.slot = n->slot;
		o.blob_ct_len = n->blob_ct_len;
		o.secret_len = n->secret_len;
		memcpy(o.nonce, n->nonce, sizeof(o.nonce));
		memcpy(o.name, n->name, sizeof(o.name));
		memcpy(o.site, n->site, sizeof(o.site));
		memcpy(g_plain + sizeof(head) + (size_t)i * sizeof(o), &o,
		       sizeof(o));
	}

	size_t plain_len = sizeof(head) + (size_t)head.count * sizeof(rec_v1_t);
	uint8_t aad[1] = { profile };

	memset(&frame, 0, sizeof(frame));
	T_OK(osk_nonce_take(OSK_NONCE_INDEX, frame.nonce));
	T_OK(osk_crypto_seal(&st->key_profile[profile], frame.nonce, aad,
			     sizeof(aad), g_plain, plain_len, g_ct,
			     sizeof(g_ct), &ct_len));
	frame.magic = OSK_IDX_MAGIC;
	frame.ct_len = (uint32_t)ct_len;
	frame.crc32 = osk_crc32(&frame, sizeof(frame) - sizeof(frame.crc32));

	uint32_t total = osk_round_page((uint32_t)(sizeof(frame) + ct_len));
	uint32_t addr = 0;

	T_OK(osk_space_index_alloc(total, &addr));

	static uint8_t page_buf[OSK_PAGE];
	uint32_t written = 0;
	size_t done = 0;

	while (written < total) {
		uint32_t off = 0;

		memset(page_buf, 0xFF, sizeof(page_buf));
		if (written == 0u) {
			memcpy(page_buf, &frame, sizeof(frame));
			off = sizeof(frame);
		}
		uint32_t room = (uint32_t)(sizeof(page_buf) - off);
		uint32_t take = (uint32_t)(ct_len - done);

		if (take > room) {
			take = room;
		}
		memcpy(page_buf + off, g_ct + done, take);
		done += take;
		T_OK(osk_flash_program(addr + written, page_buf,
				       sizeof(page_buf)));
		written += OSK_PAGE;
	}
	st->hdr.index_root[profile] = addr;
	st->hdr.index_len[profile] = total;
	st->hdr.format = OSK_FMT_V1;
	osk_index_drop();
	T_OK(osk_hdr_commit());
}

static osk_item_id_t g_id[3];

static void build_v1(void)
{
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "eins", "eins.example", "kennwort-eins", false, &g_id[0]));
	T_OK(t_add(0, "zwei", "zwei.example", "kennwort-zwei", true, &g_id[1]));
	T_OK(t_add(0, "drei", "drei.example", "kennwort-drei", false, &g_id[2]));
	make_v1(0);
	t_reboot();
}

static void check_items(void)
{
	T_EQ(t_count(0), 3);
	T_TRUE(t_secret_is(g_id[0], "kennwort-eins"));
	T_TRUE(t_secret_is(g_id[1], "kennwort-zwei"));
	T_TRUE(t_secret_is(g_id[2], "kennwort-drei"));

	osk_item_meta_t meta;

	T_OK(osk_store_list(0, 1, &meta));
	T_STR(meta.name, "zwei");
	T_STR(meta.site, "zwei.example");
	T_TRUE(meta.mirrored);
}

static void case_read_old(void)
{
	osk_test_case("Ein Tresor der Fassung 1 ist sofort lesbar");
	build_v1();

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V1);
	check_items();

	osk_test_case("Fassung 1 kannte den HOTP-Zaehler nicht: er faengt bei 0 an");
	uint64_t c = 1u;

	T_OK(osk_store_item_counter(g_id[0], &c));
	T_EQ(c, 0);

	osk_test_case("Fassung 1 kannte den Schreibschutz nicht");
	uint8_t flags = 0xFF;

	T_OK(osk_store_item_flags(g_id[0], &flags));
	T_EQ(flags & OSK_FLAG_READONLY, 0);
}

static void case_migrate(void)
{
	osk_test_case("Wanderung auf Fassung 2 ohne Datenverlust");
	build_v1();
	T_OK(osk_store_migrate());

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
	check_items();

	osk_test_case("Nach dem Neustart ist die Fassung dauerhaft 2");
	t_reboot();
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
	check_items();

	osk_test_case("Jetzt gibt es Schreibschutz und Zaehler");
	T_OK(osk_store_item_set_readonly(g_id[0], true));
	uint8_t flags = 0;

	T_OK(osk_store_item_flags(g_id[0], &flags));
	T_TRUE((flags & OSK_FLAG_READONLY) != 0u);

	uint64_t c = 0;

	T_OK(osk_store_item_counter_next(g_id[1], &c));
	T_EQ(c, 1);

	osk_test_case("Eine zweite Wanderung tut nichts");
	T_OK(osk_store_migrate());
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
}

static void case_migrate_by_change(void)
{
	osk_test_case("Wer etwas aendert, wandert nebenbei mit");
	build_v1();
	T_OK(osk_store_update(g_id[1], NULL, (const uint8_t *)"neu-zwei", 8));
	t_reboot();

	/* Der Index liegt jetzt in Fassung 2, der Kopf nennt noch 1 - und
	 * beides ist lesbar. */
	T_TRUE(t_secret_is(g_id[1], "neu-zwei"));
	T_TRUE(t_secret_is(g_id[0], "kennwort-eins"));
	T_OK(osk_store_migrate());

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
	T_TRUE(t_secret_is(g_id[1], "neu-zwei"));
}

static void case_migrate_power(void)
{
	uint32_t steps;

	build_v1();
	fake_flash_count_reset();
	T_OK(osk_store_migrate());
	steps = fake_flash_writes();

	osk_test_case("Wanderung: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build_v1();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_migrate();

		t_reboot();

		uint16_t v = 0;

		T_OK(osk_store_format_version(&v));
		/* Entweder noch die alte oder schon die neue Fassung. */
		T_TRUE(v == OSK_FMT_V1 || v == OSK_FMT_V2);
		if (r == OSK_OK) {
			T_EQ(v, OSK_FMT_V2);
		}
		/* In jedem Fall vollstaendig lesbar - das ist der Punkt. */
		check_items();
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen bei der Wanderung geprueft\n",
	       steps + 1u);
}

static void case_migrate_locked(void)
{
	osk_test_case("Ein gesperrtes Profil haelt die Wanderung auf");
	build_v1();
	fake_se050_set_pin(T_PIN2);
	T_OK(osk_store_profile_create(1, (const uint8_t *)T_PIN2,
				      strlen(T_PIN2)));
	T_OK(t_add(1, "zweites", "b.example", "geheim-b", false, NULL));
	make_v1(1);
	fake_se050_set_pin(T_PIN);
	t_reboot();   /* nur Profil 0 wird aufgeschlossen */

	T_RES(osk_store_migrate(), OSK_ERR_DENIED);

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V1);
	check_items();

	osk_test_case("Mit allen Profilen offen gelingt sie");
	fake_se050_set_pin(T_PIN2);
	T_OK(osk_store_profile_unlock(1, (const uint8_t *)T_PIN2,
				      strlen(T_PIN2)));
	T_OK(osk_store_migrate());
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
	T_EQ(t_count(1), 1);
	fake_se050_set_pin(T_PIN);
	check_items();
}

void test_migrate(void)
{
	case_read_old();
	case_migrate();
	case_migrate_by_change();
	case_migrate_power();
	case_migrate_locked();
}
