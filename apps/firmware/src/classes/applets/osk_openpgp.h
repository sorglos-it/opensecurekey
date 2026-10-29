/* OpenPGP-Karte: Unterschreiben, Entschluesseln und Anmelden mit GnuPG.
 *
 * Umsetzung nach "Functional Specification of the OpenPGP application on
 * ISO Smart Card Operating Systems", Version 3.4 (8. Juli 2020). Drei
 * Schluessel, wie die Spezifikation sie vorsieht:
 *
 *   Signatur (CRT B6)        unterschreibt Nachrichten und Dateien
 *   Verschluesselung (B8)    Schluesselaustausch beim Entschluesseln
 *   Authentisierung (A4)     Anmeldung, etwa per SSH
 *
 * Die Schluessel entstehen im SE050. Die Benutzer-PIN (PW1) ist die
 * Geraete-PIN im Chip; die Admin-PIN (PW3) und der Resetting Code liegen im
 * Tresor, mit ihrem Fehlversuchszaehler im selben Datensatz.
 *
 * Elliptische Kurven und RSA. Welche RSA-Laengen wirklich gehen, sagt
 * der Chip und nicht diese Datei (osk_rsa_card.h) - der SE051A2 kann RSA
 * ueberhaupt nicht und antwortet auf derselben Adresse wie der C2.
 * Was fehlt, steht in docs/smartcard.md.
 */
#ifndef OSK_OPENPGP_H
#define OSK_OPENPGP_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_applets.h"

/* Kennung nach Abschnitt 4.2.1: RID D27600012401, Anwendung 01,
 * Fassung 0304, Hersteller 2 Bytes, Seriennummer 4 Bytes, dann 0000. */
#define OSK_PGP_AID_LEN 16u

/* Die drei Schluessel. Die Zahlen sind unsere, nicht die der Spezifikation. */
#define OSK_PGP_KEY_SIGN 0u
#define OSK_PGP_KEY_DEC 1u
#define OSK_PGP_KEY_AUTH 2u

/* Kennung im Chip: 'PG' und die Nummer des Schluessels. */
#define OSK_PGP_KEY_ID(n) (0x50470000u | (uint32_t)(n))

/* Algorithmen, die wir anbieten (Abschnitt 4.4.3.9, Nummern nach
 * RFC 4880 Abschnitt 9.1). RSA kann die Karte in allen drei Laengen des
 * Chips, weil OpenPGP die Bitlaenge als Zahl im Datenobjekt mitschickt -
 * PIV kann nur 2048, weil es dort eine Algorithmuskennung je Laenge
 * braucht und SP 800-73-4 nur eine hat (siehe osk_piv.h). */
#define OSK_PGP_ALG_RSA 1u
#define OSK_PGP_ALG_ECDSA 19u
#define OSK_PGP_ALG_ECDH 18u

const osk_applet_t *osk_openpgp_applet(void);

/* Fuer die Pruefungen: die Kennung, wie sie GET DATA 4F liefert. */
const uint8_t *osk_openpgp_aid(void);

#endif
