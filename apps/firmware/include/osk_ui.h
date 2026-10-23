/* Freigabe und Anzeige. Jedes Applet fragt hierueber, bevor es ein Geheimnis
 * herausgibt - und sagt dabei, WAS der Nutzer freigibt, damit es auf dem
 * Display steht und nicht nur der Rechner es weiss.
 *
 * Umsetzung: src/classes/ui/, Bilder in src/views/ */
#ifndef OSK_UI_H
#define OSK_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "osk_result.h"

typedef enum {
	OSK_ASK_LOGIN, /* Anmeldung bei einem Dienst */
	OSK_ASK_SIGN,  /* etwas unterschreiben */
	OSK_ASK_DECRYPT,
	OSK_ASK_SECRET, /* Passwort oder Schluessel herausgeben */
	OSK_ASK_PAIR,	/* Geraet koppeln oder Tresor uebertragen */
	OSK_ASK_WIPE,
} osk_ask_t;

/* Was dem Nutzer gezeigt wird. Alle Texte kommen als Schluessel aus der
 * Sprachdatei; nur was wirklich vom Gegenueber stammt (Dienst, Betrag),
 * wird woertlich angezeigt. */
typedef struct {
	osk_ask_t kind;
	const char *subject;  /* Dienst, Empfaenger - woertlich */
	const char *detail;   /* Betrag, Datei, Konto - woertlich, darf NULL sein */
	const char *asked_by; /* welches Programm fragt, falls bekannt */
	uint32_t timeout_ms;
} osk_ask_req_t;

/* Blockiert bis Tastendruck, Zeitablauf oder Abbruch.
 * OSK_ERR_DENIED heisst: der Nutzer wollte nicht. */
osk_result_t osk_ui_confirm(const osk_ask_req_t *req);

/* Benutzerpruefung: erst Finger, bei Fehlschlag PIN. */
osk_result_t osk_ui_verify_user(bool *verified_out);

void osk_ui_status(const char *text_key);

#endif
