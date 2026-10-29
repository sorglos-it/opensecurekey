/* Innenleben der Rechteschicht. Nur fuer src/classes/authz/.
 *
 * Alles hier ist absichtlich fester Speicher: kein malloc, nirgends. Der
 * Plan verlangt "kein dynamischer Speicher in Krypto-Pfaden", und dieser
 * Pfad entscheidet ueber jede Freigabe. */
#ifndef AUTHZ_STATE_H
#define AUTHZ_STATE_H

#include "osk_authz.h"

/* Eine Anmeldung in Arbeit oder fertig.
 *
 * `authenticated` ist der einzige Schalter, der zaehlt: `osk_authz_check()`
 * sieht nur ihn an. Eine begonnene, aber nicht bewiesene Sitzung ist nichts
 * wert, und die Trennung in zwei Felder ist der Grund, warum sie das auch
 * im Code nicht ist. */
typedef struct {
	bool used;
	bool authenticated;
	uint16_t key_id;
	uint8_t device_challenge[OSK_AUTHZ_CHALLENGE_LEN];
	uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN];
} authz_session_slot_t;

typedef struct {
	authz_session_slot_t sessions[OSK_AUTHZ_SESSION_MAX];
	osk_authz_key_info_t keys[OSK_AUTHZ_KEY_MAX];

	struct {
		bool used;
		osk_authz_cred_t cred;
		uint8_t verifier[16]; /* HMAC ueber das Passwort, gekuerzt */
		uint8_t fails;
		bool locked;
	} creds[OSK_AUTHZ_CRED_MAX];

	bool ready;
} authz_state_t;

authz_state_t *authz_state(void);

/* Vergleich mit gleicher Laufzeit. Gibt 0 bei Gleichheit.
 *
 * Nicht memcmp(): memcmp bricht beim ersten Unterschied ab, und daraus
 * laesst sich ablesen, wie viele Bytes eines geratenen Beweises schon
 * stimmten. Genau diese Fehlerklasse hat 2024 den Marktfuehrer getroffen
 * (EUCLEAK, siehe docs/plan.md Abschnitt 4). */
int authz_equal_ct(const void *a, const void *b, size_t len);

/* Die drei Trennzeichenketten dieser Schicht. Sie unterscheiden sich,
 * damit ein Beweis aus einem Zusammenhang nicht in einem anderen gilt. */
#define AUTHZ_CTX_HOST	 "osk-authz-host-1"
#define AUTHZ_CTX_DEVICE "osk-authz-dev-1"
#define AUTHZ_CTX_PW	 "osk-authz-pw-verify-1"
#define AUTHZ_CTX_KDF	 "osk-authz-session-1"
#define AUTHZ_CTX_TSA	 "osk-authz-tsa-stmt-1"

/* Den Beweis einer Seite rechnen. `ctx` ist eine der Zeichenketten oben. */
osk_result_t authz_proof(osk_key_id_t secret, const char *ctx,
			 const uint8_t device_challenge[OSK_AUTHZ_CHALLENGE_LEN],
			 const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
			 uint8_t out[OSK_AUTHZ_PROOF_LEN]);

/* Eine Sitzung nachsehen, die angemeldet ist. NULL, wenn die Nummer nicht
 * stimmt oder die Sitzung nur begonnen wurde. */
authz_session_slot_t *authz_session_live(osk_authz_session_t s);

/* Den Eintrag zu einer Kennung finden. NULL, wenn es ihn nicht gibt. */
osk_authz_key_info_t *authz_find_key(uint16_t key_id);

/* Geraetezeit in Millisekunden seit dem Einstecken.
 *
 * ES IST KEINE WELTZEIT, und der Name sagt das. Das Geraet hat keinen Akku
 * (docs/plan.md), die Uhr beginnt bei jedem Einstecken bei null. Wer
 * daraus ein Datum macht, macht einen Fehler - siehe docs/zeitstempel.md.
 * Am Geraet liefert das k_uptime_get_32(), am PC der Pruefrahmen. */
uint32_t authz_device_ms(void);

#endif
