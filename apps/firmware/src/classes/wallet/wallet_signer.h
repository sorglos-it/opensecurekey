/* Unterschreiben - und zwar im SE050, nicht hier.
 *
 * Die Regel dieses Moduls in einem Satz: ein abgeleiteter Schluessel wird
 * in den Chip gegeben, der Chip unterschreibt, und was herauskommt wird
 * gegen den oeffentlichen Punkt nachgeprueft, bevor es das Geraet verlaesst.
 * Kann der Chip die Kurve nicht, kommt OSK_ERR_UNSUPPORTED - es wird NICHT
 * in der Firmware gerechnet. Ein geliefertes SE051 kann ab Werk ohne EdDSA
 * und Curve25519 kommen; secp256k1 ist davon nicht betroffen, aber gefragt
 * wird trotzdem (osk_se050_curves), nicht angenommen.
 *
 * Warum nachgeprueft wird: eine falsche Unterschrift auf einer Ueberweisung
 * kostet das Geld zweimal - einmal durch den Fehler und einmal durch den
 * zweiten Versuch. Die Pruefung kostet eine Punktvervielfachung. */
#ifndef WALLET_SIGNER_H
#define WALLET_SIGNER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "wallet_ec.h"

/* FEHLT in apps/firmware/include/osk_se050.h.
 *
 * Diese eine Funktion braucht die Geldboerse, und sie kann nicht umgangen
 * werden: der abgeleitete Schluessel entsteht in der Firmware (der SE05x
 * kann nicht intern ableiten, docs/se050.md), und unterschreiben soll
 * trotzdem der Chip. AN12413 4.7.1.8 (WriteECKey) nimmt einen von aussen
 * gegebenen privaten Wert an - der Weg steht also offen, es fehlt nur der
 * Eintrag im Vertrag. Bis der Besitzer von osk_se050.h sie dort aufnimmt,
 * steht sie hier; die Attrappe der Pruefungen stellt sie bereit.
 *
 * `id` soll ein fluechtiges Objekt sein: es lebt bis zum naechsten
 * osk_se050_key_delete() oder osk_se050_close(). */
osk_result_t osk_se050_key_import_ec(osk_key_id_t id, osk_curve_t curve, const uint8_t *priv,
				     size_t len);

/* Kennung des fluechtigen Objekts, in dem die Geldboerse unterschreibt.
 * Ein fester Platz, der nach jeder Unterschrift geleert wird. */
#define OSK_WALLET_SIGN_SLOT 0x57414C31u /* "WAL1" */

/* Kann der Chip diese Kurve? Fragt den Chip, einmal je Sitzung
 * zwischengespeichert. */
osk_result_t osk_signer_supports(osk_curve_t curve, bool *ok_out);

/* Hash unterschreiben. r und s kommen als je 32 Byte heraus, s immer klein
 * (BIP62, EIP-2). `pub` darf NULL sein; dann wird der Punkt selbst
 * gerechnet, um die Antwort nachzupruefen. */
osk_result_t osk_signer_sign_hash(osk_curve_t curve, const uint8_t priv[32], const uint8_t hash[32],
				  const osk_ec_point_t *pub, uint8_t r_out[32], uint8_t s_out[32]);

/* Liest eine ECDSA-Signatur in DER-Form (so antwortet der SE050) in zwei
 * 32-Byte-Zahlen. Streng: keine fuehrenden Nullen zu viel, keine negativen
 * Zahlen, kein Anhang hinter der Folge. */
osk_result_t osk_signer_der_to_raw(const uint8_t *der, size_t len, uint8_t r_out[32],
				   uint8_t s_out[32]);

/* Umgekehrt - Bitcoin will die DER-Form im Skript. Braucht bis zu 72 Byte. */
osk_result_t osk_signer_raw_to_der(const uint8_t r[32], const uint8_t s[32], uint8_t *out,
				   size_t cap, size_t *len_out);

/* Nur fuer die Pruefungen: den gemerkten Kurvenbestand verwerfen, damit der
 * naechste Aufruf den Chip wieder fragt. */
void osk_signer_forget_curves(void);

#endif
