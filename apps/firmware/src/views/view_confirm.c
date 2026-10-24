/* Die Freigabebilder: je Anlass eines, und jedes zeigt woertlich, worum es
 * geht - siehe osk_views.h.
 *
 * Der lange Name, die lange Adresse, der lange Betreff: alles wird umbrochen
 * und notfalls ueber mehrere Seiten gezeigt. Nichts verschwindet. */

#include <stdio.h>
#include <string.h>
#include "osk_lang.h"
#include "osk_text.h"
#include "osk_views.h"
#include "view_layout.h"

/* Je Anlass: Titel und Beschriftung des woertlichen Teils. Nur Schluessel -
 * kein sichtbarer Text steht hier. */
struct kind_texts {
	osk_ask_t kind;
	const char *title_key;
	const char *label_key;
	const char *second_key; /* zweiter Wert (Betrag) oder NULL */
	const char *warn_key;	/* Warnung ohne Beschriftung oder NULL */
};

static const struct kind_texts KINDS[] = {
    {OSK_ASK_LOGIN, "ask.login.title", "ask.login.label", NULL, NULL},
    {OSK_ASK_SIGN, "ask.sign.title", "ask.sign.label", "ask.sign.amount", NULL},
    {OSK_ASK_DECRYPT, "ask.decrypt.title", "ask.decrypt.label", NULL, NULL},
    {OSK_ASK_SECRET, "ask.secret.title", "ask.secret.label", NULL, NULL},
    {OSK_ASK_PAIR, "ask.pair.title", "ask.pair.label", NULL, NULL},
    {OSK_ASK_WIPE, "ask.wipe.title", "ask.wipe.label", NULL, "ask.wipe.warn"},
};

static const struct kind_texts *texts_of(osk_ask_t kind)
{
	for (size_t i = 0; i < sizeof(KINDS) / sizeof(KINDS[0]); i++) {
		if (KINDS[i].kind == kind) {
			return &KINDS[i];
		}
	}
	return &KINDS[0];
}

const char *osk_view_title_key(osk_ask_t kind)
{
	return texts_of(kind)->title_key;
}

size_t osk_view_items(const osk_ask_req_t *req, osk_view_item_t *items, size_t max_items)
{
	const struct kind_texts *kind;
	size_t count = 0;

	if (req == NULL || items == NULL || max_items == 0) {
		return 0;
	}
	kind = texts_of(req->kind);
	/* Fehlt der Dienstname, steht ein Fragezeichen da - der Nutzer soll
	 * sehen, dass die Angabe fehlt, statt eine leere Zeile zu lesen. */
	items[count].label_key = kind->label_key;
	items[count].value = (req->subject != NULL && req->subject[0] != '\0') ? req->subject : "?";
	count++;
	if (req->detail != NULL && req->detail[0] != '\0' && count < max_items) {
		items[count].label_key =
		    (kind->second_key != NULL) ? kind->second_key : "ask.detail";
		items[count].value = req->detail;
		count++;
	}
	if (kind->warn_key != NULL && count < max_items) {
		items[count].label_key = NULL;
		items[count].value = osk_lang_text(kind->warn_key);
		count++;
	}
	if (req->asked_by != NULL && req->asked_by[0] != '\0' && count < max_items) {
		items[count].label_key = "ask.by";
		items[count].value = req->asked_by;
		count++;
	}
	return count;
}

/* Eine Seite setzen. fb darf NULL sein - dann wird nur gerechnet, und daraus
 * entsteht die Seitenzahl. */
static osk_view_pos_t layout_page(osk_gfx_t *fb, const osk_view_item_t *items, size_t count,
				  osk_view_pos_t pos, int16_t top, int16_t rows)
{
	int16_t y = top;
	int16_t used = 0;

	while (used < rows && pos.item < count) {
		const osk_view_item_t *item = &items[pos.item];
		size_t value_len = (item->value != NULL) ? strlen(item->value) : 0;
		osk_text_line_t line;
		size_t next = pos.offset;

		if (!pos.label_done && item->label_key != NULL) {
			const char *label = osk_lang_text(item->label_key);
			int16_t x = osk_text_draw(fb, &osk_font_small, label, strlen(label), 0, y);

			(void)osk_text_draw(fb, &osk_font_small, ":", 1, x, y);
			pos.label_done = true;
			y = (int16_t)(y + osk_font_small.line);
			used++;
			continue;
		}
		if (osk_text_wrap(item->value, pos.offset, VIEW_BODY_COLUMNS, &line, 1, &next) ==
		    0) {
			pos.item++;
			pos.offset = 0;
			pos.label_done = false;
			continue;
		}
		(void)osk_text_draw(fb, &osk_font_small, item->value + line.start, line.len, 0, y);
		y = (int16_t)(y + osk_font_small.line);
		used++;
		pos.offset = (uint16_t)next;
		if (next >= value_len) {
			pos.item++;
			pos.offset = 0;
			pos.label_done = false;
		}
	}
	return pos;
}

static int16_t body_top_of(osk_gfx_t *fb, const osk_ask_req_t *req)
{
	return view_title(fb, osk_lang_text(osk_view_title_key(req->kind)));
}

void osk_view_confirm(osk_gfx_t *fb, const osk_ask_req_t *req, osk_view_pos_t pos,
		      osk_view_pos_t *next, size_t page, size_t pages)
{
	osk_view_item_t items[OSK_VIEW_ITEMS_MAX];
	size_t count;
	int16_t top;
	int16_t rows;
	char marker[12];

	if (next != NULL) {
		*next = pos;
	}
	if (req == NULL) {
		return;
	}
	osk_gfx_clear(fb);
	count = osk_view_items(req, items, OSK_VIEW_ITEMS_MAX);
	top = body_top_of(fb, req);
	rows = view_body_rows(top, 1);
	pos = layout_page(fb, items, count, pos, top, rows);
	if (next != NULL) {
		*next = pos;
	}
	/* Fusszeile: was der Taster tut, und wo man sich befindet. */
	if (pages > 1) {
		(void)snprintf(marker, sizeof(marker), "%u/%u", (unsigned)(page + 1),
			       (unsigned)pages);
	} else {
		marker[0] = '\0';
	}
	view_footer(fb, 0, osk_lang_text("ask.next"), (marker[0] != '\0') ? marker : NULL);
}

size_t osk_view_pages(const osk_ask_req_t *req)
{
	osk_view_item_t items[OSK_VIEW_ITEMS_MAX];
	osk_view_pos_t pos = {0, 0, false};
	size_t count;
	size_t pages = 0;
	int16_t top;
	int16_t rows;

	if (req == NULL) {
		return 1;
	}
	count = osk_view_items(req, items, OSK_VIEW_ITEMS_MAX);
	top = body_top_of(NULL, req);
	rows = view_body_rows(top, 1);
	do {
		osk_view_pos_t before = pos;

		pos = layout_page(NULL, items, count, pos, top, rows);
		pages++;
		if (pos.item == before.item && pos.offset == before.offset &&
		    pos.label_done == before.label_done) {
			break; /* kein Fortschritt - nie endlos blaettern */
		}
	} while (pos.item < count);
	return (pages == 0) ? 1 : pages;
}

void osk_view_decide(osk_gfx_t *fb, const osk_ask_req_t *req, bool again)
{
	int16_t top;
	const char *question;

	if (req == NULL) {
		return;
	}
	osk_gfx_clear(fb);
	top = body_top_of(fb, req);
	/* Beim zweiten Mal steht die Nachfrage dort, wo sonst der Hinweis steht.
	 * Auch sie wird umbrochen: "Noch einmal lang" ist laenger als eine
	 * Zeile, und abgeschnitten werden hier genauso wenig Texte wie sonst. */
	question = again ? osk_lang_text("ask.twice") : osk_lang_text("ask.reread");
	{
		osk_text_line_t lines[OSK_TEXT_MAX_LINES];
		size_t next = 0;
		int16_t rows = view_body_rows(top, 2);
		size_t count = osk_text_wrap(question, 0, VIEW_BODY_COLUMNS, lines,
					     (rows > 0) ? (size_t)rows : 1, &next);
		int16_t y = (int16_t)(top + osk_font_small.line);

		for (size_t i = 0; i < count; i++) {
			(void)osk_text_draw(fb, &osk_font_small, question + lines[i].start,
					    lines[i].len, 0, y);
			y = (int16_t)(y + osk_font_small.line);
		}
	}
	view_footer(fb, 1, osk_lang_text("ask.yes"), NULL);
	view_footer(fb, 0, osk_lang_text("ask.no"), NULL);
}
