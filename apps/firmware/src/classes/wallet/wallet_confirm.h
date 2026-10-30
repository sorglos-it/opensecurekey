/* Anzeigen, dann unterschreiben - nie umgekehrt.
 *
 * Das ist der Grund, warum in diesem Geraet ueberhaupt ein Display verbaut
 * ist. Der Rechner, der die Ueberweisung schickt, kann belogen worden sein;
 * was auf dem Display steht, kommt aus den Bytes, die wirklich
 * unterschrieben werden.
 *
 * Die harte Regel, hier umgesetzt: **was das Geraet nicht lesbar anzeigen
 * kann, unterschreibt es nicht.** Fehlt der Empfaenger, der Betrag oder die
 * Gebuehr, kommt OSK_ERR_DENIED. Es gibt keinen Weg daran vorbei: keine
 * Blindunterschrift, keine Extrafreigabe, keinen Schalter. Ein Schalter,
 * der die Regel abschaltet, ist in einem Geraet, dessen ganzer Zweck
 * diese Regel ist, kein Zugestaendnis an den Nutzer, sondern die Stelle,
 * auf die ein Angreifer ihn hinredet.
 *
 * Die Adresse wird NIE gekuerzt. Anfang und Ende zu zeigen und die Mitte
 * durch "..." zu ersetzen ist der uebliche Weg - und der uebliche Angriff:
 * ein Angreifer sucht sich so lange eine Adresse, bis Anfang und Ende
 * passen, und die Mitte sieht niemand. Darum steht hier die ganze
 * Adresse, in Gruppen von vier Zeichen, und dazu eine Pruefzahl ueber
 * ALLE Zeichen. Passt die ganze Adresse nicht, wird nicht unterschrieben.
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

/* Die laengste Adresse, die vorkommen kann: Bech32 darf nach BIP173 90
 * Zeichen haben. Ethereum braucht 42, Base58 35. Wer laenger ist, ist
 * keine Adresse, die dieses Geraet anzeigen kann - und wird abgelehnt. */
#define OSK_WALLET_ADDR_MAX 91

/* Dieselbe Adresse in Vierergruppen, mit Leerzeichen dazwischen: 90
 * Zeichen werden 23 Gruppen, also 22 Leerzeichen dazu. */
#define OSK_WALLET_ADDR_SHOWN_MAX (OSK_WALLET_ADDR_MAX + 23)

/* So viele Stellen hat die Pruefzahl ueber die ganze Adresse. */
#define OSK_WALLET_FINGERPRINT_DIGITS 8

/* Was in `recipient` steht - und damit, wie es angezeigt wird.
 *
 * Das ist kein Schoenheitsfehler, sondern notwendig: eine Adresse wird
 * gegliedert und bekommt eine Pruefzahl, weil ein Mensch sie Zeichen fuer
 * Zeichen vergleichen muss. Ein Satz, den ein Mensch LIEST, wird dadurch
 * schlechter lesbar, nicht besser. Beides gleich zu behandeln waere eine
 * Anzeige, die "ungefaehr" stimmt. */
typedef enum {
	/* Eine Adresse: in Vierergruppen, mit Pruefzahl, nie gekuerzt. */
	OSK_WALLET_SUBJECT_ADDRESS,
	/* Ein Text, den ein Mensch liest (EIP-191): woertlich, wie er ist. */
	OSK_WALLET_SUBJECT_TEXT,
} osk_wallet_subject_t;

/* Was auf dem Schirm steht. Leere Felder heissen "nicht darstellbar". */
typedef struct {
	/* Die Adresse oder der Text, woertlich und UNGEKUERZT. */
	char recipient[OSK_WALLET_TEXT_MAX];
	osk_wallet_subject_t subject_kind;
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

/* Die Adresse uebernehmen - ganz oder gar nicht. Laenger als
 * OSK_WALLET_ADDR_MAX heisst OSK_ERR_FORMAT, und dann wird nicht
 * unterschrieben. Es gibt absichtlich keine Funktion, die kuerzt. */
osk_result_t osk_wallet_set_address(osk_wallet_view_t *view, const char *address);

/* Die Adresse so aufschreiben, wie ein Mensch sie vergleichen kann: in
 * Gruppen von vier Zeichen. Das Auge verliert in einer Reihe aus 42
 * gleich aussehenden Zeichen die Stelle; in Vierergruppen nicht. Der
 * Inhalt wird dabei nicht angetastet, nur gegliedert. */
osk_result_t osk_wallet_group_address(const char *address, char *out, size_t cap);

/* Pruefzahl ueber die GANZE Adresse, als Text aus
 * OSK_WALLET_FINGERPRINT_DIGITS Ziffern.
 *
 * Wozu: sie ist der zweite, unabhaengige Vergleich. Wer die Adresse nur
 * ueberfliegt, faellt auf eine vertauschte Mitte herein; die Pruefzahl
 * aendert sich bei jedem einzelnen vertauschten Zeichen.
 *
 * Was sie NICHT ist: der Schutz. Der Schutz ist, dass die ganze Adresse
 * dasteht. Acht Ziffern sind rund 27 Bit, und 27 Bit kann ein Angreifer
 * durchprobieren. Wer nur die Pruefzahl liest und die Adresse nicht, ist
 * nicht geschuetzt - das steht so in docs/wallet-bau.md. */
osk_result_t osk_wallet_fingerprint(const char *address, char *out, size_t cap);

/* Fragt den Nutzer.
 *
 * Es gibt kein `allow_blind`. Ist die Ansicht nicht lesbar - fehlt der
 * Empfaenger, der Betrag oder die Gebuehr -, kommt OSK_ERR_DENIED, und
 * zwar immer.
 *
 * OSK_ERR_DENIED: der Nutzer wollte nicht, oder es war nichts anzuzeigen. */
osk_result_t osk_wallet_confirm(const osk_wallet_view_t *view, const char *asked_by);

#endif
