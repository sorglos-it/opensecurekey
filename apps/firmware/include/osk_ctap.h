/* FIDO2/CTAP2-Kern - transportunabhaengig. USB und NFC reichen dieselben
 * Bytes herein; die Freigabe laeuft ueber osk_ui, die Schluessel ueber den
 * SE050, abgeleitet statt einzeln gespeichert.
 *
 * Umsetzung: src/classes/ctap/ */
#ifndef OSK_CTAP_H
#define OSK_CTAP_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_transport.h"

/* Erweiterungen, die fremde Programme von uns erwarten. */
#define OSK_CTAP_EXT_HMAC_SECRET  (1u << 0)  /* Bitwarden, BitLocker, LUKS */
#define OSK_CTAP_EXT_PRF          (1u << 1)
#define OSK_CTAP_EXT_LARGE_BLOB   (1u << 2)
#define OSK_CTAP_EXT_CRED_PROTECT (1u << 3)
#define OSK_CTAP_EXT_MIN_PIN      (1u << 4)

osk_result_t osk_ctap_init(uint32_t extensions);

/* Eine vollstaendige CTAP-Nachricht bearbeiten. Der Transport hat den
 * Rahmen bereits entfernt. */
osk_result_t osk_ctap_handle(const osk_request_t *req, osk_response_t *res);

/* CTAP1/U2F fuer alte Dienste, gleiche Schluesselableitung. */
osk_result_t osk_ctap_handle_u2f(const osk_request_t *req, osk_response_t *res);

/* Zaehler fuer das Geraeteprotokoll und die Dienste-Liste im Programm. */
osk_result_t osk_ctap_cred_count(size_t *count_out);

#endif
