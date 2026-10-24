/* Anzeigen, dann unterschreiben - nie umgekehrt.
 *
 * Das ist der Grund, warum in diesem Geraet ueberhaupt ein Display verbaut
 * ist. Der Rechner, der die Ueberweisung schickt, kann belogen worden sein;
 * was auf dem Display steht, kommt aus den Bytes, die wirklich
 * unterschrieben werden.
 *
 * Die harte Regel, hier umgesetzt: **was das Geraet nicht lesbar anzeigen
 * kann, unterschreibt es nicht.** Fehlt der Empfaenger, der Betrag oder die
 * Gebuehr, kommt OSK_ERR_DENIED - es sei denn, der Aufrufer verlangt
 * ausdruecklich die Blindunterschrift, und die bekommt ihre eigene,
 * deutlich andere Frage auf dem Schirm.
 *
 * Betraege werden nie als Fliesskommazahl gerechnet. Satoshi und Wei sind
 * ganze Zahlen, und sie bleiben es bis in den Text. */
#ifndef WALLET_CONFIRM_H
#define WALLET_CONFIRM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

#define OSK_WALLET_TEXT_MAX 96
#define OSK_WALLET_AMOUNT_MAX 48

/* Was auf dem Schirm steht. Leere Felder heissen "nicht darstellbar". */
typedef struct {
	char recipient[OSK_WALLET_TEXT_MAX]; /* Adresse oder Konto, woertlich */
	char amount[OSK_WALLET_AMOUNT_MAX];
	char fee[OSK_WALLET_AMOUNT_MAX];
	char extra[OSK_WALLET_TEXT_MAX]; /* Kette, Vertragsaufruf, darf leer sein */
	/* Mehrere Empfaenger: dann steht in recipient der erste und hier die
	 * Zahl der uebrigen. 0 heisst: es ist nur dieser eine. */
	uint32_t more_recipients;
	/* false heisst: irgendetwas konnte nicht gelesen oder nicht lesbar
	 * gemacht werden. Dann wird ohne ausdrueckliche Extrafreigabe nicht
	 * unterschrieben. */
	bool readable;
	const char *missing_key; /* Sprachschluessel, warum nicht lesbar */
} osk_wallet_view_t;

void osk_wallet_view_init(osk_wallet_view_t *view);

/* Ganzzahl mit Nachkommastellen in Text, ohne Fliesskomma.
 * decimals 8 fuer Bitcoin, 18 fuer Ether. Nullen am Ende fallen weg,
 * mindestens eine Nachkommastelle bleibt stehen. */
osk_result_t osk_wallet_format_amount(uint64_t value, unsigned decimals, const char *unit,
				      char *out, size_t cap);

/* Dasselbe fuer 256-Bit-Betraege - Ethereum rechnet in Wei, und das passt
 * nicht in 64 Bit. */
osk_result_t osk_wallet_format_amount256(const uint8_t value[32], unsigned decimals,
					 const char *unit, char *out, size_t cap);

/* Adresse kuerzen, wenn sie nicht auf den Schirm passt: Anfang und Ende
 * bleiben stehen, die Mitte wird zu "...". Beide Enden sind das, was ein
 * Mensch wirklich vergleicht. Weniger als 12 Zeichen Platz heisst: nicht
 * darstellbar, und dann wird nicht unterschrieben. */
osk_result_t osk_wallet_shorten(const char *text, char *out, size_t cap);

/* Fragt den Nutzer. `allow_blind` nur, wenn er die Blindunterschrift
 * vorher ausdruecklich eingeschaltet hat.
 * OSK_ERR_DENIED: der Nutzer wollte nicht, oder es war nichts anzuzeigen. */
osk_result_t osk_wallet_confirm(const osk_wallet_view_t *view, const char *asked_by,
				bool allow_blind);

#endif
