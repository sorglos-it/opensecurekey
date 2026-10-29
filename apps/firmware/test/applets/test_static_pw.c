/* Statisches Passwort, Kartenseite: was angenommen wird, was abgelehnt
 * wird, und dass jede Herausgabe am Display sichtbar ist.
 *
 * DIE WICHTIGSTE PRUEFUNG HIER ist die Naht zu Strang E: die Karte darf
 * kein Passwort annehmen, das die Tastaturseite nicht tippen kann. Sie
 * kann 45 Zeichen, nicht 95 (OSK_KBD_SCAN_ORDER in
 * src/classes/transport/usb_kbd.h) - kein a, kein o, kein s, kein
 * Leerzeichen. Ein Passwort, das die Karte annimmt und die Tastatur dann
 * verweigert, tippt sich nie, und der Nutzer merkt es erst, wenn er es
 * braucht.
 *
 * Geprueft wird deshalb GEGEN DIE LISTE DER TASTATURSEITE und nicht gegen
 * eine Kopie davon. Das Tippen selbst prueft Strang E in
 * test/transport/.
 *
 * Am PC gegen Attrappen - Stufe 2 von vier. */
#include "test_util.h"
#include "osk_static_pw.h"
#include "osk_store.h"
#include "osk_result.h"
#include "usb_kbd.h"
#include "mocks.h"

/* Zeichen, die die Tastaturseite laut ihrer eigenen Liste tippen kann -
 * zwei davon als Beispiel, und zwei, die sie nicht kann. Sie stehen hier
 * als BUCHSTABEN und nicht als Liste: so prueft der Lauf gegen die
 * Aussage "diese Karte nimmt nur Tippbares", und nicht Liste gegen Liste. */
static void fill(osk_static_pw_t *pw, const char *text, bool enter)
{
	memset(pw, 0, sizeof(*pw));
	pw->append_enter = enter;
	pw->len = (uint8_t)strlen(text);
	memcpy(pw->text, text, pw->len);
}

/* Die Naht: jedes Zeichen aus der Liste der Tastaturseite muss
 * angenommen, jedes andere abgelehnt werden. Alle 256 Bytewerte werden
 * einzeln durchgegangen - nicht die erwarteten gesucht, sondern alle
 * angesehen. */
static void test_matches_keyboard(void)
{
	static const char order[] = OSK_KBD_SCAN_ORDER;
	unsigned accepted = 0;
	unsigned refused = 0;

	T_EQ("die Liste der Tastaturseite hat 45 Zeichen", sizeof(order) - 1u,
	     (size_t)OSK_KBD_SCAN_MAP_LEN);

	for (unsigned v = 0; v < 256u; v++) {
		const uint8_t c = (uint8_t)v;
		bool in_order = false;
		const bool ok = osk_static_pw_text_ok(&c, 1u);

		for (size_t i = 0; i + 1u < sizeof(order); i++) {
			if ((uint8_t)order[i] == c)
				in_order = true;
		}
		if (in_order) {
			accepted += ok ? 1u : 0u;
			T_TRUE("tippbares Zeichen angenommen", ok);
		} else {
			refused += ok ? 0u : 1u;
			T_TRUE("nicht tippbares Zeichen abgelehnt", !ok);
		}
	}
	T_EQ("genau 45 angenommen", accepted, (unsigned)OSK_KBD_SCAN_MAP_LEN);
	T_EQ("und 211 abgelehnt", refused, 256u - (unsigned)OSK_KBD_SCAN_MAP_LEN);

	/* Zwei Beispiele im Klartext, damit beim Lesen klar ist, was das
	 * praktisch heisst. */
	T_TRUE("'Hunter2!' geht", osk_static_pw_text_ok((const uint8_t *)"Hunter2!", 8u));
	T_TRUE("'correct horse' geht NICHT - kein o, kein Leerzeichen",
	       !osk_static_pw_text_ok((const uint8_t *)"correct horse", 13u));
	T_TRUE("ein einzelnes Leerzeichen auch nicht",
	       !osk_static_pw_text_ok((const uint8_t *)" ", 1u));
	T_TRUE("ein @ auch nicht", !osk_static_pw_text_ok((const uint8_t *)"@", 1u));
}

static void test_length_rules(void)
{
	uint8_t buf[OSK_STATIC_PW_MAX + 4u];

	/* Die Hoechstlaenge ist die der Tastatur-Warteschlange und steht
	 * nicht zweimal da. */
	T_EQ("Hoechstlaenge kommt von der Tastaturseite", (int)OSK_STATIC_PW_MAX,
	     (int)OSK_KBD_QUEUE_MAX);
	T_EQ("zwei Steckplaetze", (int)OSK_STATIC_PW_SLOTS, 2);

	memset(buf, 'c', sizeof(buf)); /* 'c' ist das erste Zeichen der Liste */
	T_TRUE("genau die Hoechstlaenge", osk_static_pw_text_ok(buf, OSK_STATIC_PW_MAX));
	T_TRUE("ein Zeichen mehr nicht", !osk_static_pw_text_ok(buf, OSK_STATIC_PW_MAX + 1u));
	T_TRUE("leer nicht", !osk_static_pw_text_ok(buf, 0u));
	T_TRUE("Nullzeiger nicht", !osk_static_pw_text_ok(NULL, 4u));

	/* Ein einziges schlechtes Zeichen mitten in einem guten Text
	 * verdirbt den ganzen - nicht nur sein eigenes. */
	T_TRUE("ein schlechtes Zeichen reicht",
	       !osk_static_pw_text_ok((const uint8_t *)"ccacc", 5u));
}

static void test_round_trip(void)
{
	osk_static_pw_t in;
	osk_static_pw_t out;
	unsigned asked_before;

	mock_reset_all();
	osk_static_pw_reset();

	T_TRUE("am Anfang leer", !osk_static_pw_present(0u));
	T_TRUE("Steckplatz 2 auch", !osk_static_pw_present(1u));

	fill(&in, "Hunter2!", true);
	T_EQ("ablegen", osk_static_pw_set(0u, &in), OSK_OK);
	T_TRUE("jetzt belegt", osk_static_pw_present(0u));
	T_TRUE("der andere bleibt leer", !osk_static_pw_present(1u));

	/* present() oeffnet nichts und fragt deshalb nicht am Display. Sonst
	 * fragte jeder Blick in die Liste nach einem Tastendruck. */
	asked_before = mock_ui_confirm_count();
	T_TRUE("present fragt nicht", osk_static_pw_present(0u));
	T_EQ("keine Frage", mock_ui_confirm_count(), asked_before);

	T_EQ("holen", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_EQ("Laenge", out.len, in.len);
	T_TRUE("Text", memcmp(out.text, in.text, out.len) == 0);
	T_TRUE("Text endet auf eine Null", out.text[out.len] == 0u);
	T_TRUE("Eingabetaste gemerkt", out.append_enter);

	/* Das Holen OEFFNET einen Tresoreintrag - hier verlaesst ein
	 * Geheimnis das Geraet, als getippter Text, den jedes Programm
	 * mitlesen kann. Also wird gefragt. */
	T_TRUE("Herausgabe wurde gezeigt", mock_ui_confirm_count() > asked_before);
	T_TRUE("und als Herausgabe gezaehlt", mock_store_reveal_count() > 0u);

	/* Ohne Eingabetaste, und ersetzen statt anlegen. */
	fill(&in, "Hunter3", false);
	T_EQ("ersetzen", osk_static_pw_set(0u, &in), OSK_OK);
	T_EQ("holen", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_EQ("neue Laenge", out.len, 7u);
	T_TRUE("neuer Text", memcmp(out.text, "Hunter3", 7u) == 0);
	T_TRUE("keine Eingabetaste", !out.append_enter);

	/* Beide Steckplaetze sind unabhaengig. */
	fill(&in, "Hunter4", false);
	T_EQ("Steckplatz 2 ablegen", osk_static_pw_set(1u, &in), OSK_OK);
	T_EQ("Steckplatz 2 holen", osk_static_pw_fetch(1u, "Tastatur", &out), OSK_OK);
	T_TRUE("eigener Inhalt", memcmp(out.text, "Hunter4", 7u) == 0);
	T_EQ("Steckplatz 1 unberuehrt", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_TRUE("immer noch der dritte Text", memcmp(out.text, "Hunter3", 7u) == 0);

	/* Tabulator und Eingabetaste kann die Tastaturseite tippen, also
	 * darf ein Passwort sie enthalten - das ist der Weg
	 * "Benutzername, Tabulator, Passwort" in einer Zeichenkette. */
	fill(&in, "Hunter2\tHunter3", false);
	T_EQ("mit Tabulator", osk_static_pw_set(0u, &in), OSK_OK);
	T_EQ("holen", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_EQ("Laenge", out.len, 15u);
	T_EQ("Tabulator steht drin", out.text[7], (uint8_t)'\t');
}

static void test_rejects(void)
{
	osk_static_pw_t in;
	osk_static_pw_t out;

	mock_reset_all();
	osk_static_pw_reset();

	fill(&in, "Hunter2", false);
	T_TRUE("Steckplatz 2 gibt es nicht", osk_static_pw_set(2u, &in) != OSK_OK);
	T_TRUE("Steckplatz 255 auch nicht", osk_static_pw_set(255u, &in) != OSK_OK);
	T_TRUE("holen aus dem dritten", osk_static_pw_fetch(2u, "Tastatur", &out) != OSK_OK);
	T_TRUE("present sagt nein", !osk_static_pw_present(2u));

	/* Schlechter Text. */
	fill(&in, "Hunter2 mit Leerzeichen", false);
	T_EQ("Leerzeichen abgelehnt", osk_static_pw_set(0u, &in), OSK_ERR_ARG);
	memset(&in, 0, sizeof(in));
	in.len = 3u;
	in.text[0] = 'c';
	in.text[1] = 0x0au;
	in.text[2] = 'b';
	T_EQ("Zeilenende im Text", osk_static_pw_set(0u, &in), OSK_ERR_ARG);
	in.len = 0u;
	T_EQ("leer", osk_static_pw_set(0u, &in), OSK_ERR_ARG);
	T_EQ("Nullzeiger", osk_static_pw_set(0u, NULL), OSK_ERR_ARG);

	T_TRUE("nichts davon ist gelandet", !osk_static_pw_present(0u));
	T_EQ("holen aus dem leeren", osk_static_pw_fetch(0u, "Tastatur", &out),
	     OSK_ERR_NOT_FOUND);

	/* Sagt der Nutzer am Display nein, kommt nichts heraus. */
	fill(&in, "Hunter2", false);
	T_EQ("ablegen", osk_static_pw_set(0u, &in), OSK_OK);
	mock_ui_set_confirm(OSK_ERR_DENIED);
	memset(&out, 0x55u, sizeof(out));
	T_EQ("Nein am Display", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_ERR_DENIED);
	T_EQ("und nichts herausgegeben", out.len, 0u);
	mock_ui_set_confirm(OSK_OK);
	T_EQ("mit Ja wieder", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
}

static void test_clear(void)
{
	osk_static_pw_t in;
	osk_static_pw_t out;

	mock_reset_all();
	osk_static_pw_reset();

	T_EQ("leeren Steckplatz raeumen", osk_static_pw_clear(0u), OSK_ERR_NOT_FOUND);
	fill(&in, "Hunter2", false);
	T_EQ("ablegen", osk_static_pw_set(0u, &in), OSK_OK);
	T_EQ("Steckplatz 2 ablegen", osk_static_pw_set(1u, &in), OSK_OK);
	T_EQ("raeumen", osk_static_pw_clear(0u), OSK_OK);
	T_TRUE("ist weg", !osk_static_pw_present(0u));
	T_EQ("holen scheitert", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_ERR_NOT_FOUND);
	T_TRUE("der andere ist noch da", osk_static_pw_present(1u));
	T_TRUE("Steckplatz ausserhalb", osk_static_pw_clear(9u) != OSK_OK);
}

/* Der Satz kommt aus dem Flash, und was dort steht, hat diese Fassung des
 * Programms nicht unbedingt hingeschrieben. Also wird beim Lesen genauso
 * geprueft wie beim Schreiben. */
static void test_broken_record(void)
{
	osk_static_pw_t out;
	uint8_t rec[8];

	mock_reset_all();
	osk_static_pw_reset();

	/* Falsche Fassung. */
	rec[0] = 99u;
	rec[1] = 0u;
	rec[2] = 0u;
	rec[3] = 2u;
	rec[4] = 'c';
	rec[5] = 'b';
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("fremde Fassung abgelehnt", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Das reservierte Byte ist belegt: der Satz kommt von einer Fassung,
	 * die etwas damit meinte, und diese hier weiss nicht was. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 7u;
	rec[2] = 0u;
	rec[3] = 2u;
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("belegtes reserviertes Byte abgelehnt", osk_static_pw_fetch(0u, "T", &out),
	     OSK_ERR_FORMAT);

	/* Laenge im Satz und Laenge des Satzes gehen auseinander - zwei
	 * Angaben desselben Namens, und es wird nicht die eine der anderen
	 * angepasst. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 0u;
	rec[2] = 0u;
	rec[3] = 9u; /* behauptet neun Zeichen */
	rec[4] = 'c';
	rec[5] = 'b';
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("Laengen uneinig", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Zu kurz fuer den Kopf. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 0u;
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 2u), OSK_OK);
	T_EQ("Satz zu kurz", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Ein Zeichen im Satz, das die Tastatur nicht tippen kann. Das ist
	 * kein Hirngespinst: eine Fassung mit einer groesseren Abbildung
	 * koennte es geschrieben haben. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 0u;
	rec[2] = 0u;
	rec[3] = 2u;
	rec[4] = 'c';
	rec[5] = 'a'; /* 'a' steht nicht in der Liste */
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("nicht tippbares Zeichen aus dem Flash", osk_static_pw_fetch(0u, "T", &out),
	     OSK_ERR_FORMAT);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();

	test_matches_keyboard();
	test_length_rules();
	test_round_trip();
	test_rejects();
	test_clear();
	test_broken_record();
	return t_report("test_static_pw");
}
