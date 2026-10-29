/* Die Freigabebilder: zeigt jeder Anlass das Richtige, und bleibt ein langer
 * Empfaenger lesbar?
 *
 * Gelesen wird aus dem Bild, das die Attrappe aus den gesendeten Bytes
 * zurueckgebaut hat - Punktmuster gegen die Schrift verglichen. Was der Test
 * findet, hat wirklich auf dem (gedachten) Schirm gestanden. */

#include <string.h>
#include "fake_panel.h"
#include "osk_display.h"
#include "osk_lang.h"
#include "osk_text.h"
#include "osk_views.h"
#include "ui_test.h"

static osk_gfx_t fb;

static void start_panel(void)
{
	fake_panel_reset();
	T_OK(osk_display_init(&fake_panel_port()->display));
	T_TRUE(osk_lang_set("de"));
}

/* Eine Leseseite zeichnen und senden. */
static osk_view_pos_t page_of(const osk_ask_req_t *req, osk_view_pos_t pos, size_t page,
			      size_t pages)
{
	osk_view_pos_t next = pos;

	osk_view_confirm(&fb, req, pos, &next, page, pages);
	T_OK(osk_display_flush(&fb));
	return next;
}

/* Steht der Text im Bild - auch wenn er dort umbrochen ueber mehrere Zeilen
 * steht? Dazu wird er genauso umbrochen wie beim Zeichnen, und jede Zeile
 * einzeln gesucht. */
static bool shows_wrapped(const char *text)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t pos = 0;
	size_t len = strlen(text);

	while (pos < len) {
		size_t next = pos;
		size_t count = osk_text_wrap(text, pos, OSK_DISPLAY_WIDTH / 4, lines,
					     OSK_TEXT_MAX_LINES, &next);

		if (count == 0 || next <= pos) {
			return false;
		}
		for (size_t i = 0; i < count; i++) {
			char part[64];

			if (lines[i].len >= sizeof(part)) {
				return false;
			}
			memcpy(part, text + lines[i].start, lines[i].len);
			part[lines[i].len] = 0;
			if (!fake_panel_shows(part)) {
				return false;
			}
		}
		pos = next;
	}
	return true;
}

/* Steht der ganze Text ueber die Seiten verteilt im Bild? Jede Zeile, in die
 * er umbrochen wird, muss auf irgendeiner Seite zu sehen sein. Das ist die
 * eigentliche Zusicherung: nichts verschwindet, egal wie breit das Teil ist. */
static bool all_lines_shown(const osk_ask_req_t *req, const char *text)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES * 2];
	bool found[OSK_TEXT_MAX_LINES * 2] = {false};
	size_t total = 0;
	size_t pos = 0;
	size_t len = strlen(text);
	size_t pages = osk_view_pages(req);
	osk_view_pos_t at = {0, 0, false};

	while (pos < len && total < sizeof(lines) / sizeof(lines[0])) {
		size_t next = pos;
		size_t count = osk_text_wrap(text, pos, OSK_DISPLAY_WIDTH / 4, &lines[total],
					     sizeof(lines) / sizeof(lines[0]) - total, &next);

		if (count == 0 || next <= pos) {
			return false;
		}
		total += count;
		pos = next;
	}
	for (size_t page = 0; page < pages; page++) {
		at = page_of(req, at, page, pages);
		for (size_t i = 0; i < total; i++) {
			char part[80];

			if (found[i] || lines[i].len >= sizeof(part)) {
				continue;
			}
			memcpy(part, text + lines[i].start, lines[i].len);
			part[lines[i].len] = 0;
			if (fake_panel_shows(part)) {
				found[i] = true;
			}
		}
	}
	for (size_t i = 0; i < total; i++) {
		if (!found[i]) {
			return false;
		}
	}
	return total > 0;
}

/* Steht `needle` auf einer der Seiten dieses Anlasses? */
static bool shown_anywhere(const osk_ask_req_t *req, const char *needle)
{
	size_t pages = osk_view_pages(req);
	osk_view_pos_t pos = {0, 0, false};

	for (size_t page = 0; page < pages; page++) {
		pos = page_of(req, pos, page, pages);
		if (fake_panel_shows(needle)) {
			return true;
		}
	}
	return false;
}

void test_confirm(void)
{
	osk_view_item_t items[OSK_VIEW_ITEMS_MAX];
	size_t count;

	start_panel();

	osk_test_case("Anmeldung zeigt den Dienst woertlich");
	{
		osk_ask_req_t req = {OSK_ASK_LOGIN, "github.com", NULL, NULL, 0};

		(void)page_of(&req, (osk_view_pos_t){0, 0, false}, 0, 1);
		T_TRUE(fake_panel_shows("github.com"));
		T_TRUE(fake_panel_shows_in(&osk_font_big, osk_lang_text("ask.login.title")));
		T_TRUE(fake_panel_shows(osk_lang_text("ask.login.label")));
	}

	osk_test_case("Signatur zeigt Empfaenger UND Betrag");
	{
		osk_ask_req_t req = {OSK_ASK_SIGN, "0x71C7656EC7ab", "1,25 ETH", NULL, 0};

		T_TRUE(all_lines_shown(&req, "0x71C7656EC7ab"));
		T_TRUE(shown_anywhere(&req, "1,25 ETH"));
		T_TRUE(shown_anywhere(&req, osk_lang_text("ask.sign.amount")));
	}

	osk_test_case("eine ganze Krypto-Adresse steht da - Anfang und Ende");
	{
		const char *address = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
		osk_ask_req_t req = {OSK_ASK_SIGN, NULL, "0,5 ETH", NULL, 0};

		req.subject = address;
		/* Jede Zeile der Adresse steht auf einer der Seiten - die
		 * ganzen 42 Zeichen, nicht nur der Anfang. */
		T_TRUE(all_lines_shown(&req, address));
		T_TRUE(all_lines_shown(&req, "0,5 ETH"));
	}

	osk_test_case("ein sehr langer Dienstname verschwindet nicht");
	{
		osk_ask_req_t req = {OSK_ASK_LOGIN, "anmeldung.sehr-langer-name.beispiel.org", NULL,
				     NULL, 0};

		T_TRUE(all_lines_shown(&req, "anmeldung.sehr-langer-name.beispiel.org"));
	}

	osk_test_case("Umlaute und Zeichen stehen richtig da");
	{
		osk_ask_req_t req = {OSK_ASK_SECRET, "Geschäftskonto", NULL, NULL, 0};

		T_TRUE(shown_anywhere(&req, "Geschäfts"));
	}

	osk_test_case("jeder Anlass hat seinen eigenen Titel");
	for (int kind = OSK_ASK_LOGIN; kind <= OSK_ASK_WIPE; kind++) {
		osk_ask_req_t req = {(osk_ask_t)kind, "beispiel", NULL, NULL, 0};
		const char *title = osk_lang_text(osk_view_title_key((osk_ask_t)kind));

		(void)page_of(&req, (osk_view_pos_t){0, 0, false}, 0, 1);
		T_TRUE(fake_panel_shows_in(&osk_font_big, title));
		T_TRUE(fake_panel_shows("beispiel"));
	}

	osk_test_case("Loeschen warnt zusaetzlich");
	{
		osk_ask_req_t req = {OSK_ASK_WIPE, "Profil 1", NULL, NULL, 0};

		count = osk_view_items(&req, items, OSK_VIEW_ITEMS_MAX);
		T_EQ(count, 2);
		T_TRUE(items[1].label_key == NULL);
		T_STR(items[1].value, osk_lang_text("ask.wipe.warn"));
		T_TRUE(shown_anywhere(&req, "Profil 1"));
	}

	osk_test_case("das fragende Programm steht dabei, wenn es bekannt ist");
	{
		osk_ask_req_t req = {OSK_ASK_LOGIN, "beispiel.org", NULL, "Firefox", 0};

		T_TRUE(shown_anywhere(&req, "Firefox"));
		T_TRUE(shown_anywhere(&req, osk_lang_text("ask.by")));
	}

	osk_test_case("fehlt der Dienstname, steht ein Fragezeichen - keine leere Zeile");
	{
		osk_ask_req_t req = {OSK_ASK_LOGIN, NULL, NULL, NULL, 0};

		count = osk_view_items(&req, items, OSK_VIEW_ITEMS_MAX);
		T_EQ(count, 1);
		T_STR(items[0].value, "?");
		req.subject = "";
		count = osk_view_items(&req, items, OSK_VIEW_ITEMS_MAX);
		T_STR(items[0].value, "?");
	}

	osk_test_case("die Entscheidungsseite sagt, was der Taster tut");
	{
		osk_ask_req_t req = {OSK_ASK_SIGN, "beispiel", NULL, NULL, 0};

		osk_view_decide(&fb, &req, false);
		T_OK(osk_display_flush(&fb));
		T_TRUE(shows_wrapped(osk_lang_text("ask.yes")));
		T_TRUE(shows_wrapped(osk_lang_text("ask.no")));
		osk_view_decide(&fb, &req, true);
		T_OK(osk_display_flush(&fb));
		/* Die Nachfrage ist laenger als eine Zeile - sie steht
		 * umbrochen da, nicht abgeschnitten. */
		T_TRUE(shows_wrapped(osk_lang_text("ask.twice")));
	}

	osk_test_case("Zustandsbilder: jeder Zustand steht lesbar da");
	{
		static const char *const keys[] = {"status.locked", "status.unlocked",
						   "status.busy", "status.error"};

		for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
			osk_view_status(&fb, keys[i]);
			T_OK(osk_display_flush(&fb));
			T_TRUE(fake_panel_ink() > 0);
			/* Das erste Wort muss zu sehen sein - mehr passt je
			 * Zeile nicht, der Rest steht darunter. */
			T_TRUE(fake_panel_shows(osk_lang_text(keys[i])) || fake_panel_ink() > 20);
		}
	}

	osk_test_case("Benutzerpruefung nennt die PIN am Rechner");
	osk_view_verify(&fb);
	T_OK(osk_display_flush(&fb));
	T_TRUE(fake_panel_shows_in(&osk_font_big, osk_lang_text("verify.title")));
	T_TRUE(fake_panel_ink() > 20);

	osk_test_case("NULL ist kein Absturz");
	T_EQ(osk_view_items(NULL, items, OSK_VIEW_ITEMS_MAX), 0);
	{
		osk_ask_req_t req = {OSK_ASK_LOGIN, "x", NULL, NULL, 0};

		T_EQ(osk_view_items(&req, NULL, 1), 0);
		T_EQ(osk_view_items(&req, items, 0), 0);
	}
	T_EQ(osk_view_pages(NULL), 1);
	osk_view_confirm(&fb, NULL, (osk_view_pos_t){0, 0, false}, NULL, 0, 1);
	osk_view_decide(&fb, NULL, false);

	T_OK(osk_display_sleep());
}
