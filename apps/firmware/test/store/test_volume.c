/* Der Datentraeger: Baumgroesse, Pfadkosten, Unverfaelschtheit, Rueckdreh.
 *
 * Die drei Betriebsarten gegeneinander stehen in test_volume_modes.c.
 *
 * Alles hier laeuft gegen Attrappen fuer Karte, Krypto und Sicherheitschip.
 * KEINE KARTE, KEIN CHIP, KEIN BRETT. Was gruen wird, heisst: der Ablauf
 * stimmt, nicht das Geraet laeuft. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "osk_volume.h"
#include "volume_internal.h"
#include "fake_card.h"
#include "fake_se050.h"
#include "test_volume.h"

/* Eine kleine Karte: 256 Datenbloecke zu 4 KiB = 1 MiB. Gross genug fuer
 * zwei Stempelseiten und eine Ebene darueber, klein genug fuer einen
 * Pruefsatz, der in Sekunden laeuft. */
#define BLOCKS 256u

static osk_vol_mode_t g_asked_mode;
static uint32_t g_asked_recv;
static bool g_asked_secret;
static unsigned g_asked_calls;
static bool g_say_no;

osk_result_t vt_ui_confirm(osk_vol_mode_t mode, uint32_t recipients, bool secret, void *ctx)
{
	(void)ctx;
	g_asked_mode = mode;
	g_asked_recv = recipients;
	g_asked_secret = secret;
	g_asked_calls++;
	return g_say_no ? OSK_ERR_DENIED : OSK_OK;
}

void vt_ui_reset(bool say_no)
{
	g_asked_mode = (osk_vol_mode_t)0;
	g_asked_recv = 0u;
	g_asked_secret = false;
	g_asked_calls = 0u;
	g_say_no = say_no;
}

osk_vol_mode_t vt_asked_mode(void)
{
	return g_asked_mode;
}

uint32_t vt_asked_recipients(void)
{
	return g_asked_recv;
}

bool vt_asked_secret(void)
{
	return g_asked_secret;
}

unsigned vt_asked_calls(void)
{
	return g_asked_calls;
}

void vt_stick(uint32_t device)
{
	osk_key_policy_t pol = {.exportable = false, .needs_touch = false};
	uint8_t pub[32];
	size_t len = 0u;

	fake_se050_device(device);
	/* Einen vorhandenen Schluessel NICHT ersetzen. Sonst waere jeder
	 * Wechsel zurueck zu diesem Stick ein neuer Stick - und eine
	 * weitergegebene Karte liesse sich nicht mehr gegen den Absender
	 * pruefen, obwohl nichts daran falsch ist. */
	if (osk_se050_key_public(VT_SIGN_KEY, pub, sizeof(pub), &len) != OSK_OK) {
		(void)osk_se050_key_create(VT_SIGN_KEY, OSK_CURVE_P256, &pol);
	}
	(void)osk_se050_counter_create(VT_COUNTER);
}

osk_result_t vt_init(void)
{
	osk_volume_config_t cfg = {
	    .root_key = VT_ROOT_KEY,
	    .sign_key = VT_SIGN_KEY,
	    .counter_id = VT_COUNTER,
	    .ui_confirm = vt_ui_confirm,
	    .ui_ctx = NULL,
	};

	return osk_volume_init(&cfg);
}

void vt_card(uint32_t data_blocks)
{
	osk_vol_layout_t lay;

	(void)osk_vol_layout(data_blocks, &lay);
	fake_card_init(lay.total_pages);
}

static void fill(uint8_t *buf, uint8_t seed)
{
	for (uint32_t i = 0u; i < OSK_VOL_BLOCK_LEN; i++) {
		buf[i] = (uint8_t)(seed + (uint8_t)i);
	}
}

static uint8_t buf_in[OSK_VOL_BLOCK_LEN];
static uint8_t buf_out[OSK_VOL_BLOCK_LEN];

/* --- Die Rechnung: Baumgroesse und Pfadkosten ------------------------- */

static void case_sizes(void)
{
	osk_vol_layout_t l8;
	osk_vol_layout_t l16;

	osk_test_case("Baumgroesse fuer 8 GiB - gerechnet, nicht geschaetzt");
	/* 8 GiB zu 4 KiB. Die Zahlen muessen zu docs/datentraeger-konzept.md
	 * passen; wenn sie auseinanderlaufen, ist eine von beiden falsch. */
	T_OK(osk_vol_layout(2097152u, &l8));
	T_EQ(l8.leaf_pages, 16384u); /* 64 MiB Stempelfeld */
	T_EQ(l8.levels, 2u);
	T_EQ(l8.level_pages[0], 128u); /* Ebene 1: 512 KiB */
	T_EQ(l8.level_pages[1], 1u);   /* Ebene 2: eine Seite */
	/* Baum gesamt 16 513 Seiten = 64,50 MiB = 0,79 % von 8 GiB. */
	T_EQ(l8.leaf_pages + l8.level_pages[0] + l8.level_pages[1], 16513u);

	osk_test_case("Baumgroesse fuer 16 GiB");
	T_OK(osk_vol_layout(4194304u, &l16));
	T_EQ(l16.leaf_pages, 32768u); /* 128 MiB */
	T_EQ(l16.levels, 3u);
	T_EQ(l16.level_pages[0], 256u);
	T_EQ(l16.level_pages[1], 2u);
	T_EQ(l16.level_pages[2], 1u);
	/* 33 027 Seiten = 129,01 MiB = 0,79 %. */
	T_EQ(l16.leaf_pages + l16.level_pages[0] + l16.level_pages[1] + l16.level_pages[2], 33027u);
	/* Der Baum liegt AUF DER KARTE. Er passt nicht in den Stick-Flash:
	 * 64,50 MiB gegen 6,82 MiB frei (docs/laufwerk.md), und auf dem
	 * Entwicklungsbrett mit 8 MB Flash erst recht nicht. Das ist der
	 * Grund, warum "8 MB beweisen nichts fuer 64 MB" den Baum nicht
	 * trifft - er war nie im Stick. */
	T_TRUE(l16.leaf_pages * OSK_VOL_BLOCK_LEN > 64u * 1024u * 1024u);

	osk_test_case("Pfadkosten: drei Lesevorgaenge, 8 KiB gehasht");
	/* Das ist die Zahl, die der Nutzer als Verzoegerung spuert. */
	T_EQ(osk_vol_path_reads(&l8), 3u);
	T_EQ(osk_vol_path_reads(&l16), 3u);
	T_EQ(osk_vol_path_hash_bytes(&l8), 8192u);
	T_EQ(osk_vol_path_hash_bytes(&l16), 8192u);
	/* Die Spitze liegt im RAM - sonst waeren es mehr. */
	T_EQ(l16.cached_levels, 2u);
	T_EQ(l8.cached_levels, 1u);

	osk_test_case("Vollstaendige Pruefung: 11 Minuten, und sie wird genannt");
	/* 25,8 MB/s ist Nordics Vergleichswert fuer den beschleunigten
	 * SHA-256 - UNGEPRUEFT, aus dem Konzept uebernommen. */
	T_TRUE(osk_volume_full_check_ms(&l16, 25800000u) > 600000u);
	T_TRUE(osk_volume_full_check_ms(&l16, 25800000u) < 700000u);
	/* In Software (870 kB/s) werden daraus Stunden. Darum steht der
	 * Befund zum Beschleuniger im Konzept so weit vorn. */
	/* 870 kB/s ist Nordics Software-Wert: aus 11 Minuten werden 5,5
	 * Stunden. Das ist der zweite Grund, warum der Befund zum
	 * Beschleuniger im Konzept so weit vorn steht. */
	T_TRUE(osk_volume_full_check_ms(&l16, 870000u) > 5u * 3600u * 1000u);
	T_TRUE(osk_volume_full_check_ms(&l16, 870000u) < 6u * 3600u * 1000u);

	osk_test_case("Eine Karte, die zu gross fuer vier Ebenen ist, wird abgewiesen");
	/* Lieber ein Fehler als ein Baum, der oben nicht zusammenlaeuft. */
	T_RES(osk_vol_layout(0u, &l8), OSK_ERR_ARG);
}

/* --- Der Aufschlag auf den Durchsatz, gemessen an der Attrappe -------- */

static void case_cost_measured(void)
{
	osk_vol_layout_t lay;
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};

	osk_test_case("Ein Lesevorgang kostet genau so viele Zugriffe wie gerechnet");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	T_OK(osk_volume_write_enable());
	fill(buf_in, 7u);
	T_OK(osk_volume_block_write(5u, buf_in));
	T_OK(osk_volume_commit());

	T_OK(osk_vol_layout(BLOCKS, &lay));
	fake_card_count_reset();
	T_OK(osk_volume_block_read(5u, buf_out));
	/* Gemessen, nicht geschaetzt: so viele Lesevorgaenge wie
	 * osk_vol_path_reads() verspricht. Laeuft das auseinander, luegt die
	 * Kostenrechnung, die der Anzeige die Verzoegerung nennt. */
	T_EQ(fake_card_reads(), osk_vol_path_reads(&lay));
	T_MEM(buf_in, buf_out, OSK_VOL_BLOCK_LEN);
	osk_volume_close();
}

/* --- Ein veraenderter Block fliegt beim LESEN auf -------------------- */

static void case_tamper(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};
	uint32_t bad = 0xffffffffu;

	osk_test_case("Veraenderter Block: beim Einstecken unentdeckt, beim Lesen erkannt");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	T_OK(osk_volume_write_enable());
	fill(buf_in, 3u);
	T_OK(osk_volume_block_write(42u, buf_in));
	T_OK(osk_volume_block_write(43u, buf_in));
	T_OK(osk_volume_commit());

	/* Der Angreifer hat die Karte in der Hand. */
	{
		osk_vol_layout_t lay;

		T_OK(osk_vol_layout(BLOCKS, &lay));
		fake_card_flip(lay.first_data_page + 42u, 100u);
	}
	T_OK(osk_volume_open(NULL));
	/* UND DAS IST DER PUNKT: die Wurzel ist in Ordnung. "Karte geprueft"
	 * waere hier eine Luege - der veraenderte Block liegt unentdeckt da,
	 * solange ihn niemand liest. */
	T_OK(osk_volume_root_unchanged());
	/* Erst beim Lesen faellt er auf - und es geht kein Byte hinaus. */
	T_RES(osk_volume_block_read(42u, buf_out), OSK_ERR_FORMAT);
	/* Der Nachbarblock ist davon unberuehrt: geprueft wird je Block. */
	T_OK(osk_volume_block_read(43u, buf_out));
	/* Die ausdrueckliche vollstaendige Pruefung nennt den Block. */
	T_RES(osk_volume_full_check_range(0u, BLOCKS, &bad), OSK_ERR_FORMAT);
	T_EQ(bad, 42u);
	osk_volume_close();

	osk_test_case("Veraenderte Stempelseite: der Pfad stimmt nicht mehr");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	T_OK(osk_volume_write_enable());
	T_OK(osk_volume_block_write(10u, buf_in));
	T_OK(osk_volume_commit());
	{
		osk_vol_layout_t lay;

		T_OK(osk_vol_layout(BLOCKS, &lay));
		/* Ein Stempel umkippen - der Block selbst bleibt unberuehrt.
		 * Damit faellt es nicht am Stempel auf, sondern am Baum. */
		fake_card_flip(vol_leaf_page_of(&lay, 10u), vol_leaf_offset_of(10u));
	}
	T_OK(osk_volume_open(NULL));
	T_RES(osk_volume_block_read(10u, buf_out), OSK_ERR_FORMAT);
	osk_volume_close();
}

/* --- Der Rueckdreh: ganzer Stand zurueckgelegt ------------------------ */

static void case_rollback(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};

	osk_test_case("Modus 1: ganzer alter Stand zurueckgelegt - der Zaehler verraet es");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	T_OK(osk_volume_write_enable());
	fill(buf_in, 1u);
	T_OK(osk_volume_block_write(1u, buf_in));
	T_OK(osk_volume_commit());

	/* Ein in sich VOELLIG GUELTIGER alter Stand - richtige Stempel,
	 * richtiger Baum, richtiges Siegel. Gegen einen verfaelschten hilft
	 * schon die Verschluesselung; gegen diesen nur der Zaehler. */
	fake_card_snapshot();
	fill(buf_in, 2u);
	T_OK(osk_volume_block_write(1u, buf_in));
	T_OK(osk_volume_commit());
	fake_card_rollback();

	T_OK(osk_volume_open(NULL));
	/* Nicht OSK_ERR_FORMAT: die Daten sind in Ordnung, sie sind nur alt. */
	T_RES(osk_volume_root_unchanged(), OSK_ERR_ROLLBACK);
	osk_volume_close();
}

/* --- Die Auflage: der Modus wird VOR dem Schreiben genannt ------------ */

static void case_mode_shown_before_write(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};

	osk_test_case("Ohne Zustimmung zur Betriebsart kein einziger Block");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	/* Keine Schreibfreigabe geholt. */
	T_RES(osk_volume_block_write(0u, buf_in), OSK_ERR_DENIED);
	T_TRUE(!osk_volume_write_enabled());

	osk_test_case("Die Anzeige sagt nein - dann wird nicht geschrieben");
	vt_ui_reset(true);
	T_RES(osk_volume_write_enable(), OSK_ERR_DENIED);
	T_EQ(vt_asked_calls(), 1u);
	T_RES(osk_volume_block_write(0u, buf_in), OSK_ERR_DENIED);

	osk_test_case("Gefragt wird VOR dem ersten Schreibzugriff auf die Karte");
	vt_ui_reset(false);
	fake_card_count_reset();
	T_OK(osk_volume_write_enable());
	/* Null Schreibzugriffe zwischen Frage und Antwort: die Anzeige kommt
	 * wirklich vorher und nicht hinterher. */
	T_EQ(fake_card_writes(), 0u);
	T_EQ(vt_asked_calls(), 1u);
	T_EQ(vt_asked_mode(), OSK_VOL_MODE_DEVICE);
	T_TRUE(vt_asked_secret());
	T_EQ(vt_asked_recipients(), 1u);
	T_OK(osk_volume_block_write(0u, buf_in));
	T_TRUE(fake_card_writes() > 0u);

	osk_test_case("Ohne Anzeige gibt es keinen Datentraeger");
	{
		osk_volume_config_t cfg = {
		    .root_key = VT_ROOT_KEY,
		    .sign_key = VT_SIGN_KEY,
		    .counter_id = VT_COUNTER,
		    .ui_confirm = NULL,
		};

		/* Nicht "dann eben ohne Anzeige". Lieber kein Datentraeger. */
		T_RES(osk_volume_init(&cfg), OSK_ERR_ARG);
	}
	T_OK(vt_init());
	osk_volume_close();
}

/* --- Ungesicherte Bloecke: die Anzeige muss es sagen koennen ---------- */

static void case_dirty(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};

	osk_test_case("Offene Bloecke werden gezaehlt, bis festgeschrieben ist");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(OSK_VOL_MODE_DEVICE, &recv, 1u, BLOCKS));
	T_OK(osk_volume_write_enable());
	T_EQ(osk_volume_dirty_blocks(), 0u);
	T_OK(osk_volume_block_write(1u, buf_in));
	T_OK(osk_volume_block_write(2u, buf_in));
	T_EQ(osk_volume_dirty_blocks(), 2u);
	T_OK(osk_volume_commit());
	T_EQ(osk_volume_dirty_blocks(), 0u);

	osk_test_case("Jede Festschreibung ist genau ein Zaehlerschritt im Chip");
	/* Nicht je Block - das waere der Zaehler als Verbrauchsgut. */
	fake_se050_raises_reset();
	T_OK(osk_volume_block_write(3u, buf_in));
	T_OK(osk_volume_block_write(4u, buf_in));
	T_OK(osk_volume_commit());
	T_EQ(fake_se050_raises(), 1u);
	osk_volume_close();
}

void test_volume(void)
{
	case_sizes();
	case_cost_measured();
	case_tamper();
	case_rollback();
	case_mode_shown_before_write();
	case_dirty();
	fake_card_free();
}
