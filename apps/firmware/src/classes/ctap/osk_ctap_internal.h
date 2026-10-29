/* Innenleben des CTAP-Kerns: Zahlen aus der Spezifikation, Obergrenzen und
 * der Zustand, den die Befehle miteinander teilen.
 *
 * Grundlage: CTAP 2.2 (Proposed Standard, 14.07.2025) und WebAuthn Level 3.
 * Jede Zahl hier steht so in der Spezifikation - nichts geraten. */
#ifndef OSK_CTAP_INTERNAL_H
#define OSK_CTAP_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_ctap.h"
#include "osk_cbor.h"

/* ---- Befehlsnummern (CTAP 2.2, Abschnitt 8.1) --------------------------- */
#define OSK_CTAP_CMD_MAKE_CREDENTIAL 0x01
#define OSK_CTAP_CMD_GET_ASSERTION 0x02
#define OSK_CTAP_CMD_GET_INFO 0x04
#define OSK_CTAP_CMD_CLIENT_PIN 0x06
#define OSK_CTAP_CMD_RESET 0x07
#define OSK_CTAP_CMD_GET_NEXT 0x08
#define OSK_CTAP_CMD_BIO_ENROLLMENT 0x09
#define OSK_CTAP_CMD_CRED_MGMT 0x0a
#define OSK_CTAP_CMD_SELECTION 0x0b
#define OSK_CTAP_CMD_LARGE_BLOBS 0x0c
#define OSK_CTAP_CMD_CONFIG 0x0d

/* ---- Antwortwerte (CTAP 2.2, Abschnitt 8.2) ----------------------------- */
#define CTAP2_OK 0x00
#define CTAP1_ERR_INVALID_COMMAND 0x01
#define CTAP1_ERR_INVALID_PARAMETER 0x02
#define CTAP1_ERR_INVALID_LENGTH 0x03
#define CTAP2_ERR_CBOR_UNEXPECTED_TYPE 0x11
#define CTAP2_ERR_INVALID_CBOR 0x12
#define CTAP2_ERR_MISSING_PARAMETER 0x14
#define CTAP2_ERR_LIMIT_EXCEEDED 0x15
#define CTAP2_ERR_LARGE_BLOB_FULL 0x18
#define CTAP2_ERR_CREDENTIAL_EXCLUDED 0x19
#define CTAP2_ERR_INVALID_CREDENTIAL 0x22
#define CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26
#define CTAP2_ERR_OPERATION_DENIED 0x27
#define CTAP2_ERR_KEY_STORE_FULL 0x28
#define CTAP2_ERR_UNSUPPORTED_OPTION 0x2b
#define CTAP2_ERR_INVALID_OPTION 0x2c
#define CTAP2_ERR_KEEPALIVE_CANCEL 0x2d
#define CTAP2_ERR_NO_CREDENTIALS 0x2e
#define CTAP2_ERR_USER_ACTION_TIMEOUT 0x2f
#define CTAP2_ERR_NOT_ALLOWED 0x30
#define CTAP2_ERR_PIN_INVALID 0x31
#define CTAP2_ERR_PIN_BLOCKED 0x32
#define CTAP2_ERR_PIN_AUTH_INVALID 0x33
#define CTAP2_ERR_PIN_AUTH_BLOCKED 0x34
#define CTAP2_ERR_PIN_NOT_SET 0x35
#define CTAP2_ERR_PUAT_REQUIRED 0x36
#define CTAP2_ERR_PIN_POLICY_VIOLATION 0x37
#define CTAP2_ERR_REQUEST_TOO_LARGE 0x39
#define CTAP2_ERR_UP_REQUIRED 0x3b
#define CTAP2_ERR_UV_BLOCKED 0x3c
#define CTAP2_ERR_INVALID_SUBCOMMAND 0x3e
#define CTAP2_ERR_UV_INVALID 0x3f
#define CTAP2_ERR_UNAUTHORIZED_PERM 0x40
#define CTAP1_ERR_OTHER 0x7f

/* ---- Flaggen im Authenticator Data (WebAuthn L3, Abschnitt 6.1) --------- */
#define OSK_AD_FLAG_UP 0x01
#define OSK_AD_FLAG_UV 0x04
#define OSK_AD_FLAG_BE 0x08
#define OSK_AD_FLAG_BS 0x10
#define OSK_AD_FLAG_AT 0x40
#define OSK_AD_FLAG_ED 0x80

/* COSE: ES256 */
#define OSK_COSE_ALG_ES256 (-7)

/* Schutzstufen der credProtect-Erweiterung (CTAP 2.2, Abschnitt 12.1) */
#define OSK_CRED_PROTECT_OPTIONAL 0x01
#define OSK_CRED_PROTECT_WITH_ID_LIST 0x02
#define OSK_CRED_PROTECT_UV_REQUIRED 0x03

/* Rechte eines pinUvAuthToken (CTAP 2.2, Abschnitt 6.5.5.7) */
#define OSK_PERM_MC 0x01
#define OSK_PERM_GA 0x02
#define OSK_PERM_CM 0x04
#define OSK_PERM_BE 0x08
#define OSK_PERM_LBW 0x10
#define OSK_PERM_ACFG 0x20

/* ---- Obergrenzen. Nichts waechst zur Laufzeit. ------------------------- */
#define OSK_MAX_MSG 1200 /* maxMsgSize, in GetInfo gemeldet */
#define OSK_MAX_RP_ID 128
#define OSK_MAX_USER_ID 64 /* WebAuthn: hoechstens 64 Bytes */
#define OSK_MAX_USER_NAME 64
#define OSK_MAX_CRED_ID 128  /* maxCredentialIdLength */
#define OSK_MAX_LIST 8	     /* maxCredentialCountInList */
#define OSK_MAX_CANDIDATES 8 /* Treffer fuer GetNextAssertion */
/* Wie viele auffindbare (residente) Nachweise das Geraet fuehrt. Die Zahl
 * steht NUR hier: GetInfo meldet daraus den Rest (0x14), und
 * authenticatorCredentialManagement 0x01 meldet dieselbe Rechnung noch
 * einmal. Zwei Stellen mit derselben Zahl waeren zwei Wahrheiten. */
#define OSK_MAX_RESIDENT 64
#define OSK_MAX_EXT_OUT 256  /* Erweiterungsausgaben im authData */
#define OSK_MAX_AUTH_DATA (37 + 16 + 2 + OSK_MAX_CRED_ID + 80 + OSK_MAX_EXT_OUT)
#define OSK_MAX_SIG 80	      /* ECDSA P-256 in DER */
#define OSK_MAX_PIN_BYTES 63  /* UTF-8, Spezifikation 6.5.1 */
#define OSK_PIN_MIN_DEFAULT 4 /* Code-Punkte */
/* Obergrenze der PIN-Laenge in Code-Punkten (Abschnitt 6.5.1). Sie
 * begrenzt, was setMinPINLength als neue Mindestlaenge annehmen darf: eine
 * Mindestlaenge oberhalb der Hoechstlaenge waere eine PIN, die niemand
 * setzen kann - das Geraet waere damit stillgelegt. */
#define OSK_PIN_MAX_CODE_POINTS 63

/* Zeit, die der Nutzer zum Bestaetigen hat. */
#define OSK_UP_TIMEOUT_MS 30000u
/* GetNextAssertion muss innerhalb von 30 s kommen (Abschnitt 6.3). */
#define OSK_GET_NEXT_WINDOW_MS 30000u

/* Ein gefundener Berechtigungsnachweis, wie ihn GetAssertion braucht. */
typedef struct {
	uint8_t cred_id[OSK_MAX_CRED_ID];
	size_t cred_id_len;
	uint8_t user_id[OSK_MAX_USER_ID];
	size_t user_id_len;
	char user_name[OSK_MAX_USER_NAME];
	bool resident;
	uint8_t cred_protect;
	bool hmac_secret;
	uint32_t created;
} osk_cred_t;

/* Gemerkter Zustand fuer GetNextAssertion (Abschnitt 6.3). */
typedef struct {
	bool active;
	uint8_t rp_id_hash[32];
	uint8_t client_data_hash[32];
	uint8_t flags; /* UP/UV, wie in der ersten Antwort */
	osk_cred_t creds[OSK_MAX_CANDIDATES];
	size_t count;
	size_t next; /* credentialCounter */
	uint64_t last_use_ms;
	bool want_hmac_secret; /* Erweiterungen der ersten Anfrage */
	uint8_t hmac_out[64];
	size_t hmac_out_len;
} osk_get_next_t;

/* Stand einer laufenden Aufzaehlung von authenticatorCredentialManagement
 * (Abschnitt 6.8). Gemerkt wird NUR ein Zeiger, keine Kopie der Eintraege:
 * jeder GetNext-Aufruf laeuft den Tresor neu durch. Eine Kopie waere
 * schneller, waere aber ein zweiter Stand neben dem Tresor - und damit
 * wieder zwei Groessen, die gleich heissen. */
typedef struct {
	bool rp_active;	  /* enumerateRPs laeuft */
	bool cred_active; /* enumerateCredentials laeuft */
	size_t cursor;	  /* naechster Platz im Tresor, ab 0 */
	uint8_t rp_id_hash[32]; /* Filter von enumerateCredentialsBegin */
} osk_cm_enum_t;

/* Der gesamte Zustand des CTAP-Kerns. Eine Instanz, kein dynamischer Speicher. */
typedef struct {
	bool ready;
	uint32_t extensions; /* OSK_CTAP_EXT_* */
	osk_get_next_t next_state;
	osk_cm_enum_t cm_enum;
	uint8_t min_pin_len; /* Code-Punkte */
	bool force_pin_change;
	/* Enterprise Attestation eingeschaltet (Config 0x01). Ab Werk aus -
	 * sie gibt eine Geraetekennung heraus und gehoert dem Betrieb, nicht
	 * der Webseite.
	 *
	 * Die Liste der RP-IDs, die die minPinLength-Erweiterung erfahren
	 * duerfen, steht NICHT hier, sondern allein in osk_ctap_ext.c. Sie
	 * stand bis zum 29.10.2026 doppelt: ein Feld min_pin_rp[2] hier, das
	 * nie beschrieben und nie gelesen wurde, und die echte Liste dort.
	 * Config 0x03 setzt sie jetzt, und ein zweiter toter Platz daneben
	 * waere genau der Fehler, der in diesem Projekt siebenmal vorkam. */
	bool ep_enabled;
} osk_ctap_state_t;

osk_ctap_state_t *osk_ctap_state(void);

/* Zeitquelle, in Millisekunden seit dem Start. In den Tests ersetzbar. */
uint64_t osk_ctap_now_ms(void);

/* Baut den Authenticator-Data-Block. ext_cbor darf NULL sein. */
osk_result_t osk_ctap_build_auth_data(const uint8_t rp_id_hash[32], uint8_t flags,
				      uint32_t sign_count, const uint8_t *attested,
				      size_t attested_len, const uint8_t *ext_cbor, size_t ext_len,
				      uint8_t *out, size_t cap, size_t *len_out);

/* Schreibt einen oeffentlichen P-256-Schluessel als COSE_Key (kanonisch). */
void osk_ctap_put_cose_p256(osk_cbor_writer_t *w, const uint8_t pub[64]);

/* Die Kennung dieses Geraetemodells. KEIN fester Wert mehr, sondern einer je
 * Firmware-Variante: FIDO verlangt, dass ein AAGUID eine Fassung mit
 * DENSELBEN Faehigkeiten bezeichnet, und die Fassung mit RSA kann mehr als
 * die ohne. Welche gilt, entscheidet CONFIG_OSK_RSA zur Uebersetzungszeit -
 * siehe osk_ctap_core.c. */
const uint8_t *osk_ctap_aaguid(void);

/* ---- Residente Nachweise im Tresor ------------------------------------
 *
 * Ein residenter (auffindbarer) Nachweis liegt als Tresoreintrag vom Typ
 * OSK_ITEM_FILE mit site == OSK_RK_SITE; der Suchschluessel (tag) ist der
 * rpIdHash, der Name des Eintrags die RP-ID. Das Geheimnis ist der unten
 * beschriebene Satz.
 *
 * Der Kern erzeugt diese Eintraege (MakeCredential), die Verwaltung
 * (osk_ctap_cm.c) zaehlt, aendert und loescht sie. Darum liegt das Format
 * hier und nicht in einer der beiden Dateien. */
#define OSK_RK_SITE "osk-passkey"
#define OSK_RK_RECORD_VERSION 0x01
#define OSK_RK_RECORD_MAX (1 + 1 + OSK_MAX_USER_ID + 1 + OSK_MAX_USER_NAME + 1 + OSK_MAX_CRED_ID)

/* Satz zusammensetzen. Liefert die Laenge oder 0, wenn etwas nicht passt. */
size_t osk_ctap_rk_pack(const uint8_t *cred_id, size_t cred_id_len, const uint8_t *user_id,
			size_t user_id_len, const char *user_name, uint8_t *out, size_t cap);

/* Satz auseinandernehmen. false, wenn er nicht von uns stammt. */
bool osk_ctap_rk_unpack(const uint8_t *rec, size_t len, osk_cred_t *out);

#endif
