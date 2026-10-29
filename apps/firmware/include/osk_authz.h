/* Rechtebereiche, Zugangsschluessel, Zugangsdaten und Zeitstempel.
 *
 * NEUER VERTRAG, ENTWORFEN VON STRANG F am 29.10.2026 und dem Master
 * gemeldet. Kein vorhandener Vertrag ist dabei geaendert worden.
 *
 * Diese Schicht antwortet auf genau zwei Fragen, und sie beantwortet sie
 * vor jedem Vorgang:
 *
 *   1. WER fragt?     -> Zugangsschluessel (identitaetsbasiert, nicht Rolle)
 *   2. DARF der das?   -> Rechtebereich und Faehigkeit
 *
 * Die zweite Frage stellen heute `applets/`, `ctap/` und `store/` jeder fuer
 * sich oder gar nicht. Die erste stellt heute niemand: es gibt eine PIN, und
 * wer sie kennt, ist dasselbe wie jeder andere, der sie kennt.
 *
 * WARUM DAS EINE BAUAUFGABE IST UND KEINE FORMALIE: FIPS 140-3 verlangt ab
 * Stufe 3 **identitaetsbasierte** Authentifizierung, nicht rollenbasierte
 * (ISO/IEC 19790 Abschnitt 7.4.3). Der Unterschied ist nicht, wie streng
 * geprueft wird, sondern WAS hinterher im Protokoll steht: bei
 * rollenbasierter Pruefung steht dort "ein Verwalter", bei
 * identitaetsbasierter steht dort, welcher. Ohne das ist keine
 * Nachvollziehbarkeit moeglich und der Prueflauf nicht zu bestehen.
 * Nachtraeglich einzieht sich das nicht: jeder Vorgang muss eine Identitaet
 * mitfuehren, und das sind alle Aufrufwege.
 *
 * Umsetzung: src/classes/authz/
 * Befund zur Zeitstempel-Frage: docs/zeitstempel.md
 * Entwurf und Begruendungen: docs/rechtebereiche.md */
#ifndef OSK_AUTHZ_H
#define OSK_AUTHZ_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"

/* --- Rechtebereiche -----------------------------------------------------
 *
 * SECHZEHN Bereiche. Ein Schluessel und ein Zugangsschluessel tragen je
 * eine Maske; gearbeitet werden darf, wo sich beide ueberschneiden.
 *
 * WARUM SIE NICHT "DOMAIN" HEISSEN, obwohl der Plan von Rechtebereichen
 * spricht und die Vorlage am Markt sie Domain nennt: das Wort ist in diesem
 * Baum schon zweimal belegt, und zwar fuer etwas voellig anderes -
 * `attest_domain` in applets/guard/osk_guard_msg.c und `domain[32]` in
 * wallet/wallet_eth.c sind Trennzeichenketten der Signatur
 * (Domain Separation), damit eine Unterschrift aus einem Zusammenhang nicht
 * in einem anderen gilt. Zwei Groessen, die gleich heissen und
 * Verschiedenes messen, ist das durchgehende Fehlermuster dieses Projekts
 * (siehe docs/plan.md Abschnitt 6, dort sechsmal aufgezaehlt). Darum
 * `realm`, und darum steht der Grund hier und nicht nur im Commit. */

#define OSK_AUTHZ_REALM_COUNT 16u

/* Maske aus (1u << n), n von 0 bis 15. Eine leere Maske heisst "nirgends",
 * nicht "ueberall" - ein nicht gesetztes Recht ist kein Recht. */
typedef uint16_t osk_realm_mask_t;

#define OSK_REALM(n)	 ((osk_realm_mask_t)(1u << (n)))
#define OSK_REALM_NONE	 ((osk_realm_mask_t)0u)
#define OSK_REALM_ALL	 ((osk_realm_mask_t)0xFFFFu)

/* --- Faehigkeiten -------------------------------------------------------
 *
 * Was ein Zugangsschluessel tun darf. Jede Faehigkeit ist ein Vorgang, den
 * man einzeln wegnehmen kann - der Dienst, der Server-Zertifikate
 * ausstellt, braucht SIGN und nichts weiter.
 *
 * Es gibt KEINE Faehigkeit "alles". Wer alle 16 Bereiche und alle
 * Faehigkeiten haben soll, bekommt sie einzeln eingetragen; eine
 * Sammelfaehigkeit wuerde beim naechsten Hinzufuegen stillschweigend
 * mitwachsen. */
typedef uint32_t osk_cap_mask_t;

#define OSK_CAP_SIGN		 ((osk_cap_mask_t)(1u << 0))  /* unterschreiben */
#define OSK_CAP_DECRYPT		 ((osk_cap_mask_t)(1u << 1))  /* entschluesseln */
#define OSK_CAP_DERIVE		 ((osk_cap_mask_t)(1u << 2))  /* ECDH, HKDF */
#define OSK_CAP_KEY_CREATE	 ((osk_cap_mask_t)(1u << 3))  /* Schluessel anlegen */
#define OSK_CAP_KEY_DELETE	 ((osk_cap_mask_t)(1u << 4))  /* Schluessel loeschen */
#define OSK_CAP_PUBLIC_READ	 ((osk_cap_mask_t)(1u << 5))  /* oeffentlichen Teil lesen */
#define OSK_CAP_COUNTER_RAISE	 ((osk_cap_mask_t)(1u << 6))  /* monotonen Zaehler hochsetzen */
#define OSK_CAP_TIMESTAMP	 ((osk_cap_mask_t)(1u << 7))  /* Zeitstempel anfordern */
#define OSK_CAP_CRED_USE	 ((osk_cap_mask_t)(1u << 8))  /* Zugangsdaten benutzen */
#define OSK_CAP_CRED_MANAGE	 ((osk_cap_mask_t)(1u << 9))  /* Zugangsdaten anlegen/loeschen */
#define OSK_CAP_AUTHZ_MANAGE	 ((osk_cap_mask_t)(1u << 10)) /* Zugangsschluessel verwalten */
#define OSK_CAP_AUDIT_READ	 ((osk_cap_mask_t)(1u << 11)) /* Geraeteprotokoll lesen */
#define OSK_CAP_RANDOM		 ((osk_cap_mask_t)(1u << 12)) /* Zufall aus dem Chip holen */
#define OSK_CAP_WRAP		 ((osk_cap_mask_t)(1u << 13)) /* Schluessel einwickeln */
#define OSK_CAP_UNWRAP		 ((osk_cap_mask_t)(1u << 14)) /* Schluessel auswickeln */
#define OSK_CAP_RESET		 ((osk_cap_mask_t)(1u << 15)) /* Werkszustand */

#define OSK_CAP_NONE		 ((osk_cap_mask_t)0u)

/* Die hoechste belegte Faehigkeit. Masken mit Bits darueber sind ein
 * Fehler des Aufrufers und werden abgewiesen, statt still mitgefuehrt zu
 * werden - sonst traegt ein Zugangsschluessel Rechte, die es noch nicht
 * gibt, und bekommt sie beim naechsten Hinzufuegen geschenkt. */
#define OSK_CAP_DEFINED		 ((osk_cap_mask_t)0xFFFFu)

/* --- Zugangsschluessel --------------------------------------------------
 *
 * Mehrere, mit je eigenen Rechten, statt eines Hauptgeheimnisses. */

#define OSK_AUTHZ_KEY_MAX   8u	/* gleichzeitig eingetragene Zugangsschluessel */
#define OSK_AUTHZ_LABEL_MAX 40u /* Name, ohne abschliessende Null */

#define OSK_AUTHZ_CHALLENGE_LEN 16u /* beide Seiten, je eine */
#define OSK_AUTHZ_PROOF_LEN	32u /* HMAC-SHA256, ungekuerzt */

/* Beschreibung eines Zugangsschluessels, so wie ihn der Verwalter sieht.
 * Das Geheimnis ist NICHT dabei: es liegt im Sicherheitschip und wird nur
 * ueber `secret` angesprochen. */
typedef struct {
	uint16_t id;		     /* vom Verwalter vergeben, 1..65535; 0 ist frei */
	char label[OSK_AUTHZ_LABEL_MAX + 1]; /* mit Null, zum Anzeigen und Protokollieren */
	osk_key_id_t secret;	     /* HMAC-Schluessel im Chip */
	osk_realm_mask_t realms;     /* wo er arbeiten darf */
	osk_cap_mask_t caps;	     /* was er tun darf */
	osk_cap_mask_t delegated;    /* was er WEITERGEBEN darf, wenn er andere anlegt */
	uint8_t max_fail;	     /* 0 heisst: nie sperren */
	bool needs_touch;	     /* Tastendruck je Vorgang */
} osk_authz_key_t;

/* Ein Platz in der Tabelle hat drei Zustaende. `osk_authz_key_info()`
 * meldet ihn, damit "gesperrt" nicht als "nicht vorhanden" erscheint - das
 * sind verschiedene Befunde, und der Verwalter muss sie unterscheiden. */
typedef enum {
	OSK_AUTHZ_FREE,	  /* Platz unbelegt */
	OSK_AUTHZ_ACTIVE, /* eingetragen und benutzbar */
	OSK_AUTHZ_LOCKED, /* zu viele Fehlversuche */
} osk_authz_state_t;

typedef struct {
	osk_authz_state_t state;
	osk_authz_key_t key;
	uint8_t fails;	    /* Fehlversuche seit dem letzten Erfolg */
	uint32_t uses;	    /* erfolgreiche Anmeldungen */
} osk_authz_key_info_t;

/* Schicht anmelden. Ohne das antwortet nichts.
 *
 * `admin_secret` ist der Chip-Schluessel des ersten Zugangsschluessels -
 * des einzigen, der beim Erstlauf existiert. Ohne ihn liesse sich kein
 * erster Verwalter eintragen, und mit einer fest eingebauten Kennung waere
 * jedes Geraet mit demselben Geheimnis zu oeffnen. */
osk_result_t osk_authz_init(osk_key_id_t admin_secret);
void osk_authz_shutdown(void);

/* --- Anmelden: zwei Schritte, und beide sind noetig --------------------
 *
 * Erst die Identitaet nennen, dann sie beweisen. Zwei Aufrufe, weil das
 * Geraet seinen eigenen Zufall beisteuern muss - sonst koennte der Rechner
 * die Herausforderung waehlen und einen alten Beweis wiederverwenden.
 *
 *   1. begin(id, host_challenge, -> device_challenge)
 *   2. der Rechner rechnet:
 *        proof_host = HMAC(K, "osk-authz-host-1" || device_challenge
 *                                               || host_challenge)
 *   3. authenticate(session, proof_host, -> proof_device)
 *        proof_device = HMAC(K, "osk-authz-dev-1" || device_challenge
 *                                                 || host_challenge)
 *
 * Der Beweis des Geraets geht zurueck, damit auch der Rechner weiss, dass
 * er mit dem richtigen Geraet spricht. Einseitig geprueft waere die Haelfte
 * der Arbeit fuer keinen Gewinn: ein untergeschobenes Geraet nimmt die
 * Anmeldung sonst widerspruchslos an.
 *
 * Die beiden Trennzeichenketten unterscheiden sich, damit der Beweis des
 * Rechners nie als Beweis des Geraets durchgeht. */

typedef uint8_t osk_authz_session_t; /* Platznummer, 0..OSK_AUTHZ_SESSION_MAX-1 */

#define OSK_AUTHZ_SESSION_MAX 4u
#define OSK_AUTHZ_NO_SESSION  ((osk_authz_session_t)0xFFu)

osk_result_t osk_authz_session_begin(uint16_t key_id,
				     const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				     uint8_t device_challenge_out[OSK_AUTHZ_CHALLENGE_LEN],
				     osk_authz_session_t *session_out);

osk_result_t osk_authz_session_authenticate(osk_authz_session_t session,
					    const uint8_t host_proof[OSK_AUTHZ_PROOF_LEN],
					    uint8_t device_proof_out[OSK_AUTHZ_PROOF_LEN]);

void osk_authz_session_end(osk_authz_session_t session);

/* Alle Sitzungen beenden. Beim Abziehen des Sticks und beim Sperren -
 * `docs/plan.md` verlangt sofortiges Sperren beim Abziehen, und eine
 * ueberlebende Sitzung waere genau die Luecke darin. */
void osk_authz_session_end_all(void);

/* --- Die Pruefung, auf die alles hinauslaeuft --------------------------
 *
 * Jeder Vorgang fragt hier, und zwar MIT der Sitzung. Ohne Sitzung gibt es
 * keine Antwort "erlaubt": eine Funktion, die auch ohne Identitaet etwas
 * durchlaesst, macht die ganze Schicht wertlos.
 *
 * Geprueft wird dreierlei, und alles drei muss zutreffen:
 *   - die Sitzung ist angemeldet (nicht nur begonnen),
 *   - der Zugangsschluessel hat die Faehigkeit,
 *   - seine Bereichsmaske ueberschneidet sich mit der des Gegenstands.
 *
 * Die Ueberschneidung ist der Kern der Trennung: der Dienst fuer
 * Server-Zertifikate und die Geldboerse haben keinen gemeinsamen Bereich,
 * also kommt keiner an die Schluessel des anderen - auch wenn beide SIGN
 * duerfen. Die Faehigkeit allein trennt nicht. */
osk_result_t osk_authz_check(osk_authz_session_t session, osk_cap_mask_t need,
			     osk_realm_mask_t object_realms);

/* Wer in dieser Sitzung angemeldet ist - fuer das Geraeteprotokoll.
 * Das ist der Zweck der ganzen Uebung: der Eintrag nennt einen Namen. */
osk_result_t osk_authz_session_identity(osk_authz_session_t session, uint16_t *key_id_out,
					const char **label_out);

/* --- Zugangsschluessel verwalten ---------------------------------------
 *
 * Verwalten darf nur, wer OSK_CAP_AUTHZ_MANAGE hat, und er darf nur
 * weitergeben, was in seiner `delegated`-Maske steht.
 *
 * WARUM ZWEI MASKEN: Ohne die zweite koennte jeder, der Zugangsschluessel
 * anlegen darf, sich einen zweiten mit mehr Rechten anlegen und ueber ihn
 * alles tun. Die Rechtetrennung waere dann eine Hoeflichkeit. Mit der
 * zweiten Maske ist die Rechtemenge eines neuen Schluessels hoechstens die
 * `delegated` seines Erzeugers - und `delegated` selbst kann nicht wachsen. */
osk_result_t osk_authz_key_add(osk_authz_session_t session, const osk_authz_key_t *key);
osk_result_t osk_authz_key_remove(osk_authz_session_t session, uint16_t key_id);
osk_result_t osk_authz_key_info(uint16_t key_id, osk_authz_key_info_t *info_out);
osk_result_t osk_authz_key_list(uint16_t *ids_out, size_t cap, size_t *count_out);

/* Einen gesperrten Zugangsschluessel wieder freigeben. Braucht
 * OSK_CAP_AUTHZ_MANAGE - und der Schluessel kann sich nicht selbst
 * freigeben, denn zum Anmelden muesste er schon frei sein. */
osk_result_t osk_authz_key_unlock(osk_authz_session_t session, uint16_t key_id);

/* --- Zugangsdaten im Sicherheitschip (Teil 3) --------------------------
 *
 * WOFUER: Ein Server, der seinen Schluessel nicht im Dateisystem liegen
 * lassen soll (`docs/plan.md`, "Dongle und Server (NGC)": "Knotenschluessel
 * im Chip statt auf der Platte"). Der Server haelt ein Passwort, das
 * Langzeitgeheimnis liegt im Chip, und heraus kommen nur
 * **Sitzungsschluessel**, die mit dem Abziehen des Sticks wertlos sind.
 *
 * Was den Chip verlaesst und was nicht, ausdruecklich:
 *   - Langzeitgeheimnis: NIE. Es ist ein HMAC-Schluessel im SE05x.
 *   - Sitzungsschluessel: JA, das ist ihr Zweck - der Server verschluesselt
 *     damit. Wer sie abhoert, hat sie bis zum Ende der Sitzung; wer das
 *     Geraet stiehlt, hat trotzdem kein Langzeitgeheimnis.
 *
 * Ableitung: SP 800-108 Zaehlermodus mit HMAC-SHA256 als PRF.
 *
 * ABWEICHUNG, DIE DER MASTER ENTSCHEIDEN MUSS (gemeldet am 29.10.2026): Die
 * Vorlage am Markt leitet mit AES-CMAC nach SCP03 ab. Hier steht
 * HMAC-SHA256, aus zwei Gruenden - der Vertrag `osk_se050.h` fuehrt
 * `osk_se050_hmac()`, aber kein CMAC, und diese Schicht soll kein eigenes
 * Krypto-Grundverfahren mitbringen. Folge: **fremde Programme, die das
 * Protokoll der Vorlage sprechen, passen nicht.** Fuer NGC und `osk-guard`
 * ist das ohne Belang, denn das ist unsere eigene Software auf beiden
 * Seiten. Wer Fremdverträglichkeit will, braucht CMAC im Vertrag. */

#define OSK_AUTHZ_CRED_MAX	8u
#define OSK_AUTHZ_CRED_PW_MIN	8u
#define OSK_AUTHZ_CRED_PW_MAX	64u
#define OSK_AUTHZ_SESSION_KEY_LEN 16u

typedef struct {
	uint16_t id;
	char label[OSK_AUTHZ_LABEL_MAX + 1];
	osk_key_id_t secret;	 /* Langzeitgeheimnis im Chip */
	osk_realm_mask_t realms; /* in welchen Bereichen diese Zugangsdaten gelten */
	uint8_t max_fail;
	bool needs_touch;
} osk_authz_cred_t;

/* Das Paar, das herauskommt. Zwei Schluessel, weil Verschluesseln und
 * Sichern nie derselbe sein duerfen. */
typedef struct {
	uint8_t enc[OSK_AUTHZ_SESSION_KEY_LEN];
	uint8_t mac[OSK_AUTHZ_SESSION_KEY_LEN];
} osk_authz_session_keys_t;

osk_result_t osk_authz_cred_add(osk_authz_session_t session, const osk_authz_cred_t *cred,
				const uint8_t *password, size_t pw_len);
osk_result_t osk_authz_cred_remove(osk_authz_session_t session, uint16_t cred_id);
osk_result_t osk_authz_cred_list(uint16_t *ids_out, size_t cap, size_t *count_out);
osk_result_t osk_authz_cred_tries_left(uint16_t cred_id, uint8_t *tries_out);

/* Sitzungsschluessel holen. Braucht OSK_CAP_CRED_USE und einen Bereich,
 * der sich mit dem der Zugangsdaten ueberschneidet.
 *
 * Falsches Passwort zaehlt einen Fehlversuch und sperrt bei `max_fail` -
 * darum wird das Passwort geprueft und nicht nur in die Ableitung gegeben.
 * Gaebe es nur die Ableitung, kaeme bei falschem Passwort ein falscher
 * Schluessel heraus, und der Versuch waere unbegrenzt wiederholbar. */
osk_result_t osk_authz_cred_session(osk_authz_session_t session, uint16_t cred_id,
				    const uint8_t *password, size_t pw_len,
				    const uint8_t host_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				    const uint8_t device_challenge[OSK_AUTHZ_CHALLENGE_LEN],
				    osk_authz_session_keys_t *keys_out);

/* --- Zeitstempel nach RFC 3161 (Teil 4) --------------------------------
 *
 * BEFUND ZUERST, er bestimmt die Schnittstelle: Das Geraet hat keinen Akku,
 * seine Uhr wird beim Einstecken vom Rechner gestellt, und eine falsche
 * Rechneruhr setzt eine falsche Geraetezeit (`docs/architecture.md`). Ein
 * Zeitstempel aus dieser Uhr beweist genau so viel wie der Rechner, gegen
 * den er beweisen soll.
 *
 * DARUM: **Das Geraet ist Antragsteller, nie Zeitstempelstelle.** Die Zeit
 * kommt aus dem Token einer fremden Stelle. Es gibt hier mit Absicht keine
 * Funktion, die aus der Geraeteuhr einen Zeitstempel macht - haette diese
 * Schicht eine, wuerde irgendwann jemand ihr Ergebnis fuer einen Beweis
 * halten.
 *
 * Was das Geraet ohne jede fremde Stelle beweist, ist die REIHENFOLGE:
 * jede Erklaerung traegt einen monotonen Chipzaehler (Strang A), der sich
 * nicht zurueckdrehen laesst. Das ist kein Ersatz fuer eine Uhrzeit,
 * sondern eine andere, schwaecher klingende und staerker belegte Aussage.
 *
 * Ganze Begruendung samt den drei Beweisarten: docs/zeitstempel.md */

#define OSK_TSA_NONCE_LEN  16u
#define OSK_TSA_HASH_LEN   32u /* SHA-256; die Schicht hasht nicht selbst */

/* Was aus einer geprueften Antwort herauskommt. Die GenTime bleibt
 * Zeichenkette, wie sie im Token steht - sie wird nicht in eine Zahl
 * umgerechnet, damit niemand sie mit der Geraetezeit vermischt. */
#define OSK_TSA_GENTIME_MAX 24u

typedef struct {
	uint8_t nonce[OSK_TSA_NONCE_LEN];
	uint8_t hash[OSK_TSA_HASH_LEN];
	bool nonce_sent;
} osk_tsa_request_t;

typedef struct {
	char gen_time[OSK_TSA_GENTIME_MAX + 1]; /* aus dem Token der Stelle */
	bool gen_time_present;
	uint64_t counter;    /* unser monotoner Zaehlerstand */
	uint32_t device_ms;  /* Geraetezeit SEIT DEM EINSTECKEN - keine Weltzeit */
	uint16_t key_id;     /* wer es anstiess */
} osk_tsa_statement_t;

/* Eine Anfrage nach RFC 3161 Abschnitt 2.4.1 in DER bauen.
 * `hash` ist der Hash der Daten; gehasht hat der Aufrufer, wie bei
 * `osk_se050_sign()` und aus demselben Grund. */
osk_result_t osk_tsa_request_build(osk_authz_session_t session, const uint8_t hash[OSK_TSA_HASH_LEN],
				   bool with_nonce, uint8_t *der_out, size_t cap,
				   size_t *len_out, osk_tsa_request_t *req_out);

/* Eine Antwort pruefen - die FORM, nicht die Signatur.
 *
 * Geprueft werden `status`, der Aufbau, die Laengen, und dass die Nonce die
 * der Anfrage ist. Die Signatur des Tokens prueft das Verwaltungsprogramm
 * am Rechner; dafuer braeuchte die Firmware CMS und eine
 * X.509-Kettenpruefung (offener Punkt, docs/zeitstempel.md Abschnitt 6).
 * Die Funktion heisst darum `_parse` und nicht `_verify`: der Name soll
 * beim Lesen des Aufrufs nicht mehr versprechen, als er tut. */
osk_result_t osk_tsa_response_parse(const osk_tsa_request_t *req, const uint8_t *der, size_t len,
				    osk_tsa_statement_t *stmt_out, const uint8_t **token_out,
				    size_t *token_len_out);

/* Das fremde Token an dieses Geraet binden: eine eigene Erklaerung ueber
 * Datenhash, Tokenhash, Zaehlerstand und Identitaet unterschreiben.
 *
 * Ohne diesen Schritt liegen zwei Papiere nebeneinander, die nichts
 * verbindet. Braucht OSK_CAP_TIMESTAMP und OSK_CAP_SIGN. */
osk_result_t osk_tsa_countersign(osk_authz_session_t session, osk_key_id_t signing_key,
				 osk_key_id_t counter_id, const uint8_t hash[OSK_TSA_HASH_LEN],
				 const uint8_t *token, size_t token_len,
				 osk_tsa_statement_t *stmt_out, uint8_t *sig_out, size_t cap,
				 size_t *sig_len_out);

#endif
