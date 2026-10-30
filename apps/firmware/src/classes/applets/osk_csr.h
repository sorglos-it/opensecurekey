/* Zertifikatsanfrage nach PKCS#10 (RFC 2986) - auf dem Geraet gebaut.
 *
 * WARUM SIE HIER ENTSTEHT UND NICHT IM VERWALTUNGSPROGRAMM: eine
 * Zertifikatsanfrage ist mit dem privaten Schluessel UNTERSCHRIEBEN. Das
 * ist ihr ganzer Sinn - sie beweist dem Aussteller, dass der Antragsteller
 * den Schluessel zu dem oeffentlichen Teil wirklich besitzt. Der
 * Schluessel liegt im Sicherheitschip und verlaesst ihn nie. Also muss die
 * Anfrage dort entstehen, wo unterschrieben wird. Das
 * Verwaltungsprogramm darf sie anfordern und hinausgeben; bauen darf es
 * sie nicht, weil es dafuer den Schluessel braeuchte.
 *
 * WAS DRIN STEHT (RFC 2986 Abschnitt 4):
 *   CertificationRequestInfo ::= SEQUENCE {
 *       version        INTEGER (0),
 *       subject        Name,
 *       subjectPKInfo  SubjectPublicKeyInfo,
 *       attributes [0] SET OF Attribute }
 * und darum herum die Unterschrift. Die Erweiterungen (SAN, Key Usage,
 * Extended Key Usage) sind KEIN eigenes Feld: sie reisen als Attribut
 * extensionRequest mit, RFC 2985 Abschnitt 5.4.2. Wer sie als Feld sucht,
 * findet sie nicht.
 *
 * WAS SIE NICHT TUT, und das ist Absicht:
 *   - Sie fragt NICHT nach PIN und nicht nach Tastendruck. Das gehoert
 *     zum Aufrufer: er kennt die Freigaberegel des Steckplatzes, dieses
 *     Modul kennt sie nicht. Anders als bei der Attestierung
 *     (osk_attest.h) ist das hier nicht harmlos - hier unterschreibt der
 *     SCHLUESSEL DES BENUTZERS, nicht ein getrennter
 *     Attestierungsschluessel. Ein Aufruf ohne Freigabe ist ein Fehler im
 *     Aufrufer.
 *   - Sie kennt keine Zertifizierungsstelle, keinen Widerrufsweg und
 *     keine Ablaufwarnung. Das ist eine eigene Aufgabe im
 *     Verwaltungsprogramm (docs/plan.md).
 *   - Sie waehlt nichts selbst. Betreff, Erweiterungen und
 *     Verwendungszweck kommen vom Aufrufer. Ein Modul, das sich den
 *     Betreff ausdenkt, baut Anfragen, die der Aussteller ablehnt.
 *
 * UNTERSCHRIEBEN WIRD IMMER MIT SHA-256. Begruendung: das ist der einzige
 * Hash, den diese Schicht hat (src/classes/ctap/osk_crypto.h), und der
 * Hash wird nach dem Vertrag (osk_se050.h) beim Aufrufer gebildet, nicht
 * im Chip. Die Kennung im Feld signatureAlgorithm sagt genau das, was
 * getan wurde - gemischt wird nichts.
 *
 * STUFE: geschrieben und am PC gegen eine Attrappe gelaufen. Am Geraet
 * ist nichts davon gelaufen. Was die Attrappe ueber den Chip ANNIMMT,
 * steht in docs/csr.md - der Chip ist nicht da, und eine Attrappe prueft
 * die eigene Seite des Vertrags, nicht die andere.
 */
#ifndef OSK_CSR_H
#define OSK_CSR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "osk_spki.h"

/* Laengste Anfrage, die entstehen kann: ein RSA-4096-Schluessel bringt
 * allein 512 Byte Modulus und 512 Byte Unterschrift mit, dazu Betreff,
 * Erweiterungen und die Verpackung. */
#define OSK_CSR_MAX 2048u

/* Dieselbe Anfrage als PEM: je drei Bytes vier Zeichen, dazu ein
 * Zeilenumbruch je 64 Zeichen und die zwei Randzeilen. Abgeleitet und
 * nicht noch einmal hingeschrieben - eine Zahl gehoert an eine Stelle. */
#define OSK_CSR_B64_LEN ((OSK_CSR_MAX + 2u) / 3u * 4u)
#define OSK_CSR_PEM_MAX (OSK_CSR_B64_LEN + OSK_CSR_B64_LEN / 64u + 80u)

#define OSK_CSR_SAN_MAX 4u
#define OSK_CSR_EKU_MAX 4u

/* Welche Art Schluessel im Steckplatz liegt, sagt osk_spki_key_t
 * (osk_spki.h). Hier steht dafuer KEINE zweite Aufzaehlung: zwei Listen
 * derselben drei Schluesselarten laufen auseinander, und die Anfrage und
 * das Zertifikat muessen denselben oeffentlichen Teil schreiben. */

/* Art eines Namens in subjectAltName (RFC 5280 Abschnitt 4.2.1.6). */
typedef enum {
	OSK_CSR_SAN_DNS,   /* dNSName [2] */
	OSK_CSR_SAN_EMAIL, /* rfc822Name [1] */
	OSK_CSR_SAN_URI,   /* uniformResourceIdentifier [6] */
} osk_csr_san_kind_t;

typedef struct {
	osk_csr_san_kind_t kind;
	const char *name;
} osk_csr_san_t;

/* Der Verwendungszweck ist eine Maske aus OSK_KU_* (osk_der.h). Sie steht
 * dort und nicht hier, weil das Attestierungszertifikat dieselben
 * Bitnummern braucht. */

typedef enum {
	OSK_CSR_EKU_SERVER_AUTH,
	OSK_CSR_EKU_CLIENT_AUTH,
	OSK_CSR_EKU_CODE_SIGNING,
	OSK_CSR_EKU_EMAIL_PROTECTION,
} osk_csr_eku_t;

/* Was der Aufrufer angibt. Alles, was NULL oder 0 ist, bleibt weg - mit
 * einer Ausnahme: `cn` ist Pflicht. Eine Anfrage ohne Betreff ist nach RFC
 * 2986 erlaubt, aber dann muss subjectAltName als kritisch markiert sein
 * (RFC 5280 Abschnitt 4.2.1.6), und das ist ein anderer Fall als der hier
 * gebaute. */
typedef struct {
	osk_key_id_t key;	   /* Schluessel im Chip */
	osk_spki_key_t key_type;   /* und welche Art er ist */
	const char *cn;		   /* Betreff: CN (2.5.4.3), Pflicht */
	const char *country;	   /* C (2.5.4.6), zwei Buchstaben, oder NULL */
	const char *org;	   /* O (2.5.4.10) oder NULL */
	const char *org_unit;	   /* OU (2.5.4.11) oder NULL */
	const char *serial_number; /* 2.5.4.5, etwa die Geraetenummer, oder NULL */
	osk_csr_san_t san[OSK_CSR_SAN_MAX];
	uint8_t san_count;
	uint16_t key_usage; /* Maske aus OSK_KU_*, 0 = Erweiterung weglassen */
	osk_csr_eku_t eku[OSK_CSR_EKU_MAX];
	uint8_t eku_count;
} osk_csr_req_t;

/* Nur den unterschriebenen Teil bauen: CertificationRequestInfo.
 *
 * Gebraucht wird das an zwei Stellen, und darum steht es hier und nicht
 * als Nebenwirkung in osk_csr_der():
 *   - osk_csr_der() hasht genau diese Bytes und laesst sie unterschreiben.
 *   - Die Pruefung vergleicht sie mit dem, was in der fertigen Anfrage
 *     steht. Dass die Unterschrift ueber DIESE Bytes geht und nicht ueber
 *     irgendwelche, ist der Kern der Sache.
 *
 * Fehler:
 *   OSK_ERR_ARG         kein `cn`, zu viele Namen, unbekannte Schluesselart
 *   OSK_ERR_NOT_FOUND   im Steckplatz liegt kein Schluessel
 *   OSK_ERR_UNSUPPORTED RSA verlangt, aber dieses Geraet kann kein RSA
 *   OSK_ERR_FORMAT      der Chip liefert einen Schluessel, der nicht passt
 *   OSK_ERR_FULL        `cap` zu klein
 */
osk_result_t osk_csr_info(const osk_csr_req_t *req, uint8_t *out, size_t cap, size_t *len_out);

/* Die vollstaendige Anfrage als DER. Unterschreiben laesst sie der Chip;
 * dieselben Fehler wie oben, dazu OSK_ERR_IO, wenn der Chip nicht
 * unterschreibt. */
osk_result_t osk_csr_der(const osk_csr_req_t *req, uint8_t *out, size_t cap, size_t *len_out);

/* Dieselbe Anfrage als PEM: Base64 in Zeilen von 64 Zeichen, zwischen den
 * zwei Randzeilen, die RFC 7468 Abschnitt 4 vorschreibt. `out` wird mit 0
 * abgeschlossen; `*len_out` ist die Laenge OHNE die abschliessende 0.
 *
 * Getrennt von osk_csr_der(), weil ein Geraet, das die Anfrage als DER
 * ausgibt, sie nicht zweimal bauen soll: PEM ist dieselben Bytes in einer
 * anderen Schreibweise, nicht eine zweite Anfrage. Zwei Anfragen waeren
 * zwei Unterschriften, und damit zwei verschiedene Dateien fuer denselben
 * Schluessel. */
osk_result_t osk_csr_pem(const uint8_t *der, size_t der_len, char *out, size_t cap,
			 size_t *len_out);

/* Karte aus und wieder an: die festen Zwischenpuffer leeren. */
void osk_csr_reset(void);

#endif
