/* Zahlenwerte des SE050-IoT-Applets.
 *
 * Alle aus AN12413 "SE050 APDU Specification" Rev. 2.12 vom 24.03.2021.
 * Tabelle und Abschnitt stehen je Gruppe dabei, damit jeder Wert
 * nachschlagbar bleibt. Anders als se050_t1_wire.h ist hier NICHTS geraten -
 * dieses Dokument ist frei herunterladbar und vollstaendig.
 *
 * Aufgenommen ist nur, was dieser Treiber wirklich sendet. Der Baustein
 * kann deutlich mehr (DESFire, TLS-PRF, I2C-Herr); das steht hier
 * absichtlich nicht - ein unbenutzter Wert wird nicht gegengelesen.
 * RSA und die monotonen Zaehler sind am 28.10.2026 dazugekommen, weil der
 * Treiber sie jetzt sendet (Strang A).
 *
 * Achtung beim Nachpruefen: die Tabellen in AN12413 haben zwei Spalten,
 * Name und Wert, die in der PDF-Textausgabe getrennt untereinander landen.
 * Wer sie mit "pdftotext -layout" liest, bekommt sie um eine Zeile
 * verschoben. Richtig ist die Leserichtung ohne -layout: dort stehen erst
 * alle Namen, dann alle Werte, und beide Listen sind gleich lang. */
#ifndef SE050_CMD_H
#define SE050_CMD_H

/* Kennung des IoT-Applets (Abschnitt 2.3). */
#define SE050_APPLET_AID                                 \
	{0xA0, 0x00, 0x00, 0x03, 0x96, 0x54, 0x53, 0x00, \
	 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00}
#define SE050_APPLET_AID_LEN 16u

/* Groesste Nutzlast ohne gesicherte Uebertragung (Tabelle 16).
 * Mit SCP03 ist weniger drin - Auffuellung und MAC gehen ab. */
#define SE050_MAX_PAYLOAD 889u
#define SE050_APDU_BUF 894u

/* CLA (Abschnitt 4.1.1.1): 0x80 offen, 0x84 gesichert. */
#define SE050_CLA 0x80u
#define SE050_CLA_SECURE 0x84u

/* Befehlsbyte INS (Tabelle 19) und seine Zusatzbits (Tabelle 18). */
#define INS_WRITE 0x01u
#define INS_READ 0x02u
#define INS_CRYPTO 0x03u
#define INS_MGMT 0x04u
#define INS_PROCESS 0x05u
#define INS_TRANSIENT 0x80u
#define INS_AUTH_OBJECT 0x40u
#define INS_ATTEST 0x20u

/* P1: Schluesselart (Tabelle 21) und Objektart (Tabelle 22). */
#define P1_KEY_PAIR 0x60u
#define P1_PRIVATE 0x40u
#define P1_PUBLIC 0x20u
#define P1_DEFAULT 0x00u
#define P1_EC 0x01u
#define P1_RSA 0x02u
#define P1_AES 0x03u
#define P1_HMAC 0x05u
#define P1_USERID 0x07u
#define P1_COUNTER 0x08u
#define P1_CURVE 0x0Bu
#define P1_SIGNATURE 0x0Cu
#define P1_MAC 0x0Du

/* P2 (Tabelle 23) - nur die benutzten. */
#define P2_DEFAULT 0x00u
#define P2_SIZE 0x07u
#define P2_SIGN 0x09u
#define P2_DH 0x0Fu
#define P2_SESSION_CREATE 0x1Bu
#define P2_SESSION_CLOSE 0x1Cu
#define P2_VERSION 0x20u
#define P2_MEMORY 0x22u
#define P2_LIST 0x25u
#define P2_TYPE 0x26u
#define P2_DELETE_OBJECT 0x28u
#define P2_DECRYPT_ONESHOT 0x37u
#define P2_SESSION_USERID 0x2Cu
#define P2_HKDF 0x2Du
#define P2_MAC 0x32u
#define P2_CURVE_LIST 0x34u
#define P2_ATTRIBUTES 0x3Bu
#define P2_GENERATE_ONESHOT 0x45u
#define P2_RANDOM 0x49u
#define P2_RAW 0x4Fu
#define P2_SCP 0x52u

/* TLV-Kennungen (Tabelle 27). */
#define TAG_SESSION_ID 0x10u
#define TAG_POLICY 0x11u
#define TAG_MAX_ATTEMPTS 0x12u
#define TAG_1 0x41u
#define TAG_2 0x42u
#define TAG_3 0x43u
#define TAG_4 0x44u
#define TAG_5 0x45u
#define TAG_6 0x46u
#define TAG_7 0x47u

/* Kurven (Tabelle 37). Kurven unter 0x40 sind Weierstrass-Kurven;
 * nur die listet ReadECCurveList auf (Abschnitt 4.8.4). */
#define EC_NIST_P256 0x03u
#define EC_NIST_P384 0x04u
#define EC_SECP256K1 0x10u
#define EC_ED25519 0x40u
#define EC_MONT_DH_25519 0x41u

/* Signaturverfahren: Weierstrass (Tabelle 28), Edwards (Tabelle 29). */
#define SIG_ECDSA_SHA_256 0x21u
#define SIG_ECDSA_SHA_384 0x22u
#define SIG_ECDSA_SHA_512 0x26u
#define SIG_ED25519PURE 0xA3u

/* Hashverfahren (Tabelle 35) und MAC-Verfahren (Tabelle 36). */
#define DIGEST_SHA256 0x04u
#define DIGEST_SHA384 0x05u
#define DIGEST_SHA512 0x06u
#define HMAC_SHA256 0x19u

/* --- RSA ---------------------------------------------------------------
 *
 * Signaturverfahren (Tabelle 31). Die Auffuellung steckt im Namen und wird
 * nie aus der Hashlaenge erraten: PKCS#1 v1.5 und PSS sind nicht
 * gegeneinander austauschbar. */
#define RSA_SHA256_PKCS1_PSS 0x2Cu
#define RSA_SHA384_PKCS1_PSS 0x2Du
#define RSA_SHA512_PKCS1_PSS 0x2Eu
#define RSA_SHA_256_PKCS1 0x28u
#define RSA_SHA_384_PKCS1 0x29u
#define RSA_SHA_512_PKCS1 0x2Au

/* Verschluesselungsverfahren (Tabelle 32).
 *
 * BEFUND: RSA_PKCS1_OAEP gibt es nur in EINER Fassung, "using SHA1 as
 * digest". Ein OAEP mit SHA-256 kennt AN12413 nicht, und osk_hash_t im
 * Vertrag kennt kein SHA-1 - darum lehnt osk_se050_rsa_decrypt() OAEP ab
 * statt eine Auffuellung zu nehmen, die der Aufrufer nicht bestellt hat.
 * Siehe docs/se050.md. */
#define RSA_PKCS1 0x0Au
#define RSA_NO_PAD 0x0Cu
#define RSA_PKCS1_OAEP 0x0Fu

/* Schluessellaengen (Tabelle 33). Der Wert IST die Zahl der Bits; das
 * Feld ist zwei Byte lang. "Other values are not supported"
 * (Abschnitt 4.7.1.2). */
#define RSA_2048 2048u
#define RSA_3072 3072u
#define RSA_4096 4096u

/* Schluesselteile (Tabelle 34). Mehr als diese zwei liest der Treiber
 * nicht - die privaten Teile gibt der Baustein ohnehin nicht heraus. */
#define RSA_COMP_MOD 0x00u
#define RSA_COMP_PUB_EXP 0x01u

/* Merkmalbits fuer RSA (Tabelle 5 und Tabelle 40, die beide dieselben
 * Werte nennen). PLAIN heisst "rohe RSA-Schluessel anlegen", CRT heisst
 * "Schluessel im CRT-Format anlegen" - zwei verschiedene Formate, nicht
 * zwei Namen fuer dasselbe.
 *
 * Gegenprobe, die den Spaltenversatz der PDF-Textausgabe ausschliesst:
 * Tabelle 40 nennt CONFIG_RSA_ALL = 0x0060, und 0x0020 | 0x0040 ist genau
 * das. */
#define CONFIG_RSA_PLAIN 0x0020u
#define CONFIG_RSA_CRT 0x0040u

/* Statuswort (Tabelle 15). */
#define SW_NO_ERROR 0x9000u
#define SW_SECURITY_STATUS 0x6982u
#define SW_DATA_INVALID 0x6984u
#define SW_CONDITIONS_NOT_SATISFIED 0x6985u
#define SW_COMMAND_NOT_ALLOWED 0x6986u
#define SW_WRONG_DATA 0x6A80u

/* Speicherart (Tabelle 25). Fuer die Frage "wie viele Schluessel passen
 * noch" zaehlt nur der dauerhafte Speicher. */
#define MEM_PERSISTENT 0x01u
#define MEM_TRANSIENT_RESET 0x02u
#define MEM_TRANSIENT_DESELECT 0x03u

/* Objekttyp (Tabelle 24) - nur was gelesen wird. */
#define TYPE_EC_KEY_PAIR 0x01u
#define TYPE_RSA_KEY_PAIR 0x04u
#define TYPE_RSA_KEY_PAIR_CRT 0x05u
#define TYPE_AES_KEY 0x09u
#define TYPE_USERID 0x0Cu
#define TYPE_COUNTER 0x0Du
#define TYPE_HMAC_KEY 0x11u

/* PlatformSCPRequest (Tabelle 48). Der Baustein kommt mit
 * SE050_SCP_NOT_REQUIRED aus dem Werk - siehe docs/se050.md. */
#define SE050_SCP_REQUIRED 0x01u
#define SE050_SCP_NOT_REQUIRED 0x02u

/* SetIndicator (Tabelle 46): so meldet ReadECCurveList je Kurve,
 * ob sie im Baustein angelegt ist. */
#define SE050_NOT_SET 0x01u
#define SE050_SET 0x02u

#endif
