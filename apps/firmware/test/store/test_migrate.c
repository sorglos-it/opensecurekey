/* Wanderung von einer alten Formatfassung auf die jetzige.
 *
 * Fuer den Test wird ein alter Tresor von Hand gebaut: dieselben Geheimnisse,
 * aber der Index liegt im alten Aufbau und der Kopf nennt die alte Fassung. So
 * laesst sich pruefen, was nach einem Firmware-Update wirklich passiert - ohne
 * dass die alte Firmware noch vorhanden sein muss.
 *
 * Geprueft werden BEIDE Wege: Fassung 1 (240 Byte je Satz, ohne HOTP-Zaehler,
 * ohne Schreibschutz, ohne Kennung) und Fassung 2 (256 Byte, ohne Kennung). */

#include <stdio.h>
#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

/* Der Satz, wie ihn Fassung 1 geschrieben hat. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t profile;
	uint8_t flags;
	uint64_t changed_utc;
	uint32_t slot;
	uint32_t blob_ct_len;
	uint32_t secret_len;
	uint8_t nonce[OSK_NONCE_LEN];
	char name[OSK_NAME_MAX];
	char site[OSK_SITE_MAX];
	uint8_t reserved[8];
} rec_v1_t;

/* Der Satz, wie ihn Fassung 2 geschrieben hat. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t profile;
	uint8_t flags;
	uint64_t changed_utc;
	uint32_t slot;
	uint32_t blob_ct_len;
	uint32_t secret_len;
	uint32_t pad0;
	uint64_t counter;
	uint8_t nonce[OSK_NONCE_LEN];
	char name[OSK_NAME_MAX];
	char site[OSK_SITE_MAX];
	uint8_t reserved[12];
} rec_v2_t;

OSK_STATIC_ASSERT(sizeof(rec_v1_t) == OSK_IDX_REC_V1_SIZE, test_rec_v1_240);
OSK_STATIC_ASSERT(sizeof(rec_v2_t) == OSK_IDX_REC_V2_SIZE, test_rec_v2_256);

static uint8_t g_plain[sizeof(osk_idx_head_t) + OSK_ITEMS_MAX * OSK_IDX_REC_V2_SIZE];
static uint8_t g_ct[sizeof(g_plain) + OSK_TAG_LEN];

/* Schreibt den Index eines Profils im alten Aufbau und setzt die Fassung im
 * Kopf. Alles andere - Geheimnisse, Schluessel, Protokoll - bleibt. */
static void make_old(uint8_t profile, uint16_t format)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_page_t *page;
	osk_idx_frame_t frame;
	osk_idx_head_t head;
	size_t ct_len = 0;
	size_t rec_size = (format == OSK_FMT_V1) ? OSK_IDX_REC_V1_SIZE : OSK_IDX_REC_V2_SIZE;

	T_OK(osk_index_load(profile));
	page = osk_index_page();
	head = page->head;
	head.format = format;

	memcpy(g_plain, &head, sizeof(head));
	for (uint16_t i = 0; i < head.count; i++) {
		const osk_idx_rec_t *n = &page->recs[i];
		uint8_t *dst = g_plain + sizeof(head) + (size_t)i * rec_size;

		if (format == OSK_FMT_V1) {
			rec_v1_t o;

			memset(&o, 0, sizeof(o));
			o.id = n->id;
			o.type = n->type;
			o.profile = n->profile;
			/* Fassung 1 kannte nur zwei Kennzeichen. */
			o.flags = (uint8_t)(n->flags & (OSK_FLAG_MIRRORED | OSK_FLAG_PAIRED_ONLY));
			o.changed_utc = n->changed_utc;
			o.slot = n->slot;
			o.blob_ct_len = n->blob_ct_len;
			o.secret_len = n->secret_len;
			memcpy(o.nonce, n->nonce, sizeof(o.nonce));
			memcpy(o.name, n->name, sizeof(o.name));
			memcpy(o.site, n->site, sizeof(o.site));
			memcpy(dst, &o, sizeof(o));
		} else {
			rec_v2_t o;

			memset(&o, 0, sizeof(o));
			o.id = n->id;
			o.type = n->type;
			o.profile = n->profile;
			o.flags = n->flags;
			o.changed_utc = n->changed_utc;
			o.slot = n->slot;
			o.blob_ct_len = n->blob_ct_len;
			o.secret_len = n->secret_len;
			o.counter = n->counter;
			memcpy(o.nonce, n->nonce, sizeof(o.nonce));
			memcpy(o.name, n->name, sizeof(o.name));
			memcpy(o.site, n->site, sizeof(o.site));
			memcpy(dst, &o, sizeof(o));
		}
	}

	size_t plain_len = sizeof(head) + (size_t)head.count * rec_size;
	uint8_t aad[1] = {profile};

	memset(&frame, 0, sizeof(frame));
	T_OK(osk_nonce_take(OSK_NONCE_INDEX, frame.nonce));
	T_OK(osk_crypto_seal(&st->key_profile[profile], frame.nonce, aad, sizeof(aad), g_plain,
			     plain_len, g_ct, sizeof(g_ct), &ct_len));
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
		T_OK(osk_flash_program(addr + written, page_buf, sizeof(page_buf)));
		written += OSK_PAGE;
	}
	st->hdr.index_root[profile] = addr;
	st->hdr.index_len[profile] = total;
	st->hdr.format = (uint16_t)format;
	osk_index_drop();
	T_OK(osk_hdr_commit());
}

static osk_item_id_t g_id[3];
static const uint8_t TAG[8] = {1, 2, 3, 4, 5, 6, 7, 8};

static void build_old(uint16_t format)
{
	osk_item_meta_t meta;

	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "eins", "eins.example", "kennwort-eins", false, &g_id[0]));
	T_OK(t_add(0, "zwei", "zwei.example", "kennwort-zwei", true, &g_id[1]));

	/* Der dritte Eintrag bekommt eine Kennung. In der alten Fassung gibt es
	 * dafuer kein Feld - genau darum muss die Wanderung sie verlieren
	 * duerfen, ohne etwas zu erfinden. */
	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = 0;
	meta.name = "drei";
	meta.site = "drei.example";
	meta.tag_len = sizeof(TAG);
	memcpy(meta.tag, TAG, sizeof(TAG));
	T_OK(osk_store_add(&meta, (const uint8_t *)"kennwort-drei", 13, &g_id[2]));

	make_old(0, format);
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

static void case_read_old(uint16_t format)
{
	osk_test_case("Ein alter Tresor ist sofort lesbar");
	build_old(format);

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, format);
	check_items();

	osk_test_case("Was die alte Fassung nicht kannte, bleibt leer");
	osk_item_meta_t meta;

	T_OK(osk_store_list(0, 2, &meta));
	T_STR(meta.name, "drei");
	/* Keine Fassung vor 3 hatte ein Feld fuer die Kennung. Sie ist weg,
	 * und sie wird nicht geraten - eine falsche Kennung waere schlimmer
	 * als keine. */
	T_EQ(meta.tag_len, 0);

	uint64_t c = 1u;

	T_OK(osk_store_item_counter(g_id[0], &c));
	if (format == OSK_FMT_V1) {
		T_EQ(c, 0);

		uint8_t flags = 0xFF;

		T_OK(osk_store_item_flags(g_id[0], &flags));
		T_EQ(flags & OSK_FLAG_READONLY, 0);
	}
}

static void case_migrate(uint16_t format)
{
	osk_test_case("Wanderung auf die jetzige Fassung ohne Datenverlust");
	build_old(format);
	T_OK(osk_store_migrate());

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	check_items();

	osk_test_case("Nach dem Neustart ist die Fassung dauerhaft die neue");
	t_reboot();
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	check_items();

	osk_test_case("Jetzt gibt es Schreibschutz, Zaehler und Kennung");
	T_OK(osk_store_item_set_readonly(g_id[0], true));
	uint8_t flags = 0;

	T_OK(osk_store_item_flags(g_id[0], &flags));
	T_TRUE((flags & OSK_FLAG_READONLY) != 0u);

	uint64_t c = 0;

	T_OK(osk_store_item_counter_next(g_id[1], &c));
	T_EQ(c, 1);

	/* Die Kennung laesst sich nachtragen - und ab dann wird gefunden. */
	osk_item_meta_t meta;

	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = 0;
	meta.name = "drei";
	meta.site = "drei.example";
	meta.tag_len = sizeof(TAG);
	memcpy(meta.tag, TAG, sizeof(TAG));
	T_OK(osk_store_update(g_id[2], &meta, NULL, 0));

	osk_item_id_t found[4];
	size_t len = 0;

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, TAG, sizeof(TAG), found, 4, &len));
	T_EQ(len, 1);
	T_EQ(found[0], g_id[2]);

	osk_test_case("Eine zweite Wanderung tut nichts");
	T_OK(osk_store_migrate());
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
}

static void case_migrate_by_change(uint16_t format)
{
	osk_test_case("Wer etwas aendert, wandert nebenbei mit");
	build_old(format);
	T_OK(osk_store_update(g_id[1], NULL, (const uint8_t *)"neu-zwei", 8));
	t_reboot();

	/* Der Index liegt jetzt in der neuen Fassung, der Kopf nennt noch die
	 * alte - und beides ist lesbar. */
	T_TRUE(t_secret_is(g_id[1], "neu-zwei"));
	T_TRUE(t_secret_is(g_id[0], "kennwort-eins"));
	T_OK(osk_store_migrate());

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	T_TRUE(t_secret_is(g_id[1], "neu-zwei"));
}

static void case_migrate_power(uint16_t format)
{
	uint32_t steps;

	build_old(format);
	fake_flash_count_reset();
	T_OK(osk_store_migrate());
	steps = fake_flash_writes();

	osk_test_case("Wanderung: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build_old(format);
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_migrate();

		t_reboot();

		uint16_t v = 0;

		T_OK(osk_store_format_version(&v));
		/* Entweder noch die alte oder schon die neue Fassung. */
		T_TRUE(v == format || v == OSK_FMT_CURRENT);
		if (r == OSK_OK) {
			T_EQ(v, OSK_FMT_CURRENT);
		}
		/* In jedem Fall vollstaendig lesbar - das ist der Punkt. */
		check_items();
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
	printf("      Fassung %u: %u Schreibstellen bei der Wanderung\n", (unsigned)format,
	       steps + 1u);
}

static void case_migrate_locked(void)
{
	osk_test_case("Ein gesperrtes Profil haelt die Wanderung auf");
	build_old(OSK_FMT_V2);
	fake_se050_set_pin(T_PIN2);
	T_OK(osk_store_profile_create(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
	T_OK(t_add(1, "zweites", "b.example", "geheim-b", false, NULL));
	make_old(1, OSK_FMT_V2);
	fake_se050_set_pin(T_PIN);
	t_reboot(); /* nur Profil 0 wird aufgeschlossen */

	T_RES(osk_store_migrate(), OSK_ERR_DENIED);

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_V2);
	check_items();

	osk_test_case("Mit allen Profilen offen gelingt sie");
	fake_se050_set_pin(T_PIN2);
	T_OK(osk_store_profile_unlock(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
	T_OK(osk_store_migrate());
	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	T_EQ(t_count(1), 1);
	fake_se050_set_pin(T_PIN);
	check_items();
}

static void case_unknown_format(void)
{
	osk_test_case("Eine unbekannte Fassung wird nicht geraten");
	build_old(OSK_FMT_V2);

	/* Ein Tresor aus der Zukunft: hoehere Fassung, als diese Firmware
	 * kennt. Lieber eine klare Auskunft als falsch gelesene Daten. */
	osk_store_state_t *st = osk_store_state();

	st->hdr.format = (uint16_t)(OSK_FMT_CURRENT + 1u);
	T_OK(osk_hdr_commit());

	osk_store_lock();
	fake_flash_revive();
	T_RES(osk_store_open(), OSK_ERR_FORMAT);
	T_TRUE(!osk_store_is_open());
}

void test_migrate(void)
{
	const uint16_t formats[2] = {OSK_FMT_V1, OSK_FMT_V2};

	for (int i = 0; i < 2; i++) {
		printf("    [Fassung %u -> %u]\n", (unsigned)formats[i], (unsigned)OSK_FMT_CURRENT);
		case_read_old(formats[i]);
		case_migrate(formats[i]);
		case_migrate_by_change(formats[i]);
		case_migrate_power(formats[i]);
	}
	case_migrate_locked();
	case_unknown_format();
}
