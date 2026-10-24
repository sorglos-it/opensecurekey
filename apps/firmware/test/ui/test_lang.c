/* Sprachen: fuenf Stueck, gleiche Schluessel, Rueckfall auf Englisch.
 *
 * Dass die fuenf Dateien in apps/firmware/lang/ dieselben Schluessel haben,
 * wird an zwei Stellen geprueft: generate.py bricht ab, wenn eine Datei
 * abweicht (das Makefile ruft es mit --check auf), und tools/check.py prueft
 * es noch einmal von aussen. Hier wird geprueft, was die Firmware daraus
 * macht - und dass jeder Schluessel, den ein Bild benutzt, wirklich da ist. */

#include <string.h>
#include "osk_lang.h"
#include "osk_views.h"
#include "ui_test.h"

/* Jeder Schluessel, den die Bilder nachschlagen. Fehlt einer in der Tabelle,
 * stuende "?" auf dem Schirm - das soll hier auffallen, nicht dort. */
static const char *const USED[] = {
    "ask.login.title",
    "ask.login.label",
    "ask.sign.title",
    "ask.sign.label",
    "ask.sign.amount",
    "ask.decrypt.title",
    "ask.decrypt.label",
    "ask.secret.title",
    "ask.secret.label",
    "ask.pair.title",
    "ask.pair.label",
    "ask.wipe.title",
    "ask.wipe.label",
    "ask.wipe.warn",
    "ask.by",
    "ask.detail",
    "ask.next",
    "ask.yes",
    "ask.no",
    "ask.reread",
    "ask.twice",
    "status.locked",
    "status.unlocked",
    "status.busy",
    "status.battery",
    "status.error",
    "verify.title",
    "verify.hint",
    "verify.ok",
    "error.timeout",
    "error.denied",
};

void test_lang(void)
{
	osk_test_case("fuenf Sprachen sind dabei");
	T_EQ(osk_lang_count(), 5);
	T_TRUE(osk_lang_set("de"));
	T_TRUE(osk_lang_set("en"));
	T_TRUE(osk_lang_set("fr"));
	T_TRUE(osk_lang_set("it"));
	T_TRUE(osk_lang_set("es"));

	osk_test_case("unbekannte Sprache faellt auf Englisch zurueck");
	T_TRUE(!osk_lang_set("xx"));
	T_STR(osk_lang_current(), "en");
	T_TRUE(!osk_lang_set(NULL));
	T_STR(osk_lang_current(), "en");

	osk_test_case("jeder Schluessel hat in jeder Sprache einen Text");
	for (size_t lang = 0; lang < osk_lang_count(); lang++) {
		T_TRUE(osk_lang_set(osk_lang_name(lang)));
		for (size_t i = 0; i < osk_lang_key_count(); i++) {
			const char *text = osk_lang_text(osk_lang_key(i));

			T_TRUE(text != NULL && text[0] != '\0');
			T_TRUE(strcmp(text, "?") != 0);
			/* Nie der nackte Schluessel - der sagt dem Nutzer nichts. */
			T_TRUE(strcmp(text, osk_lang_key(i)) != 0);
		}
	}

	osk_test_case("die Bilder benutzen nur Schluessel, die es gibt");
	T_TRUE(osk_lang_set("de"));
	for (size_t i = 0; i < sizeof(USED) / sizeof(USED[0]); i++) {
		const char *text = osk_lang_text(USED[i]);

		T_TRUE(strcmp(text, "?") != 0);
	}

	osk_test_case("die Titel aller Anlaesse sind da");
	for (int kind = OSK_ASK_LOGIN; kind <= OSK_ASK_WIPE; kind++) {
		const char *key = osk_view_title_key((osk_ask_t)kind);

		T_TRUE(key != NULL);
		T_TRUE(strcmp(osk_lang_text(key), "?") != 0);
	}

	osk_test_case("unbekannter Schluessel gibt ein Zeichen, nie NULL");
	T_TRUE(osk_lang_text("gibt.es.nicht") != NULL);
	T_STR(osk_lang_text("gibt.es.nicht"), "?");
	T_TRUE(osk_lang_text(NULL) != NULL);

	osk_test_case("die Sprachen sagen Verschiedenes");
	T_TRUE(osk_lang_set("de"));
	const char *de = osk_lang_text("status.locked");
	char keep[64];

	(void)snprintf(keep, sizeof(keep), "%s", de);
	T_TRUE(osk_lang_set("es"));
	T_TRUE(strcmp(keep, osk_lang_text("status.locked")) != 0);

	osk_test_case("Namen und Schluessel ausserhalb der Liste geben Leerstring");
	T_STR(osk_lang_name(99), "");
	T_STR(osk_lang_key(99999), "");
	T_TRUE(osk_lang_key_count() > 20);
	T_TRUE(osk_lang_set("de"));
}
