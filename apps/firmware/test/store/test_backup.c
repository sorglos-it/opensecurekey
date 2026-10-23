/* Sicherungsdatei und Umschlaege (Shamir). */

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
#include "osk_backup.h"
#include "osk_shamir.h"

#define PASS "wortfolge-mit-genug-zeichen"
#define BUF_MAX (256u * 1024u)

struct mem {
	uint8_t *buf;
	size_t len;
	size_t pos;
	size_t cap;
};

static uint8_t g_file[BUF_MAX];
static struct mem g_mem;

static osk_result_t sink(void *ctx, const uint8_t *data, size_t len)
{
	struct mem *m = (struct mem *)ctx;

	if (m->len + len > m->cap) {
		return OSK_ERR_FULL;
	}
	memcpy(m->buf + m->len, data, len);
	m->len += len;
	return OSK_OK;
}

static osk_result_t src(void *ctx, uint8_t *data, size_t len)
{
	struct mem *m = (struct mem *)ctx;

	if (m->pos + len > m->len) {
		/* Die Datei ist hier zu Ende - fuer den Leser ein Fehler, nie
		 * ein stilles Ende. */
		return OSK_ERR_IO;
	}
	memcpy(data, m->buf + m->pos, len);
	m->pos += len;
	return OSK_OK;
}

static void mem_reset(void)
{
	g_mem.buf = g_file;
	g_mem.cap = sizeof(g_file);
	g_mem.len = 0;
	g_mem.pos = 0;
}

/* Geraet A mit drei Eintraegen, davon zwei spiegelbar. */
static uint8_t g_tag[OSK_TAG_MAX];

static void build_source(void)
{
	osk_item_meta_t meta;

	t_device(0, 1u, T_PIN);

	/* Der erste Eintrag bekommt eine Kennung zum Suchen. */
	T_OK(osk_crypto_sha256((const uint8_t *)"bank.example", 12, g_tag));
	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = 0;
	meta.name = "Bank";
	meta.site = "bank.example";
	meta.mirrored = true;
	meta.tag_len = OSK_TAG_MAX;
	memcpy(meta.tag, g_tag, OSK_TAG_MAX);
	osk_item_id_t bank_id = 0;

	T_OK(osk_store_add(&meta, (const uint8_t *)"kennwort-eins", 13, &bank_id));
	T_OK(t_add(0, "Post", "post.example", "kennwort-zwei", false, NULL));
	T_OK(t_add(0, "Notiz", "", "ein etwas laengeres geheimnis", true, NULL));
}

/* Ein frisches, FREMDES Geraet - andere Kennung, anderer SE050. */
static void build_target(void)
{
	t_device(1, 77u, T_PIN2);
}

static void check_restored(void)
{
	T_EQ(t_count(0), 3);

	osk_item_id_t id = t_find_name(0, "Bank");

	T_TRUE(id != 0);
	T_TRUE(t_secret_is(id, "kennwort-eins"));

	id = t_find_name(0, "Post");
	T_TRUE(id != 0);
	T_TRUE(t_secret_is(id, "kennwort-zwei"));

	id = t_find_name(0, "Notiz");
	T_TRUE(id != 0);
	T_TRUE(t_secret_is(id, "ein etwas laengeres geheimnis"));

	/* Die Kennung zum Suchen wandert mit - sonst faende FIDO2 nach dem
	 * Wiederherstellen seine Nachweise nicht mehr. */
	osk_item_meta_t meta;
	osk_item_id_t bank = t_find_name(0, "Bank");

	T_OK(osk_store_list(0, 0, &meta));
	osk_item_id_t found[4];
	size_t len = 0;

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, g_tag, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 1);
	T_EQ(found[0], bank);

	/* Die Kennzeichen wandern mit. */
	uint8_t flags = 0;

	T_OK(osk_store_item_flags(t_find_name(0, "Bank"), &flags));
	T_TRUE((flags & OSK_FLAG_MIRRORED) != 0u);
	T_OK(osk_store_item_flags(t_find_name(0, "Post"), &flags));
	T_EQ(flags & OSK_FLAG_MIRRORED, 0);
}

static void case_pass_policy(void)
{
	osk_test_case("Das Hauptkennwort wird in Zeichen gemessen");
	T_RES(osk_backup_check_pass("kurz", 4), OSK_ERR_ARG);
	T_RES(osk_backup_check_pass("elfzeichen", 10), OSK_ERR_ARG);
	T_OK(osk_backup_check_pass("zwoelfzeiche", 12));
	/* Zwoelf Ziffern genuegen der Regel - deshalb wird eine Wortfolge
	 * vorgeschlagen, statt auf Ziffern zu bestehen. */
	T_OK(osk_backup_check_pass("123456789012", 12));

	osk_test_case("Steuerzeichen sind nicht erlaubt");
	T_RES(osk_backup_check_pass("mit\tzeichen\n!!", 14), OSK_ERR_ARG);

	osk_test_case("Der Vorschlag ist eine Wortfolge aus acht Woertern");
	char pass[OSK_PASS_SUGGEST_MAX];

	t_init();
	T_OK(osk_backup_suggest_pass(pass, sizeof(pass)));
	T_OK(osk_backup_check_pass(pass, strlen(pass)));

	unsigned words = 1;

	for (const char *p = pass; *p != '\0'; p++) {
		if (*p == '-') {
			words++;
		} else {
			T_TRUE(*p >= 'a' && *p <= 'z');
		}
	}
	T_EQ(words, OSK_PASS_WORDS);

	osk_test_case("Zwei Vorschlaege sind verschieden");
	char second[OSK_PASS_SUGGEST_MAX];

	T_OK(osk_backup_suggest_pass(second, sizeof(second)));
	T_TRUE(strcmp(pass, second) != 0);
}

static void case_round_trip(void)
{
	size_t written = 0, restored = 0;

	osk_test_case("Sicherung schreiben und auf einem fremden Stick lesen");
	build_source();
	mem_reset();
	fake_ui_reset();
	fake_ui_answer(true);
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, NULL));
	T_EQ(written, 3);
	T_TRUE(g_mem.len > sizeof(osk_backup_head_t));
	T_EQ(fake_ui_last_kind(), OSK_ASK_PAIR);

	osk_test_case("In der Datei steht kein Geheimnis im Klartext");
	T_TRUE(memmem_missing(g_file, g_mem.len, "kennwort-eins"));

	build_target();
	g_mem.pos = 0;
	fake_ui_answer(true);
	T_OK(osk_backup_read(PASS, strlen(PASS), src, &g_mem, &restored));
	T_EQ(restored, 3);
	check_restored();

	osk_test_case("Auch nach dem Neustart des fremden Sticks");
	t_reboot();
	check_restored();
}

static void case_wrong_pass(void)
{
	size_t written = 0, restored = 0;

	osk_test_case("Falsches Kennwort gibt nichts her");
	build_source();
	mem_reset();
	fake_ui_answer(true);
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, NULL));

	build_target();
	g_mem.pos = 0;
	T_RES(osk_backup_read("ganz-anderes-kennwort", 21, src, &g_mem, &restored), OSK_ERR_DENIED);
	T_EQ(t_count(0), 0);

	osk_test_case("Ohne Freigabe am Geraet wird nichts gelesen");
	g_mem.pos = 0;
	fake_ui_answer(false);
	T_RES(osk_backup_read(PASS, strlen(PASS), src, &g_mem, &restored), OSK_ERR_DENIED);
	T_EQ(t_count(0), 0);
	fake_ui_answer(true);
}

static void case_damaged(void)
{
	size_t written = 0, restored = 0;

	osk_test_case("Eine abgeschnittene Datei faellt auf");
	build_source();
	mem_reset();
	fake_ui_answer(true);
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, NULL));

	size_t full = g_mem.len;

	build_target();
	g_mem.len = full - 40u; /* das Abschlussstueck fehlt */
	g_mem.pos = 0;
	T_TRUE(osk_backup_read(PASS, strlen(PASS), src, &g_mem, &restored) != OSK_OK);

	osk_test_case("Ein verfaelschtes Byte faellt auf");
	build_source();
	mem_reset();
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, NULL));
	g_file[sizeof(osk_backup_head_t) + 20u] ^= 0x01u;
	build_target();
	g_mem.pos = 0;
	T_RES(osk_backup_read(PASS, strlen(PASS), src, &g_mem, &restored), OSK_ERR_DENIED);

	osk_test_case("Ein verfaelschter Kopf faellt auf");
	build_source();
	mem_reset();
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, NULL));
	g_file[8] ^= 0x01u; /* file_format */
	build_target();
	g_mem.pos = 0;
	T_RES(osk_backup_read(PASS, strlen(PASS), src, &g_mem, &restored), OSK_ERR_FORMAT);

	osk_test_case("Zu grosse Argon2-Werte werden ehrlich abgelehnt");
	osk_argon2_params_t big = {
	    .m_cost_kib = OSK_ARGON2_HOST_MEM_KIB,
	    .t_cost = OSK_ARGON2_HOST_TIME,
	    .lanes = 4u,
	};

	build_source();
	mem_reset();
	T_RES(osk_backup_write(PASS, strlen(PASS), &big, sink, &g_mem, &written, NULL),
	      OSK_ERR_UNSUPPORTED);
}

static void case_shares(void)
{
	size_t written = 0, restored = 0;
	osk_key_t key;
	osk_share_t shares[5];

	osk_test_case("Der Dateischluessel laesst sich in Umschlaege teilen");
	build_source();
	mem_reset();
	fake_ui_answer(true);
	T_OK(osk_backup_write(PASS, strlen(PASS), NULL, sink, &g_mem, &written, &key));
	T_OK(osk_shamir_split(key.b, 3u, 5u, shares));

	osk_test_case("Drei von fuenf Umschlaegen oeffnen die Sicherung");
	osk_share_t pick[3];
	osk_key_t back;

	pick[0] = shares[4];
	pick[1] = shares[1];
	pick[2] = shares[2];
	T_OK(osk_shamir_combine(pick, 3u, back.b));
	T_MEM(back.b, key.b, OSK_KEY_LEN);

	build_target();
	g_mem.pos = 0;
	T_OK(osk_backup_read_with_key(&back, src, &g_mem, &restored));
	T_EQ(restored, 3);
	check_restored();

	osk_test_case("Zwei Umschlaege genuegen nicht");
	T_RES(osk_shamir_combine(pick, 2u, back.b), OSK_ERR_DENIED);
}

void test_backup(void)
{
	case_pass_policy();
	case_round_trip();
	case_wrong_pass();
	case_damaged();
	case_shares();
}
