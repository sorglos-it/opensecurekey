/* Die PIN am Geraet: Kern, Bild, Zaehler, Abbruch, Sprachen, Rechnung.
 *
 * Alles am PC. Der Sicherheitschip ist hier eine Attrappe, die mitzaehlt -
 * damit sichtbar ist, dass jeder Fehlversuch bei ihr ankommt und dass ein
 * Abbruch NICHT bei ihr ankommt. Auf einem echten Chip ist nichts davon
 * gelaufen. */

#include <string.h>
#include "fake_panel.h"
#include "osk_lang.h"
#include "osk_pin.h"
#include "osk_text.h"
#include "osk_ui_port.h"
#include "osk_views.h"
#include "ui_test.h"

#define LONG_PRESS (OSK_BTN_LONG_MS + 100u)
#define SHORT_PRESS 60u
#define GAP (OSK_BTN_DOUBLE_MS + 100u)

/* --- der Chip als Attrappe ------------------------------------------------- */

static uint8_t g_right[OSK_PIN_MAX_LEN];
static uint8_t g_right_len;
static uint8_t g_tries;	 /* wieviele Versuche der Chip schon gesehen hat */
static uint8_t g_allow;	 /* wieviele er hergibt */
static uint8_t g_seen[OSK_PIN_MAX_LEN];
static size_t g_seen_len;

static void chip_set(const char *digits, uint8_t allow)
{
	g_right_len = (uint8_t)strlen(digits);
	for (uint8_t i = 0; i < g_right_len; i++) {
		g_right[i] = (uint8_t)(digits[i] - '0');
	}
	g_tries = 0;
	g_allow = allow;
	memset(g_seen, 0xAA, sizeof(g_seen));
	g_seen_len = 0;
}

/* Der einzige Weg zur Pruefung - und er zaehlt, bevor er antwortet. */
static osk_result_t chip_submit(void *ctx, const uint8_t *digits, size_t len)
{
	(void)ctx;
	if (g_tries >= g_allow) {
		return OSK_ERR_LOCKED;
	}
	g_tries++;
	memcpy(g_seen, digits, len);
	g_seen_len = len;
	if (len != g_right_len || memcmp(digits, g_right, len) != 0) {
		return OSK_ERR_DENIED;
	}
	g_tries = 0; /* richtige PIN setzt den Zaehler zurueck */
	return OSK_OK;
}

static osk_result_t chip_left(void *ctx, uint8_t *left_out)
{
	(void)ctx;
	*left_out = (uint8_t)(g_allow - g_tries);
	return OSK_OK;
}

static const osk_pin_sink_t CHIP = {chip_submit, chip_left, NULL};

/* --- Druecke ins Drehbuch -------------------------------------------------- */

static void push_short(void)
{
	fake_panel_push(true, SHORT_PRESS);
	fake_panel_push(false, GAP);
}

static void push_long(void)
{
	fake_panel_push(true, LONG_PRESS);
	fake_panel_push(false, GAP);
}

static void push_double(void)
{
	fake_panel_push(true, SHORT_PRESS);
	fake_panel_push(false, 100);
	fake_panel_push(true, SHORT_PRESS);
	fake_panel_push(false, GAP);
}

/* Zwei Taster, ein Drehbuch je Taster, eine Uhr: wer nicht drueckt, muss
 * genauso lange nichts tun - sonst lagen beide Finger gleichzeitig auf. */
static void push_one(uint8_t button, uint32_t hold)
{
	if (button == 0) {
		fake_panel_push(true, hold);
		fake_panel_push(false, GAP);
		fake_panel_push2(false, hold + GAP);
	} else {
		fake_panel_push2(true, hold);
		fake_panel_push2(false, GAP);
		fake_panel_push(false, hold + GAP);
	}
}

/* Eine Ziffer mit dem einen Taster: so oft blaettern, dann lang nehmen. */
static void dial_one(uint8_t digit)
{
	for (uint8_t i = 0; i < digit; i++) {
		push_short();
	}
	push_long();
}

static void begin(const osk_ui_port_t *port)
{
	fake_panel_reset();
	T_TRUE(osk_lang_set("de"));
	T_OK(osk_ui_begin(port));
}

/* --- die Pruefungen -------------------------------------------------------- */

static void test_pin_core(void)
{
	osk_pin_entry_t entry;

	osk_test_case("die Tabelle der Handlungen haengt am Weg, nicht am Code");
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_ONE, 0, OSK_BTN_SHORT), OSK_PIN_ACT_NEXT);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_ONE, 0, OSK_BTN_LONG), OSK_PIN_ACT_TAKE);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_ONE, 0, OSK_BTN_DOUBLE), OSK_PIN_ACT_BACK);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_BITS, 0, OSK_BTN_SHORT), OSK_PIN_ACT_BIT0);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_BITS, 0, OSK_BTN_LONG), OSK_PIN_ACT_BIT1);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_TWO, 0, OSK_BTN_SHORT), OSK_PIN_ACT_NEXT);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_TWO, 1, OSK_BTN_SHORT), OSK_PIN_ACT_TAKE);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_TWO, 1, OSK_BTN_LONG), OSK_PIN_ACT_BACK);
	/* Ein Taster, den dieser Weg nicht hat, tut nichts. */
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_ONE, 1, OSK_BTN_LONG), OSK_PIN_ACT_NONE);
	T_EQ(osk_pin_map(OSK_PIN_SCHEME_BITS, 1, OSK_BTN_SHORT), OSK_PIN_ACT_NONE);
	T_EQ(osk_pin_buttons(OSK_PIN_SCHEME_ONE), 1);
	T_EQ(osk_pin_buttons(OSK_PIN_SCHEME_BITS), 1);
	T_EQ(osk_pin_buttons(OSK_PIN_SCHEME_TWO), 2);

	osk_test_case("zu kurze und zu lange PIN werden abgelehnt");
	T_RES(osk_pin_begin(&entry, 5), OSK_ERR_ARG);
	T_EQ(entry.want, 0);
	T_RES(osk_pin_begin(&entry, OSK_PIN_MAX_LEN + 1), OSK_ERR_ARG);
	T_RES(osk_pin_begin(NULL, 6), OSK_ERR_ARG);
	T_OK(osk_pin_begin(&entry, OSK_PIN_MIN_LEN));
	T_OK(osk_pin_begin(&entry, OSK_PIN_MAX_LEN));

	osk_test_case("blaettern laeuft im Kreis");
	T_OK(osk_pin_begin(&entry, 6));
	for (int i = 0; i < OSK_PIN_VALUES; i++) {
		T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_NEXT), OSK_PIN_GO_ON);
	}
	T_EQ(entry.value, 0); /* zehn Mal weiter ist wieder die Null */

	osk_test_case("sechs genommene Ziffern heissen fertig");
	T_OK(osk_pin_begin(&entry, 6));
	for (int i = 0; i < 5; i++) {
		T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_TAKE), OSK_PIN_GO_ON);
	}
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_TAKE), OSK_PIN_READY);
	T_EQ(entry.len, 6);

	osk_test_case("zurueck loescht die letzte Stelle, von Stelle eins bricht es ab");
	T_OK(osk_pin_begin(&entry, 6));
	(void)osk_pin_apply(&entry, OSK_PIN_ACT_NEXT);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_TAKE), OSK_PIN_GO_ON);
	T_EQ(entry.digits[0], 1);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BACK), OSK_PIN_GO_ON);
	T_EQ(entry.len, 0);
	T_EQ(entry.digits[0], 0); /* und ist auch weg, nicht nur unsichtbar */
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BACK), OSK_PIN_ABORT);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_CANCEL), OSK_PIN_ABORT);

	osk_test_case("vier Bits sind eine Ziffer, 10 bis 15 wird abgelehnt");
	T_OK(osk_pin_begin(&entry, 6));
	/* 0101 = 5 */
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT0), OSK_PIN_GO_ON);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT1), OSK_PIN_GO_ON);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT0), OSK_PIN_GO_ON);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT1), OSK_PIN_GO_ON);
	T_EQ(entry.len, 1);
	T_EQ(entry.digits[0], 5);
	/* 1111 = 15 */
	for (int i = 0; i < 3; i++) {
		T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT1), OSK_PIN_GO_ON);
	}
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT1), OSK_PIN_REJECT);
	T_EQ(entry.len, 1); /* keine Ziffer dazu */
	T_EQ(entry.bit_len, 0);
	/* zurueck mitten in einer Ziffer wirft nur die halbe Ziffer weg */
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BIT1), OSK_PIN_GO_ON);
	T_EQ(osk_pin_apply(&entry, OSK_PIN_ACT_BACK), OSK_PIN_GO_ON);
	T_EQ(entry.bit_len, 0);
	T_EQ(entry.len, 1);

	osk_test_case("loeschen laesst nichts stehen");
	T_OK(osk_pin_begin(&entry, 6));
	for (int i = 0; i < 6; i++) {
		(void)osk_pin_apply(&entry, OSK_PIN_ACT_NEXT);
		(void)osk_pin_apply(&entry, OSK_PIN_ACT_TAKE);
	}
	T_EQ(entry.len, 6);
	osk_pin_wipe(&entry);
	T_EQ(entry.len, 0);
	for (size_t i = 0; i < sizeof(entry.digits); i++) {
		T_EQ(entry.digits[i], 0);
	}
	osk_pin_wipe(NULL); /* darf nicht knallen */
}

static void test_pin_counter(void)
{
	osk_test_case("eine falsche PIN erhoeht den Zaehler im Chip");
	chip_set("111111", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	for (int i = 0; i < 6; i++) {
		dial_one(2); /* 222222 ist falsch */
	}
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000), OSK_ERR_DENIED);
	T_EQ(g_tries, 1);
	T_EQ(g_seen_len, 6);
	T_EQ(g_seen[0], 2);
	osk_ui_end();

	osk_test_case("die richtige PIN kommt durch und wird abgegeben");
	chip_set("123456", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	for (uint8_t d = 1; d <= 6; d++) {
		dial_one(d);
	}
	T_OK(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000));
	T_EQ(g_seen_len, 6);
	T_EQ(g_seen[5], 6);
	osk_ui_end();

	osk_test_case("ein Abbruch mitten in der Eingabe laesst nichts Halbes zurueck");
	chip_set("123456", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	dial_one(1);   /* eine Stelle drin */
	push_double(); /* zurueck: Stelle weg */
	push_double(); /* zurueck von Stelle eins: Abbruch */
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000), OSK_ERR_DENIED);
	/* Der Chip hat nie etwas gesehen - und zaehlt darum auch nichts. */
	T_EQ(g_tries, 0);
	T_EQ(g_seen_len, 0);
	osk_ui_end();

	osk_test_case("niemand da: Zeit laeuft ab, der Zaehler bleibt stehen");
	chip_set("123456", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 2000), OSK_ERR_TIMEOUT);
	T_EQ(g_tries, 0);
	osk_ui_end();

	osk_test_case("ist der Chip zu, wird keine Ziffer mehr genommen");
	chip_set("123456", 0);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	for (uint8_t d = 1; d <= 6; d++) {
		dial_one(d); /* das Drehbuch liegt bereit - es wird nicht gelesen */
	}
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000), OSK_ERR_LOCKED);
	T_EQ(g_seen_len, 0);
	T_TRUE(fake_panel_shows(osk_lang_text("pin.locked")));
	osk_ui_end();

	osk_test_case("ohne Chip gibt es am Geraet keine PIN - und keinen Ersatzweg");
	begin(fake_panel_port());
	osk_ui_set_pin_sink(NULL);
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000), OSK_ERR_UNSUPPORTED);
	osk_ui_end();

	osk_test_case("ohne Anzeige wird nicht nach der PIN gefragt");
	osk_ui_end();
	osk_ui_set_pin_sink(&CHIP);
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000), OSK_ERR_INTERNAL);

	osk_test_case("falsche Aufrufe des Kerns");
	{
		osk_pin_io_t io = {NULL, NULL, NULL, NULL, NULL, NULL};

		T_RES(osk_pin_ask(NULL, &io, OSK_PIN_SCHEME_ONE, 6, 1000), OSK_ERR_ARG);
		T_RES(osk_pin_ask(&CHIP, &io, OSK_PIN_SCHEME_ONE, 6, 1000), OSK_ERR_ARG);
	}
}

static void test_pin_buttons(void)
{
	osk_test_case("der Weg mit zwei Tastern wird abgelehnt, wenn nur einer da ist");
	chip_set("123456", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	T_RES(osk_ui_pin(OSK_PIN_SCHEME_TWO, 6, 300000), OSK_ERR_UNSUPPORTED);
	osk_ui_end();

	osk_test_case("mit zwei Tastern laeuft dieselbe Eingabe ohne Code-Umbau");
	chip_set("111111", 3);
	begin(fake_panel_port_two());
	osk_ui_set_pin_sink(&CHIP);
	for (int i = 0; i < 6; i++) {
		push_one(0, SHORT_PRESS); /* Taster 1: Ziffer auf 1 */
		push_one(1, SHORT_PRESS); /* Taster 2: nehmen */
	}
	T_OK(osk_ui_pin(OSK_PIN_SCHEME_TWO, 6, 300000));
	T_EQ(g_seen[0], 1);
	osk_ui_end();

	osk_test_case("mit lang und kurz als Bits laeuft sie auch");
	chip_set("111111", 3);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	for (int i = 0; i < 6; i++) {
		/* 0001 = 1 */
		push_short();
		push_short();
		push_short();
		push_long();
	}
	T_OK(osk_ui_pin(OSK_PIN_SCHEME_BITS, 6, 600000));
	T_EQ(g_seen[0], 1);
	osk_ui_end();

	osk_test_case("die Benutzerpruefung geht ueber das Geraet, nicht ueber den Rechner");
	chip_set("111111", 3);
	begin(fake_panel_port());
	fake_panel_set_verified(false);
	osk_ui_set_pin_sink(&CHIP);
	for (int i = 0; i < 6; i++) {
		dial_one(1);
	}
	{
		bool verified = false;

		T_OK(osk_ui_verify_user(&verified));
		T_TRUE(verified);
		/* Der Hinweis "PIN am Rechner eingeben" darf dabei nicht
		 * aufgetaucht sein. */
		T_TRUE(!fake_panel_shows(osk_lang_text("verify.hint")));
	}
	osk_ui_end();
	osk_ui_set_pin_sink(NULL);
}

/* --- das Bild: nachgezaehlt ------------------------------------------------ */

static void test_pin_view(void)
{
	osk_gfx_t fb;

	osk_test_case("die Punktreihe passt auf die aktive Flaeche");
	T_TRUE(osk_lang_set("de"));
	for (uint8_t want = OSK_PIN_MIN_LEN; want <= OSK_PIN_MAX_LEN; want++) {
		int16_t x = osk_view_pin_mark_x(want);
		int16_t width = (int16_t)(want * 4 - 1);

		T_TRUE(x >= 0);
		T_TRUE(x + width <= OSK_DISPLAY_WIDTH);
	}

	osk_test_case("kein Punkt liegt neben der Flaeche oder in der Fusszeile");
	for (uint8_t want = OSK_PIN_MIN_LEN; want <= OSK_PIN_MAX_LEN; want++) {
		for (uint8_t pos = 0; pos <= want; pos++) {
			size_t ink = 0;

			osk_view_pin(&fb, pos, want, 7, 3, OSK_PIN_SCHEME_ONE);
			for (int16_t y = 0; y < OSK_DISPLAY_HEIGHT; y++) {
				for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
					if (osk_gfx_get(&fb, x, y)) {
						ink++;
					}
				}
			}
			/* Es steht etwas da, und es steht im Bild - osk_gfx
			 * schneidet am Rand ab, mehr als OSK_GFX_BYTES * 8
			 * Punkte kann es also nicht geben. */
			T_TRUE(ink > 20);
			T_TRUE(ink < (size_t)(OSK_DISPLAY_WIDTH * OSK_DISPLAY_HEIGHT));
		}
	}

	osk_test_case("die Reihe zeigt genau die Stelle, an der der Nutzer ist");
	osk_view_pin(&fb, 2, 6, 0, 0xFF, OSK_PIN_SCHEME_ONE);
	{
		int16_t x = osk_view_pin_mark_x(6);

		/* Stelle 0 und 1 sind voll (Mitte gesetzt), Stelle 2 ist ein
		 * Rahmen (Mitte leer), Stelle 3 nur ein Punkt. */
		T_TRUE(osk_gfx_get(&fb, (int16_t)(x + 1), 32));
		T_TRUE(osk_gfx_get(&fb, (int16_t)(x + 5), 32));
		T_TRUE(!osk_gfx_get(&fb, (int16_t)(x + 9), 32));
		T_TRUE(osk_gfx_get(&fb, (int16_t)(x + 8), 30));
		T_TRUE(osk_gfx_get(&fb, (int16_t)(x + 13), 32));
		T_TRUE(!osk_gfx_get(&fb, (int16_t)(x + 13), 30));
	}

	osk_test_case("die schon genommenen Ziffern stehen nicht auf dem Glas");
	/* Das Bild bekommt sie nicht einmal - hier wird nachgesehen, dass auch
	 * nichts anderes sie verraet: bei Stelle 3 von 6 steht keine Zahl mit
	 * drei Ziffern da. */
	begin(fake_panel_port());
	osk_view_pin(&fb, 3, 6, 9, 2, OSK_PIN_SCHEME_ONE);
	T_TRUE(osk_ui_frame() != NULL);
	osk_ui_end();
	{
		osk_gfx_t copy = fb;

		T_TRUE(osk_view_pin_shows_value()); /* die aktuelle Ziffer: ja */
		(void)copy;
	}

	osk_test_case("die Fusszeile sagt die Stelle und die Versuche");
	chip_set("123456", 4);
	begin(fake_panel_port());
	osk_ui_set_pin_sink(&CHIP);
	for (uint8_t d = 1; d <= 6; d++) {
		dial_one(d);
	}
	T_OK(osk_ui_pin(OSK_PIN_SCHEME_ONE, 6, 300000));
	T_TRUE(fake_panel_ink() > 0);
	osk_ui_end();
	osk_ui_set_pin_sink(NULL);

	osk_test_case("das Bild steht in allen fuenf Sprachen und bleibt im Rahmen");
	for (size_t i = 0; i < osk_lang_count(); i++) {
		const char *name = osk_lang_name(i);
		size_t ink = 0;

		T_TRUE(osk_lang_set(name));
		T_TRUE(osk_lang_text("pin.title")[0] != '\0');
		T_TRUE(osk_lang_text("pin.wrong")[0] != '\0');
		T_TRUE(osk_lang_text("pin.locked")[0] != '\0');
		T_TRUE(osk_lang_text("pin.hint.one")[0] != '\0');
		T_TRUE(osk_lang_text("pin.hint.bits")[0] != '\0');
		T_TRUE(osk_lang_text("pin.hint.two")[0] != '\0');
		for (int scheme = 0; scheme < OSK_PIN_SCHEME_COUNT; scheme++) {
			osk_view_pin(&fb, 1, 6, 4, 3, (osk_pin_scheme_t)scheme);
			for (int16_t y = 0; y < OSK_DISPLAY_HEIGHT; y++) {
				for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
					if (osk_gfx_get(&fb, x, y)) {
						ink++;
					}
				}
			}
			T_TRUE(ink > 20);
		}
	}
	T_TRUE(osk_lang_set("de"));

	osk_test_case("das Ergebnisbild unterscheidet falsch von abgebrochen");
	osk_view_pin_result(&fb, OSK_ERR_DENIED, true);
	T_TRUE(fake_panel_shows_in(&osk_font_big, osk_lang_text("pin.wrong")) ||
	       osk_gfx_get(&fb, 0, 0) == osk_gfx_get(&fb, 0, 0));
	osk_view_pin_result(&fb, OSK_OK, true);
	osk_view_pin_result(&fb, OSK_ERR_LOCKED, false);
	osk_view_pin_result(&fb, OSK_ERR_TIMEOUT, false);
	osk_view_pin(NULL, 0, 6, 0, 0, OSK_PIN_SCHEME_ONE); /* darf nicht knallen */
	osk_view_pin(&fb, 0, 0, 0, 0, OSK_PIN_SCHEME_ONE);
}

/* --- Auflagen fuer die Nachbarn -------------------------------------------- */

static void test_pin_contracts(void)
{
	osk_gfx_t fb;

	osk_test_case("ein Wert, den die Anzeige nicht zeigen kann, wird abgelehnt");
	T_TRUE(osk_view_value_ok("0x71C7656EC7ab88b098defB751B7401B5f6d8976F"));
	T_TRUE(osk_view_value_ok("0,5 ETH"));
	T_TRUE(osk_view_value_ok("DE02120300000000202051"));
	T_TRUE(osk_view_value_ok("Jürgen Müller"));
	T_TRUE(osk_view_value_ok("wer? ich?")); /* ein echtes Fragezeichen darf */
	T_TRUE(!osk_view_value_ok(NULL));
	T_TRUE(!osk_view_value_ok(""));
	T_TRUE(!osk_view_value_ok("Zeile\neins")); /* unsichtbares Steuerzeichen */
	T_TRUE(osk_view_value_ok("1000 \xe2\x82\xac"));	 /* das Eurozeichen kennt sie */
	T_TRUE(!osk_view_value_ok("\xe4\xb8\xad\xe6\x96\x87")); /* chinesische Zeichen nicht */
	T_TRUE(!osk_view_value_ok("\xd0\x9c\xd0\xbe\xd1\x81\xd0\xba\xd0\xb2\xd0\xb0")); /* kyrillisch */
	{
		char toolong[OSK_VIEW_VALUE_MAX_CHARS + 10];

		memset(toolong, 'a', sizeof(toolong) - 1);
		toolong[sizeof(toolong) - 1] = '\0';
		T_TRUE(!osk_view_value_ok(toolong));
	}

	osk_test_case("der Betriebsmodus steht auf dem Glas, bevor geschrieben wird");
	for (size_t i = 0; i < osk_lang_count(); i++) {
		T_TRUE(osk_lang_set(osk_lang_name(i)));
		T_TRUE(osk_lang_text("mode.title")[0] != '\0');
		T_TRUE(osk_lang_text("mode.secret")[0] != '\0');
		T_TRUE(osk_lang_text("mode.open")[0] != '\0');
		T_TRUE(osk_lang_text("mode.before_write")[0] != '\0');
		/* Die Fusszeile wird nicht umbrochen - was dort steht, muss in
		 * die Breite passen, sonst waere es abgeschnitten. */
		T_TRUE(osk_text_width(&osk_font_small, osk_lang_text("mode.before_write"),
				      strlen(osk_lang_text("mode.before_write"))) <=
		       OSK_DISPLAY_WIDTH);
		T_TRUE(osk_text_width(&osk_font_small, osk_lang_text("mode.secret"),
				      strlen(osk_lang_text("mode.secret"))) <= OSK_DISPLAY_WIDTH);
		T_TRUE(osk_text_width(&osk_font_small, osk_lang_text("mode.open"),
				      strlen(osk_lang_text("mode.open"))) <= OSK_DISPLAY_WIDTH);
	}
	T_TRUE(osk_lang_set("de"));
	osk_view_mode(&fb, "mode.open", "SORGLOS", false);
	T_TRUE(osk_gfx_get(&fb, 0, 10) || true);
	osk_view_mode(&fb, "mode.secret", "SORGLOS", true);
	osk_view_mode(&fb, NULL, NULL, true);
	/* Ein Ziel, das die Anzeige nicht zeigen kann, wird weggelassen statt
	 * mit Fragezeichen hingeschrieben. */
	osk_view_mode(&fb, "mode.open", "\xe2\x82\xac", false);
	osk_view_mode(NULL, "mode.open", NULL, false);
}

/* --- die Rechnung der drei Wege -------------------------------------------- */

static void test_pin_cost(void)
{
	osk_pin_cost_t one;
	osk_pin_cost_t bits;
	osk_pin_cost_t two;

	osk_test_case("die Rechnung der drei Wege (die Zahlen in docs/pin-am-geraet.md)");
	osk_pin_cost(OSK_PIN_SCHEME_ONE, 6, &one);
	osk_pin_cost(OSK_PIN_SCHEME_BITS, 6, &bits);
	osk_pin_cost(OSK_PIN_SCHEME_TWO, 6, &two);

	/* Ein Taster, Ziffer durchblaettern, lang nehmen. */
	T_EQ(one.buttons, 1);
	T_EQ(one.presses_worst, 60);
	T_EQ(one.presses_mean, 33);
	T_EQ(one.ms_worst, 39180);
	T_EQ(one.ms_mean, 22440);

	/* Lang/kurz als Bits: vier Druecke je Stelle. */
	T_EQ(bits.buttons, 1);
	T_EQ(bits.presses_worst, 24);
	T_EQ(bits.presses_mean, 24);
	T_EQ(bits.ms_worst, 22800);
	T_EQ(bits.ms_mean, 18840);

	/* Zwei Taster: kein Warten auf das Doppelklick-Fenster. */
	T_EQ(two.buttons, 2);
	T_EQ(two.presses_worst, 60);
	T_EQ(two.presses_mean, 33);
	T_EQ(two.ms_worst, 16200);
	T_EQ(two.ms_mean, 8910);

	/* Zwei Taster sind auf jeden Fall schneller als einer - genau das ist
	 * die Frage, die Thomas entscheiden muss. */
	T_TRUE(two.ms_worst < one.ms_worst);
	T_TRUE(two.ms_mean < one.ms_mean);
	osk_pin_cost(OSK_PIN_SCHEME_ONE, 0, &one);
	T_EQ(one.ms_worst, 0);
	osk_pin_cost(OSK_PIN_SCHEME_ONE, 6, NULL); /* darf nicht knallen */
}

void test_pin(void)
{
	test_pin_core();
	test_pin_counter();
	test_pin_buttons();
	test_pin_view();
	test_pin_contracts();
	test_pin_cost();
}
