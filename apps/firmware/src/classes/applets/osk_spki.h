/* SubjectPublicKeyInfo aus einem Schluessel im Sicherheitschip.
 *
 * WARUM GETRENNT: diese paar Zeilen sind das Bindeglied zwischen dem
 * Vertrag mit dem Chip (osk_se050.h) und allem, was ein X.509-Gebilde
 * schreibt - das Attestierungszertifikat (osk_attest.h) und die
 * Zertifikatsanfrage (osk_csr.h) brauchen genau dieselben Bytes. Sie
 * standen bis zum 30.10.2026 in osk_attest.c, weil es nur einen Aufrufer
 * gab.
 *
 * Es sind drei Dinge, die hier zusammenkommen, und alle drei sind Stellen,
 * an denen aus einem richtigen Schluessel ein falsches Zertifikat wird:
 *   - der Chip haengt den Kurvenpunkt hinten an eine Verpackung; er wird
 *     herausgeholt (osk_card_public_point()) und nicht angenommen,
 *   - RSA kommt als Modulus und Exponent getrennt, und der Exponent ist
 *     drei ODER vier Byte lang (osk_se050.h sagt, warum),
 *   - der Parameter im AlgorithmIdentifier ist bei einer Kurve die
 *     Kurvenkennung und bei RSA ein ausdruecklicher NULL-Wert. Nicht
 *     "nichts".
 *
 * Aufbau nach RFC 5280 Abschnitt 4.1.2.7, Kennungen nach RFC 5480 und
 * RFC 8017 Anhang A.1.
 */
#ifndef OSK_SPKI_H
#define OSK_SPKI_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "osk_apdu.h"

/* Welche Art Schluessel im Chip liegt.
 *
 * Eine eigene Aufzaehlung und nicht die PIV-Algorithmuskennung: SP 800-73-4
 * hat fuer RSA-3072 und RSA-4096 keine Kennung (osk_piv.h sagt, warum),
 * OpenPGP kann alle drei Laengen. Die Laenge selbst steht hier nicht - sie
 * wird beim Chip erfragt und hat damit genau eine Quelle. */
typedef enum {
	OSK_SPKI_P256,
	OSK_SPKI_P384,
	OSK_SPKI_RSA,
} osk_spki_key_t;

/* Schreibt das SubjectPublicKeyInfo des Schluessels `id` nach `r`.
 *
 * Fehler:
 *   OSK_ERR_ARG         unbekannte Schluesselart
 *   OSK_ERR_NOT_FOUND   im Chip liegt dieser Schluessel nicht
 *   OSK_ERR_UNSUPPORTED RSA verlangt, aber dieses Geraet kann kein RSA -
 *                       entweder ohne RSA gebaut oder ein SE051A2 verbaut
 *   OSK_ERR_FORMAT      der Chip liefert etwas, das kein Schluessel dieser
 *                       Art sein kann
 *
 * Ein Ueberlauf von `r` wird NICHT gemeldet: dafuer ist `r->overflow` da,
 * und es wird einmal am Ende geprueft, nicht bei jedem Stueck. */
osk_result_t osk_spki_write(osk_resp_t *r, osk_key_id_t id, osk_spki_key_t type);

/* Bytes, die der Kurvenpunkt dieser Art belegt (0x04 || X || Y): 65 bei
 * P-256, 97 bei P-384, 0 bei RSA. Gebraucht von jedem, der den Punkt aus
 * der Verpackung des Chips holt. */
size_t osk_spki_point_len(osk_spki_key_t type);

#endif
