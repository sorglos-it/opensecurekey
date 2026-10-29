/* Gemeinsame Hilfen der Prueffaelle der Rechteschicht.
 *
 * Der Pruefrahmen selbst (tiny_test.h) wird von test/se050/ mitbenutzt und
 * NICHT kopiert: eine zweite Fassung wuerde still auseinanderlaufen, und
 * dieses Projekt hat das Muster schon oft genug gesehen. */
#ifndef AUTHZ_T_H
#define AUTHZ_T_H

#include <string.h>
#include "tiny_test.h"
#include "osk_authz.h"
#include "fake_se050.h"

/* Einen Namen in einen Prueffall schreiben: laengenbegrenzt kopieren statt
 * strcpy. sizeof(s) schliesst die abschliessende Null ein, und das Ziel ist
 * immer OSK_AUTHZ_LABEL_MAX + 1 gross - die Laenge steht damit im Aufruf
 * und nicht in einer Annahme ueber die Zeichenkette. */
#define T_LABEL(dst, s) (void)memcpy((dst), (s), sizeof(s))

/* Kennungen, die die Prueffaelle benutzen. Fest, damit ein Fehlerbericht
 * immer dieselbe Zahl nennt. */
#define T_SECRET_ADMIN	0x00A0A000u
#define T_SECRET_WEB	0x00A0A001u
#define T_SECRET_WALLET 0x00A0A002u
#define T_SECRET_CRED	0x00A0A003u
#define T_SIGNING_KEY	0x00A0A004u
#define T_COUNTER	0x00A0A005u

#define T_KEY_ADMIN  1u
#define T_KEY_WEB    2u
#define T_KEY_WALLET 3u

/* Die Bereiche, in denen gearbeitet wird. Zwei, die sich nicht
 * ueberschneiden - das ist der Prueffall, auf den es ankommt. */
#define T_REALM_WEB    OSK_REALM(1)
#define T_REALM_WALLET OSK_REALM(5)

/* Vollstaendig anmelden: begin, Beweis rechnen, authenticate. Gibt
 * OSK_AUTHZ_NO_SESSION, wenn etwas schiefgeht - der Aufrufer prueft das. */
osk_authz_session_t t_login(uint16_t key_id, osk_key_id_t secret);

/* Nur anmelden versuchen, mit einem Beweis, der falsch sein darf. Liefert,
 * was osk_authz_session_authenticate() gemeldet hat. */
osk_result_t t_login_with(uint16_t key_id, const uint8_t *proof, size_t proof_len);

/* Den Grundzustand herstellen: Chip-Nachbau leer, Schicht angemeldet,
 * Admin vorhanden. */
void t_setup(void);

void test_realm(void);
void test_keys(void);
void test_cred(void);
void test_der(void);
void test_tsa(void);

#endif
