/* Statisches Passwort, Kartenseite: was angenommen wird, was abgelehnt
 * wird, und dass jede Herausgabe am Display sichtbar ist.
 *
 * Das Tippen selbst gehoert Strang E (transport/) und wird hier NICHT
 * geprueft - es gibt auch nichts, wogegen man es pruefen koennte, solange
 * kein USB-Tastatur-Transport da ist. Geprueft wird genau die Naht:
 * osk_static_pw_fetch() liefert, was getippt werden soll.
 *
 * Am PC gegen Attrappen - Stufe 2 von vier. */
#include "test_util.h"
#include "osk_static_pw.h"
#include "osk_store.h"
#include "osk_result.h"
#include "mocks.h"

static void fill(osk_static_pw_t *pw, const char *text, uint8_t layout, bool enter)
{
	memset(pw, 0, sizeof(*pw));
	pw->layout = layout;
	pw->append_enter = enter;
	pw->len = (uint8_t)strlen(text);
	memcpy(pw->text, text, pw->len);
}

static void test_text_rules(void)
{
	uint8_t buf[OSK_STATIC_PW_MAX + 4u];

	T_EQ("Hoechstlaenge", (int)OSK_STATIC_PW_MAX, 64);
	T_EQ("zwei Steckplaetze", (int)OSK_STATIC_PW_SLOTS, 2);

	T_TRUE("gewoehnliches Passwort",
	       osk_static_pw_text_ok((const uint8_t *)"Hunter2!", 8u));
	/* Leerzeichen ist 0x20 und erlaubt - eine Wortfolge ist ein
	 * gueltiges Passwort. */
	T_TRUE("mit Leerzeichen",
	       osk_static_pw_text_ok((const uint8_t *)"correct horse battery", 21u));
	T_TRUE("Tilde ist 0x7E und noch drin", osk_static_pw_text_ok((const uint8_t *)"~", 1u));
	T_TRUE("Leerzeichen allein", osk_static_pw_text_ok((const uint8_t *)" ", 1u));

	T_TRUE("leer nicht", !osk_static_pw_text_ok((const uint8_t *)"x", 0u));
	T_TRUE("Nullzeiger nicht", !osk_static_pw_text_ok(NULL, 4u));

	memset(buf, 'a', sizeof(buf));
	T_TRUE("genau die Hoechstlaenge", osk_static_pw_text_ok(buf, OSK_STATIC_PW_MAX));
	T_TRUE("ein Zeichen mehr nicht", !osk_static_pw_text_ok(buf, OSK_STATIC_PW_MAX + 1u));

	/* Alles ausserhalb von 0x20 bis 0x7E wird abgelehnt - und zwar beim
	 * SETZEN, nicht beim Tippen. Ein Zeichen, dessen Tastencode von der
	 * Belegung des Rechners abhaengt, kaeme manchmal anders an, und ein
	 * Passwort, das manchmal anders ankommt, ist schlimmer als keines. */
	{
		uint8_t one[2];

		one[0] = 0x1fu;
		T_TRUE("Steuerzeichen 0x1F nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0x09u;
		T_TRUE("Tabulator nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0x0au;
		T_TRUE("Zeilenende nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0x00u;
		T_TRUE("Null nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0x7fu;
		T_TRUE("0x7F nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0xe4u; /* ein Umlaut in Latin-1 */
		T_TRUE("Umlaut nicht", !osk_static_pw_text_ok(one, 1u));
		one[0] = 0xffu;
		T_TRUE("0xFF nicht", !osk_static_pw_text_ok(one, 1u));
	}

	/* Ein einziges schlechtes Zeichen mitten in einem guten Text
	 * verdirbt den ganzen - nicht nur sein eigenes. */
	{
		uint8_t mixed[6];

		memcpy(mixed, "ab", 2u);
		mixed[2] = 0x0au;
		memcpy(mixed + 3, "cd", 2u);
		T_TRUE("ein schlechtes Zeichen reicht", !osk_static_pw_text_ok(mixed, 5u));
	}
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

	fill(&in, "Hunter2!", OSK_KBD_LAYOUT_US, true);
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
	T_EQ("Belegung", out.layout, OSK_KBD_LAYOUT_US);
	T_TRUE("Eingabetaste gemerkt", out.append_enter);

	/* Das Holen OEFFNET einen Tresoreintrag - hier verlaesst ein
	 * Geheimnis das Geraet, als getippter Text, den jedes Programm
	 * mitlesen kann. Also wird gefragt. */
	T_TRUE("Herausgabe wurde gezeigt", mock_ui_confirm_count() > asked_before);
	T_TRUE("und als Herausgabe gezaehlt", mock_store_reveal_count() > 0u);

	/* Ohne Eingabetaste, und ersetzen statt anlegen. */
	fill(&in, "zweites", OSK_KBD_LAYOUT_US, false);
	T_EQ("ersetzen", osk_static_pw_set(0u, &in), OSK_OK);
	T_EQ("holen", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_EQ("neue Laenge", out.len, 7u);
	T_TRUE("neuer Text", memcmp(out.text, "zweites", 7u) == 0);
	T_TRUE("keine Eingabetaste", !out.append_enter);

	/* Beide Steckplaetze sind unabhaengig. */
	fill(&in, "slotzwei", OSK_KBD_LAYOUT_US, false);
	T_EQ("Steckplatz 2 ablegen", osk_static_pw_set(1u, &in), OSK_OK);
	T_EQ("Steckplatz 2 holen", osk_static_pw_fetch(1u, "Tastatur", &out), OSK_OK);
	T_TRUE("eigener Inhalt", memcmp(out.text, "slotzwei", 8u) == 0);
	T_EQ("Steckplatz 1 unberuehrt", osk_static_pw_fetch(0u, "Tastatur", &out), OSK_OK);
	T_TRUE("immer noch der zweite Text", memcmp(out.text, "zweites", 7u) == 0);
}

static void test_rejects(void)
{
	osk_static_pw_t in;
	osk_static_pw_t out;

	mock_reset_all();
	osk_static_pw_reset();

	fill(&in, "egal", OSK_KBD_LAYOUT_US, false);
	T_TRUE("Steckplatz 2 gibt es nicht", osk_static_pw_set(2u, &in) != OSK_OK);
	T_TRUE("Steckplatz 255 auch nicht", osk_static_pw_set(255u, &in) != OSK_OK);
	T_TRUE("holen aus dem dritten", osk_static_pw_fetch(2u, "Tastatur", &out) != OSK_OK);
	T_TRUE("present sagt nein", !osk_static_pw_present(2u));

	/* Eine Belegung, die Strang E noch nicht vergeben hat. Lieber
	 * ablehnen, als eine Zahl annehmen, die auf beiden Seiten etwas
	 * anderes heisst. */
	fill(&in, "egal", 1u, false);
	T_EQ("fremde Belegung", osk_static_pw_set(0u, &in), OSK_ERR_UNSUPPORTED);
	fill(&in, "egal", 200u, false);
	T_EQ("noch fremdere", osk_static_pw_set(0u, &in), OSK_ERR_UNSUPPORTED);

	/* Schlechter Text. */
	memset(&in, 0, sizeof(in));
	in.layout = OSK_KBD_LAYOUT_US;
	in.len = 3u;
	in.text[0] = 'a';
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
	fill(&in, "geheim", OSK_KBD_LAYOUT_US, false);
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
	fill(&in, "weg damit", OSK_KBD_LAYOUT_US, false);
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
	rec[1] = OSK_KBD_LAYOUT_US;
	rec[2] = 0u;
	rec[3] = 2u;
	rec[4] = 'a';
	rec[5] = 'b';
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("fremde Fassung abgelehnt", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Laenge im Satz und Laenge des Satzes gehen auseinander - zwei
	 * Angaben desselben Namens, und es wird nicht die eine der anderen
	 * angepasst. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = OSK_KBD_LAYOUT_US;
	rec[2] = 0u;
	rec[3] = 9u; /* behauptet neun Zeichen */
	rec[4] = 'a';
	rec[5] = 'b';
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("Laengen uneinig", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Zu kurz fuer den Kopf. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 0u;
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 2u), OSK_OK);
	T_EQ("Satz zu kurz", osk_static_pw_fetch(0u, "T", &out), OSK_ERR_FORMAT);

	/* Ein Zeichen im Satz, das es nicht geben darf. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = OSK_KBD_LAYOUT_US;
	rec[2] = 0u;
	rec[3] = 2u;
	rec[4] = 'a';
	rec[5] = 0x0au;
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("schlechtes Zeichen aus dem Flash", osk_static_pw_fetch(0u, "T", &out),
	     OSK_ERR_FORMAT);

	/* Eine Belegung aus dem Flash, die es nicht gibt. */
	mock_reset_all();
	rec[0] = 1u;
	rec[1] = 7u;
	rec[2] = 0u;
	rec[3] = 2u;
	rec[4] = 'a';
	rec[5] = 'b';
	T_EQ("Satz hinterlegen", mock_store_put(OSK_ITEM_LOGIN, "SPW-1", rec, 6u), OSK_OK);
	T_EQ("fremde Belegung aus dem Flash", osk_static_pw_fetch(0u, "T", &out),
	     OSK_ERR_FORMAT);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();

	test_text_rules();
	test_round_trip();
	test_rejects();
	test_clear();
	test_broken_record();
	return t_report("test_static_pw");
}
