/* Was der Datentraeger an Krypto braucht und der Tresor nicht hat.
 *
 * Der Tresor rechnet AES-256-GCM (osk_crypto.h). Der Datentraeger darf das
 * nicht: der Beschleuniger des nRF52840 kann laut Treibertabelle weder
 * AES-256 noch GCM, und beim Datentraeger geht es um Megabyte am Stueck statt
 * um 8-KiB-Haeppchen. Also ChaCha20-Poly1305 - das einzige 256-Bit-Verfahren,
 * das dort beschleunigt gelistet ist. Begruendung vollstaendig in
 * docs/datentraeger-konzept.md, Abschnitt "Die Verschluesselung".
 *
 * Eigene Krypto wird NICHT geschrieben. Gerechnet wird:
 *  - auf dem Geraet in osk_volume_crypto_psa.c ueber PSA Crypto,
 *  - im Test in apps/firmware/test/store/fake_vcrypto.c - eine
 *    nachvollziehbare Attrappe ohne jede Sicherheit. Sie prueft den Ablauf
 *    des Datentraegers, nicht die Verschluesselung.
 *
 * Alles Uebrige - SHA-256, HKDF, Zufall, Wischen, Vergleich ohne
 * Laufzeitunterschied - kommt aus osk_crypto.h und wird hier nicht
 * wiederholt. */
#ifndef OSK_VOLUME_CRYPTO_H
#define OSK_VOLUME_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_crypto.h"

/* Poly1305-Stempel. Gleich lang wie der GCM-Stempel des Tresors, aber ein
 * anderes Verfahren - darum ein eigener Name. */
#define OSK_VOL_AEAD_TAG_LEN 16u
#define OSK_VOL_AEAD_NONCE_LEN 12u

/* Ed25519: der oeffentliche Teil ist 32 Byte, die Signatur 64. */
#define OSK_VOL_SIG_LEN 64u
#define OSK_VOL_PUB_LEN 32u

/* ChaCha20-Poly1305. ct_out braucht plain_len + OSK_VOL_AEAD_TAG_LEN Platz;
 * der Stempel steht hinten. aad darf NULL sein.
 *
 * Eine Nonce zweimal mit demselben Schluessel bei anderem Klartext gibt nicht
 * eine Nachricht her, sondern den Stempelschluessel. Wer hier aufruft, muss
 * die Nonce aus Schreibzaehler, Blocknummer und Zweck bilden - siehe
 * volume_block.c. */
osk_result_t osk_vol_aead_seal(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			       size_t plain_len, uint8_t *ct_out, size_t ct_cap,
			       size_t *ct_len_out);

/* Bei falschem Stempel: OSK_ERR_FORMAT, und plain_out bleibt unbrauchbar -
 * es gibt nie halb entschluesselte Daten. */
osk_result_t osk_vol_aead_open(const osk_key_t *key, const uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN],
			       const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			       uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out);

/* Eine Ed25519-Signatur gegen einen FREMDEN oeffentlichen Schluessel pruefen.
 *
 * Warum nicht im Chip: osk_se050.h kann signieren (osk_se050_sign) und mit
 * eigenen Schluesseln rechnen, aber eine Signatur eines anderen Sticks zu
 * pruefen, ist eine Rechnung auf oeffentlichen Daten - kein Geheimnis
 * beteiligt. Sie gehoert darum auf den Rechenkern und nicht ueber den I2C.
 *
 * ZU BESTAETIGEN: dass die PSA-Umsetzung des nRF Connect SDK Ed25519-Pruefung
 * auf dem nRF52840 anbietet. Am Geraet ungeprueft, es gibt kein Brett. */
osk_result_t osk_vol_sig_verify(const uint8_t pub[OSK_VOL_PUB_LEN], const uint8_t *msg,
				size_t msg_len, const uint8_t sig[OSK_VOL_SIG_LEN]);

#endif
