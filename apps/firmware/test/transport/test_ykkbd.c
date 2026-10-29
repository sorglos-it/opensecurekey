/* Pruefungen der Tastaturseite - das statische Passwort.
 *
 * Was hier bewiesen wird: die Werkseinstellung der Abbildung passt zur
 * HID-Tastentabelle, ein Zeichen ausserhalb der Abbildung tippt NICHTS
 * statt die Haelfte, und zwischen zwei Zeichen steht immer ein
 * Loslassen - sonst kommt "aa" beim Rechner als ein "a" an. */
#include "yk_test.h"
#include "usb_kbd.h"

/* Mitschrift der hinausgehenden Berichte. */
#define LOG_MAX 512
static struct {
	uint8_t rpt[LOG_MAX][OSK_KBD_RPT_LEN];
	size_t n;
} log_;

static void sink(const uint8_t rpt[OSK_KBD_RPT_LEN], void *ctx)
{
	(void)ctx;
	if (log_.n < LOG_MAX) {
		memcpy(log_.rpt[log_.n], rpt, OSK_KBD_RPT_LEN);
		log_.n++;
	}
}

static void start(void)
{
	memset(&log_, 0, sizeof(log_));
	osk_kbd_init(sink, NULL);
}

/* Tippt zu Ende und liefert die Zahl der Berichte. */
static size_t run_out(void)
{
	unsigned guard = 0;

	while (osk_kbd_pump()) {
		if (++guard > 4u * OSK_KBD_QUEUE_MAX) {
			t_fail("das Tippen endet nicht", __FILE__, __LINE__, NULL);
			break;
		}
	}
	return log_.n;
}

static void default_map_matches_hid_table(void)
{
	uint8_t code = 0;

	start();
	/* Gegenprobe gegen die HID-Tastentabelle, nicht gegen die eigene
	 * Tabelle: 'c' ist Platz 0x06, 'b' ist 0x05, '0' ist 0x27,
	 * '1' ist 0x1e. Tabulator 0x2b, Eingabe 0x28. */
	T_TRUE("c steht in der Abbildung", osk_kbd_scan_code('c', &code));
	T_EQ("c ist 0x06", code, 0x06);
	T_TRUE("b steht", osk_kbd_scan_code('b', &code));
	T_EQ("b ist 0x05", code, 0x05);
	T_TRUE("0 steht", osk_kbd_scan_code('0', &code));
	T_EQ("0 ist 0x27", code, 0x27);
	T_TRUE("1 steht", osk_kbd_scan_code('1', &code));
	T_EQ("1 ist 0x1e", code, 0x1e);
	T_TRUE("Tabulator steht", osk_kbd_scan_code('\t', &code));
	T_EQ("Tabulator ist 0x2b", code, 0x2b);
	T_TRUE("Eingabe steht", osk_kbd_scan_code('\r', &code));
	T_EQ("Eingabe ist 0x28", code, 0x28);

	/* Grossbuchstaben sind derselbe Platz mit gesetztem Umschalterbit. */
	uint8_t lower = 0;

	osk_kbd_scan_code('d', &lower);
	T_TRUE("D steht", osk_kbd_scan_code('D', &code));
	T_EQ("D ist d mit Umschalter", code, lower | OSK_KBD_SHIFT);

	/* Das Ausrufezeichen ist die 1 mit Umschalter. */
	uint8_t one = 0;

	osk_kbd_scan_code('1', &one);
	T_TRUE("! steht", osk_kbd_scan_code('!', &code));
	T_EQ("! ist 1 mit Umschalter", code, one | OSK_KBD_SHIFT);

	/* Was nicht in der Reihenfolge steht, steht nicht darin. 'a', 'm',
	 * 'o', 'q', 's', 'w', 'x', 'y', 'z' fehlen - Modhex benutzt sie
	 * nicht. */
	const char *missing = "amoqswxyz @.-_";

	for (const char *p = missing; *p; p++) {
		if (osk_kbd_scan_code(*p, &code)) {
			char d[48];

			snprintf(d, sizeof(d), "'%c' wurde gefunden", *p);
			t_fail("Zeichen ausserhalb der Abbildung", __FILE__, __LINE__, d);
		}
		t_checks++;
	}

	/* Die Reihenfolge und die Werkseinstellung muessen gleich lang
	 * sein - sonst zeigt ein Zeichen auf den Code des naechsten. Das
	 * ist genau die Art Fehler, die still falsche Passwoerter tippt. */
	T_EQ("Reihenfolge hat 45 Zeichen", sizeof(OSK_KBD_SCAN_ORDER) - 1u,
	     OSK_KBD_SCAN_MAP_LEN);
}

static void typing_sequence(void)
{
	start();
	T_EQ("cb tippen", osk_kbd_type("cb", 2u), OSK_OK);
	T_TRUE("es wird getippt", osk_kbd_busy());
	T_EQ("vier Berichte fuer zwei Zeichen", run_out(), 4);
	T_TRUE("danach ist Ruhe", !osk_kbd_busy());

	T_EQ("1: c gedrueckt", log_.rpt[0][2], 0x06);
	T_EQ("1: kein Umschalter", log_.rpt[0][0], 0x00);
	T_EQ("2: losgelassen", log_.rpt[1][2], 0x00);
	T_EQ("3: b gedrueckt", log_.rpt[2][2], 0x05);
	T_EQ("4: losgelassen", log_.rpt[3][2], 0x00);

	/* Dasselbe Zeichen zweimal: ohne das Loslassen zwischendurch
	 * sieht der Rechner nur einen Druck. */
	start();
	osk_kbd_type("cc", 2u);
	run_out();
	T_EQ("1: c", log_.rpt[0][2], 0x06);
	T_EQ("2: los", log_.rpt[1][2], 0x00);
	T_EQ("3: c wieder", log_.rpt[2][2], 0x06);
	T_EQ("4: los", log_.rpt[3][2], 0x00);

	/* Grossbuchstabe: Umschalter im Bericht, nicht im Tastencode. */
	start();
	osk_kbd_type("C", 1u);
	run_out();
	T_EQ("Umschalter links gesetzt", log_.rpt[0][0], OSK_KBD_MOD_LSHIFT);
	T_EQ("und der Platz ohne das Bit 0x80", log_.rpt[0][2], 0x06);
	T_EQ("Byte 1 bleibt reserviert", log_.rpt[0][1], 0x00);
	T_EQ("beim Loslassen kein Umschalter", log_.rpt[1][0], 0x00);
}

static void all_or_nothing(void)
{
	start();
	/* 'x' steht nicht in der Abbildung: NICHTS wird getippt, auch
	 * nicht die drei Zeichen davor. */
	T_EQ("unbekanntes Zeichen scheitert", osk_kbd_type("cbdx", 4u), OSK_ERR_UNSUPPORTED);
	T_TRUE("es wird nicht getippt", !osk_kbd_busy());
	T_EQ("kein einziger Bericht ist hinausgegangen", log_.n, 0);

	/* Zu lang. */
	{
		char big[OSK_KBD_QUEUE_MAX + 2];

		memset(big, 'c', sizeof(big));
		T_EQ("zu lang wird abgewiesen",
		     osk_kbd_type(big, (size_t)OSK_KBD_QUEUE_MAX + 1u), OSK_ERR_FULL);
		T_EQ("kein Bericht", log_.n, 0);
		/* Genau die Grenze geht. */
		T_EQ("die Grenze geht", osk_kbd_type(big, OSK_KBD_QUEUE_MAX), OSK_OK);
		T_EQ("zwei Berichte je Zeichen", run_out(), 2u * OSK_KBD_QUEUE_MAX);
	}

	start();
	T_EQ("Laenge 0 wird abgewiesen", osk_kbd_type("c", 0u), OSK_ERR_ARG);
	T_EQ("NULL wird abgewiesen", osk_kbd_type(NULL, 3u), OSK_ERR_ARG);

	/* Zwei Auftraege gleichzeitig gehen nicht. */
	T_EQ("erster Auftrag", osk_kbd_type("cbd", 3u), OSK_OK);
	T_EQ("zweiter wird abgewiesen", osk_kbd_type("efg", 3u), OSK_ERR_EXISTS);
	osk_kbd_cancel();
	T_TRUE("nach dem Abbruch ist Ruhe", !osk_kbd_busy());
	T_TRUE("und pump tut nichts mehr", !osk_kbd_pump());
	/* Jetzt geht wieder einer. */
	T_EQ("danach geht es wieder", osk_kbd_type("efg", 3u), OSK_OK);
}

static void scan_map_write(void)
{
	uint8_t map[OSK_KBD_SCAN_MAP_LEN];
	uint8_t code = 0;

	start();
	/* Eine eigene Belegung: alles auf 0x04 ('a' im englischen Satz). */
	memset(map, 0x04, sizeof(map));
	T_EQ("Abbildung setzen", osk_kbd_set_scan_map(map), OSK_OK);
	T_TRUE("c steht noch", osk_kbd_scan_code('c', &code));
	T_EQ("c zeigt jetzt auf 0x04", code, 0x04);
	T_EQ("NULL wird abgewiesen", osk_kbd_set_scan_map(NULL), OSK_ERR_ARG);

	/* Mitten im Tippen darf nicht umgestellt werden. */
	osk_kbd_type("cbd", 3u);
	T_EQ("waehrend des Tippens nicht umstellen", osk_kbd_set_scan_map(map),
	     OSK_ERR_EXISTS);
	osk_kbd_cancel();

	/* Zuruecksetzen bringt die Werkseinstellung. */
	osk_kbd_reset_scan_map();
	T_TRUE("c steht", osk_kbd_scan_code('c', &code));
	T_EQ("und zeigt wieder auf 0x06", code, 0x06);

	/* Die Abbildung ist von aussen lesbar - der Lesebefehl braucht
	 * das, und die Pruefung auch. */
	T_TRUE("Abbildung lesbar", osk_kbd_scan_map() != NULL);
	T_EQ("erster Eintrag ist c", osk_kbd_scan_map()[0], 0x06);
	T_EQ("letzter Eintrag ist die Eingabetaste",
	     osk_kbd_scan_map()[OSK_KBD_SCAN_MAP_LEN - 1], 0x28);
}

static void raw_codes(void)
{
	const uint8_t codes[3] = {0x04u, 0x05u, 0x86u};

	start();
	T_EQ("rohe Codes gehen", osk_kbd_type_codes(codes, 3u), OSK_OK);
	T_EQ("sechs Berichte", run_out(), 6);
	T_EQ("erster Code", log_.rpt[0][2], 0x04);
	T_EQ("dritter Code mit Umschalter", log_.rpt[4][0], OSK_KBD_MOD_LSHIFT);
	T_EQ("und ohne das Bit 0x80", log_.rpt[4][2], 0x06);

	T_EQ("Laenge 0", osk_kbd_type_codes(codes, 0u), OSK_ERR_ARG);
	T_EQ("NULL", osk_kbd_type_codes(NULL, 3u), OSK_ERR_ARG);
	T_EQ("zu lang", osk_kbd_type_codes(codes, (size_t)OSK_KBD_QUEUE_MAX + 1u),
	     OSK_ERR_FULL);
}

void suite_ykkbd(void)
{
	printf("== Tastatur\n");
	default_map_matches_hid_table();
	typing_sequence();
	all_or_nothing();
	scan_map_write();
	raw_codes();
}
