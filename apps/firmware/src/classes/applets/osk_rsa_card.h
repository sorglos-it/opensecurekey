/* Was PIV und OpenPGP beim RSA gemeinsam brauchen.
 *
 * Die Chipseite ist fertig und gehoert Strang A (osk_se050.h, Abschnitt
 * RSA). Hier liegt die Kartenseite: die Umrechnung zwischen dem, was ein
 * Kartenprogramm schickt, und dem, was der Vertrag verlangt.
 *
 * DREI DINGE, DIE DER VERTRAG ABSICHTLICH SO HAT und die genau hier
 * ankommen:
 *
 * 1. Die Auffuellung wird immer genannt, sie ist nie die Vorgabe. PIV
 *    benutzt PKCS#1 v1.5, OpenPGP beides. Jeder Aufruf in diesem Modul
 *    nennt sie.
 *
 * 2. `osk_hash_t` ist Pflicht und aus der Laenge nicht ableitbar -
 *    SHA-512/256 und SHA-256 sind beide 32 Byte lang. Darum wird das
 *    Verfahren NICHT an der Laenge erkannt, sondern an der Kennung im
 *    DigestInfo. Wer hier die Laenge nimmt, baut genau den Fehler, vor dem
 *    der Vertrag warnt.
 *
 * 3. Entschluesseln gehoert dazu: PIV-Steckplatz 9D und der
 *    OpenPGP-Verschluesselungsschluessel. Ohne das bleibt verschluesselte
 *    Post ungelesen.
 *
 * UND DIE WARNUNG VON STRANG A: osk_se050_rsa_sizes() fragt den Chip ab
 * und nimmt nichts an. Der SE051A2 kann RSA ueberhaupt nicht, der C2 kann
 * es, und beide antworten auf dieselbe Adresse. Liefert die Abfrage 0,
 * arbeiten beide Anwendungen OHNE RSA weiter und melden den RSA-Kommandos
 * "Funktion nicht vorhanden" - sie stuerzen nicht ab und sie tun auch
 * nicht so, als waere ein Schluessel entstanden.
 *
 * ZWEI FASSUNGEN DER FIRMWARE: mit und ohne RSA, als Kconfig-Option
 * CONFIG_OSK_RSA (docs/plan.md, "Querschnitt"). Die Option hat GENAU EINE
 * Stelle, an der sie wirkt: osk_rsa_available() antwortet ohne sie immer
 * false. Danach biegt jede Anwendung von selbst auf den Weg ohne RSA ab,
 * weil sie dieselbe Frage stellt wie bei einem Chip ohne RSA.
 *
 * Dass der uebrige Code dieses Moduls in beiden Fassungen mituebersetzt
 * wird, ist Absicht und kein Versehen: ein Pfad, den nur eine der beiden
 * Fassungen ueberhaupt dem Uebersetzer zeigt, ist in der anderen nicht
 * geprueft. Was die Fassung ohne RSA wirklich spart, liegt im Treiber
 * (src/classes/se050/) und nicht hier.
 *
 * Vorsicht beim Lesen: "gebaut" und "im Chip vorhanden" sind zwei
 * verschiedene Fragen, und beide muessen ja sagen.
 */
#ifndef OSK_RSA_CARD_H
#define OSK_RSA_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "osk_apdu.h"

/* IST RSA MITGEBAUT? Nur fuer Pruefungen und Berichte gedacht. Der Code
 * selbst fragt osk_rsa_available(), denn "gebaut" und "im Chip vorhanden"
 * sind zwei Fragen, und beide muessen ja sagen. Die Pruefungen brauchen
 * die Unterscheidung aber: ihre Erwartungen sind in den zwei Fassungen
 * verschieden, und eine Option, die nur in einer Richtung geprueft ist,
 * ist keine (docs/plan.md, Abschnitt 8). */
#if defined(CONFIG_OSK_RSA)
#define OSK_RSA_BUILT 1
#else
#define OSK_RSA_BUILT 0
#endif

/* Laengster Modulus, den wir annehmen: 4096 Bit. */
#define OSK_RSA_MOD_MAX 512u

/* Laengstes DigestInfo: 19 Byte Vorsatz + 64 Byte SHA-512. */
#define OSK_RSA_DI_MAX 83u

/* --- Ist RSA ueberhaupt da? -------------------------------------------- */

/* Fragt den Chip einmal je Einschaltvorgang und merkt sich die Antwort.
 * false heisst: dieses Geraet kann kein RSA - entweder ist die Firmware
 * ohne RSA gebaut, oder im Geraet sitzt ein SE051A2. */
bool osk_rsa_available(void);

/* Kann der Chip diese Laenge? Fragt nur die gemerkte Maske, nicht den
 * Chip. false auch dann, wenn `bits` keine der drei Laengen ist. */
bool osk_rsa_bits_ok(uint16_t bits);

/* Bitlaenge in die Aufzaehlung des Vertrags. false bei allem anderen. */
bool osk_rsa_bits_enum(uint16_t bits, osk_rsa_bits_t *out);

/* Bytes, die ein Modulus dieser Bitlaenge belegt. 0 bei unbekannter Laenge. */
size_t osk_rsa_mod_bytes(uint16_t bits);

void osk_rsa_forget(void);

/* --- DigestInfo und der PKCS#1-v1.5-Block ------------------------------ */

/* Zerlegt ein DigestInfo, wie RFC 8017 Abschnitt 9.2 es vorschreibt.
 *
 * Darauf laeuft Punkt 2 oben hinaus: OpenPGP schickt beim Unterschreiben
 * KEINEN nackten Hash, sondern das DigestInfo - die Kennung des Verfahrens
 * und den Hash zusammen (Spezifikation 3.4 Abschnitt 7.2.10). Der Vertrag
 * will umgekehrt den nackten Hash und die Kennung getrennt. Diese Funktion
 * ist die Naht dazwischen.
 *
 * `*hash_out` zeigt in `in` und gilt nur so lange wie dieser Puffer.
 * false bei allem, was nicht genau eines der drei DigestInfo ist - auch
 * bei einem, das richtig aussieht und ein Byte zu lang ist. */
bool osk_rsa_digestinfo_parse(const uint8_t *in, size_t len, osk_hash_t *alg_out,
			      const uint8_t **hash_out, size_t *hash_len_out);

/* Packt einen vollstaendigen Unterschriftenblock aus:
 *   00 01 FF..FF 00 || DigestInfo      (RFC 8017 Abschnitt 9.2, EMSA-PKCS1-v1_5)
 *
 * PIV laesst das Auffuellen beim Aufrufer und erwartet, dass die Karte den
 * fertigen Block mit dem privaten Schluessel rechnet (SP 800-73-4 Teil 2
 * Abschnitt 3.2.4). Diese Funktion macht daraus wieder Hash und Kennung,
 * damit die Unterschrift ueber den benannten, aufgefuellten Weg des
 * Vertrags laeuft - siehe die Begruendung in osk_piv.c bei do_rsa_sign().
 *
 * Nimmt den Block mit fuehrendem 00 (Laenge = Modulus) und ohne (ein Byte
 * kuerzer), weil Kartenprogramme es unterschiedlich halten. Verlangt
 * mindestens acht Auffuellbytes, wie RFC 8017 es vorschreibt. */
bool osk_rsa_pkcs1_unwrap(const uint8_t *block, size_t block_len, size_t mod_len,
			  osk_hash_t *alg_out, const uint8_t **hash_out, size_t *hash_len_out);

/* --- Oeffentlicher Teil ------------------------------------------------ */

/* Bitlaenge des Modulus eines vorhandenen Schluessels. Gefragt wird der
 * Chip; geraten wird nichts. 0, wenn es den Schluessel nicht gibt. */
uint16_t osk_rsa_modulus_bits(osk_key_id_t id);

/* Schreibt 7F49 { 81 = Modulus, 82 = Exponent } - die Form, die PIV
 * (Teil 2 Abschnitt 3.3.1) und OpenPGP (Abschnitt 7.2.14) beide fuer RSA
 * verlangen.
 *
 * Der Exponent kommt in der Laenge heraus, die der Chip meldet: drei oder
 * vier Bytes. Wer 65537 annimmt, baut irgendwann ein falsches Zertifikat -
 * so steht es im Vertrag, und darum steht hier keine Annahme. */
uint16_t osk_rsa_put_public(osk_key_id_t id, osk_resp_t *res);

/* Modulus und Exponent rohe, fuer das Attestierungszertifikat. */
osk_result_t osk_rsa_public_parts(osk_key_id_t id, uint8_t *mod, size_t mod_cap, size_t *mod_len,
				  uint8_t *exp, size_t exp_cap, size_t *exp_len);

#endif
