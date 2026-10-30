/* DER schreiben - die Stuecke, die mehr als eine Kartenanwendung braucht.
 *
 * WARUM ES DIESE DATEI GIBT: der Aufbau nach X.690 stand bis zum
 * 30.10.2026 vollstaendig in osk_attest.c, weil ihn nur die Attestierung
 * brauchte. Mit der Zertifikatsanfrage (osk_csr.h) braucht ihn ein
 * zweiter Teil, und zwar Byte fuer Byte denselben: dieselben
 * Objektkennungen, dieselbe Regel fuer die fuehrende Null im INTEGER,
 * dieselben Namensteile. Zweimal hingeschrieben waere eine der beiden
 * Fassungen irgendwann anders - und der Fehler fiele nicht hier auf,
 * sondern beim Gegenueber, das das Zertifikat prueft.
 *
 * Geschrieben wird nach ITU-T X.690 (DER), RFC 5280 (X.509), RFC 5480
 * (elliptische Kurven) und RFC 8017 Anhang A.1 (RSA). Nach der
 * Beschreibung, nicht nach einer vorhandenen Umsetzung.
 *
 * Diese Ebene weiss nichts von Zertifikaten und nichts von Anfragen. Sie
 * schreibt Tags, Laengen und die drei Dinge, bei denen DER eine Regel hat,
 * die man beim ersten Mal falsch macht: das Vorzeichen eines INTEGER, die
 * ungenutzten Bits eines BIT STRING und die Wahl des Zeichenkettentyps.
 *
 * Gebaut wird immer in einen osk_resp_t (osk_apdu.h). Der merkt sich einen
 * Ueberlauf, und deshalb hat hier keine Funktion ein Ergebnis: geprueft
 * wird einmal am Ende, an `overflow`.
 */
#ifndef OSK_DER_H
#define OSK_DER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_apdu.h"

/* --- Tags (X.690 Abschnitt 8) ------------------------------------------- */

#define OSK_DER_BOOLEAN 0x01u
#define OSK_DER_INTEGER 0x02u
#define OSK_DER_BITSTRING 0x03u
#define OSK_DER_OCTETSTRING 0x04u
#define OSK_DER_NULL 0x05u
#define OSK_DER_OID 0x06u
#define OSK_DER_UTF8 0x0cu
#define OSK_DER_PRINTABLE 0x13u
#define OSK_DER_IA5 0x16u
#define OSK_DER_UTCTIME 0x17u
#define OSK_DER_GENTIME 0x18u
#define OSK_DER_SEQUENCE 0x30u
#define OSK_DER_SET 0x31u
#define OSK_DER_CTX0 0xa0u
#define OSK_DER_CTX3 0xa3u

/* --- Objektkennungen ---------------------------------------------------- */

/* Eine Kennung mit ihrer Laenge. Zusammen, weil die zwei immer zusammen
 * gebraucht werden und ein `sizeof` am Aufrufort schon einmal zu einer
 * Kennung mit falscher Laenge gefuehrt hat. Nur der Inhalt, ohne Tag und
 * Laengenfeld. */
typedef struct {
	const uint8_t *v;
	uint8_t len;
} osk_oid_t;

/* Schluesselarten und Unterschriftsverfahren. */
extern const osk_oid_t osk_oid_ec_public;    /* 1.2.840.10045.2.1 */
extern const osk_oid_t osk_oid_ecdsa_sha256; /* 1.2.840.10045.4.3.2 */
extern const osk_oid_t osk_oid_rsa;	     /* 1.2.840.113549.1.1.1 */
extern const osk_oid_t osk_oid_rsa_sha256;   /* 1.2.840.113549.1.1.11 */

/* Die zwei Kurven. Die Zahlen selbst stehen in osk_card.c und werden hier
 * nur verpackt - OpenPGP braucht sie ebenfalls, und dieselbe Zahl gehoert
 * an eine Stelle. */
extern const osk_oid_t osk_oid_curve_p256;
extern const osk_oid_t osk_oid_curve_p384;

/* Namensteile (X.520, Kennungen aus 2.5.4). */
extern const osk_oid_t osk_oid_cn;	      /* 2.5.4.3 */
extern const osk_oid_t osk_oid_country;	      /* 2.5.4.6 */
extern const osk_oid_t osk_oid_org;	      /* 2.5.4.10 */
extern const osk_oid_t osk_oid_ou;	      /* 2.5.4.11 */
extern const osk_oid_t osk_oid_serial_number; /* 2.5.4.5 */

/* Erweiterungen (RFC 5280 Abschnitt 4.2, Kennungen aus 2.5.29). */
extern const osk_oid_t osk_oid_key_usage;	  /* 2.5.29.15 */
extern const osk_oid_t osk_oid_san;		  /* 2.5.29.17 */
extern const osk_oid_t osk_oid_basic_constraints; /* 2.5.29.19 */
extern const osk_oid_t osk_oid_ext_key_usage;	  /* 2.5.29.37 */

/* Erweiterter Verwendungszweck (RFC 5280 Abschnitt 4.2.1.12). */
extern const osk_oid_t osk_oid_eku_server; /* 1.3.6.1.5.5.7.3.1 */
extern const osk_oid_t osk_oid_eku_client; /* 1.3.6.1.5.5.7.3.2 */
extern const osk_oid_t osk_oid_eku_code;   /* 1.3.6.1.5.5.7.3.3 */
extern const osk_oid_t osk_oid_eku_email;  /* 1.3.6.1.5.5.7.3.4 */

/* Das Attribut, mit dem Erweiterungen in eine Anfrage kommen
 * (RFC 2985 Abschnitt 5.4.2): 1.2.840.113549.1.9.14. */
extern const osk_oid_t osk_oid_ext_request;

/* --- Verwendungszweck --------------------------------------------------- */

/* Die Bitnummern sind die der NORM (RFC 5280 Abschnitt 4.2.1.3) und nicht
 * die eines Rechnerwortes: Bit 0 ist digitalSignature und steht als
 * HOECHSTWERTIGES Bit im ersten Byte. Sie stehen hier, weil zwei Teile sie
 * brauchen - das Attestierungszertifikat und die Zertifikatsanfrage. */
#define OSK_KU_DIGITAL_SIGNATURE 0x0001u
#define OSK_KU_NON_REPUDIATION 0x0002u
#define OSK_KU_KEY_ENCIPHERMENT 0x0004u
#define OSK_KU_DATA_ENCIPHERMENT 0x0008u
#define OSK_KU_KEY_AGREEMENT 0x0010u
#define OSK_KU_KEY_CERT_SIGN 0x0020u
#define OSK_KU_CRL_SIGN 0x0040u

/* --- Schreiben ---------------------------------------------------------- */

void osk_der_oid(osk_resp_t *r, const osk_oid_t *oid);

/* BOOLEAN TRUE. DER kennt dafuer genau ein Byte, 0xFF - jedes andere
 * Byte ungleich 0 waere BER und nicht DER. */
void osk_der_true(osk_resp_t *r);

/* AlgorithmIdentifier: SEQUENCE { OID, Parameter }. Der Parameter ist eine
 * Kurvenkennung, ein ausdruecklicher NULL-Wert oder gar nichts - alle drei
 * Faelle kommen vor, und welcher gilt, schreibt die Norm je Verfahren vor
 * (RFC 5480 Abschnitt 2.1.1 gegen RFC 8017 Anhang A.1). */
void osk_der_alg(osk_resp_t *r, const osk_oid_t *oid, const osk_oid_t *par, bool par_null);

/* INTEGER aus einer vorzeichenlosen Zahl, hoechstwertiges Byte zuerst:
 * fuehrende Nullen weg, und eine Null davor, wenn das oberste Bit sonst als
 * Vorzeichen gelesen wuerde. Genau die Stelle, an der ein falsches
 * Zertifikat entsteht, wenn man sie sich schenkt. */
void osk_der_uint(osk_resp_t *r, const uint8_t *v, size_t len);

/* BIT STRING ohne ungenutzte Bits am Ende. */
void osk_der_bits(osk_resp_t *r, const uint8_t *v, size_t len);

/* Eine Zeichenkette als PrintableString, wenn jedes Zeichen dort erlaubt
 * ist (X.680 Tabelle 10: Buchstaben, Ziffern, Leerzeichen und
 * '()+,-./:=?), sonst als UTF8String.
 *
 * Das ist keine Feinheit: ein '@' in einer E-Mail-Adresse und ein Umlaut in
 * einem Namen sind in einem PrintableString nicht erlaubt. Ein Pruefer, der
 * das streng nimmt, lehnt die Anfrage ab - und die Fehlermeldung sagt dann
 * irgendetwas ueber den Schluessel. */
void osk_der_string(osk_resp_t *r, const char *text);

/* Ein Namensteil: SET { SEQUENCE { OID, Zeichenkette } }. */
void osk_der_rdn(osk_resp_t *r, const osk_oid_t *oid, const char *text);

/* Eine Erweiterung: SEQUENCE { OID, [critical], OCTET STRING { Inhalt } }.
 * Der Inhalt wird zwischen begin und end geschrieben - er ist je
 * Erweiterung anders, und ein Zwischenpuffer dafuer waere Speicher, den
 * das Geraet nicht hat. */
void osk_der_ext_begin(osk_resp_t *r, const osk_oid_t *oid, bool critical, osk_tlv_mark_t *outer,
		       osk_tlv_mark_t *inner);
void osk_der_ext_end(osk_resp_t *r, const osk_tlv_mark_t *outer, const osk_tlv_mark_t *inner);

/* KeyUsage als BIT STRING, aus einer Maske mit Bit 0 (digitalSignature)
 * als hoechstwertigem Bit des ersten Bytes - so zaehlt RFC 5280
 * Abschnitt 4.2.1.3, und nicht wie ein Rechner zaehlt. Ungenutzte Bits am
 * Ende werden gezaehlt und gemeldet, wie DER es verlangt. */
void osk_der_key_usage_bits(osk_resp_t *r, uint16_t mask);

#endif
