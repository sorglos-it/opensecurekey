/* Einstieg der Firmware auf dem nRF52840.
 *
 * Das ist das Geruest, nicht das Geraet. Es geht beim Start EINMAL durch
 * alle sieben Teile - se050, store, ctap, applets, transport, ui, wallet -
 * und schreibt, was jeder gemeldet hat. Mehr tut es nicht: es gibt noch
 * keine Hardware, und ohne Chip kann der halbe Weg gar nicht gelingen.
 *
 * Wozu es dann gut ist: so wird jeder Teil wirklich uebersetzt UND
 * gebunden. Ein Teil, den niemand aufruft, wirft der Binder weg - und dann
 * sagt die gemessene Groesse des Abbilds nichts aus (docs/build.md).
 *
 * Was jeder Teil hier meldet, ist noch kein Urteil ueber ihn: OSK_ERR_IO
 * vom SE050 ist am Brett ohne Chip das richtige Ergebnis.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "osk_result.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ctap.h"
#include "osk_transport.h"
#include "osk_ui.h"

#include "osk_applets.h"
#include "osk_ui_port.h"
#include "wallet_bip39.h"

LOG_MODULE_REGISTER(osk, LOG_LEVEL_INF);

#ifndef OSK_VERSION
#define OSK_VERSION "unbekannt"
#endif

/* Ein Schritt des Durchlaufs: Name und was er gemeldet hat. */
static void step(const char *name, osk_result_t r)
{
	if (r == OSK_OK) {
		LOG_INF("%s: ok", name);
	} else {
		LOG_WRN("%s: %s", name, osk_result_text(r));
	}
}

/* Der Taster und die Benutzerpruefung haengen am Brett. Bis es da ist,
 * meldet die Pruefung "nicht geprueft" - osk_ui_confirm() verlangt dann
 * den Tastendruck, so wie gedacht. */
static bool not_verified(void *ctx)
{
	ARG_UNUSED(ctx);
	return false;
}

/* --- se050 -------------------------------------------------------------- */
static osk_result_t walk_se050(void)
{
	uint32_t curves = 0;
	uint16_t free_bytes = 0;
	osk_result_t r = osk_se050_open();

	if (r != OSK_OK) {
		return r;
	}
	r = osk_se050_curves(&curves);
	if (r == OSK_OK) {
		LOG_INF("se050: Kurven 0x%08x", curves);
		r = osk_se050_free_memory(&free_bytes);
		if (r == OSK_OK) {
			LOG_INF("se050: frei mindestens %u Byte", free_bytes);
		}
	}
	return r;
}

/* --- store -------------------------------------------------------------- */
static osk_result_t walk_store(void)
{
	uint16_t format = 0;
	size_t count = 0;
	osk_result_t r = osk_store_open();

	if (r != OSK_OK) {
		return r;
	}
	if (osk_store_format_version(&format) == OSK_OK) {
		LOG_INF("store: Format %u", format);
	}
	r = osk_store_count(0, &count);
	if (r == OSK_OK) {
		LOG_INF("store: %u Eintraege im Profil 0", (unsigned)count);
	}
	return r;
}

/* --- ctap --------------------------------------------------------------- */
static osk_result_t walk_ctap(void)
{
	size_t creds = 0;
	osk_result_t r =
	    osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET | OSK_CTAP_EXT_PRF | OSK_CTAP_EXT_LARGE_BLOB |
			  OSK_CTAP_EXT_CRED_PROTECT | OSK_CTAP_EXT_MIN_PIN);

	if (r != OSK_OK) {
		return r;
	}
	r = osk_ctap_cred_count(&creds);
	if (r == OSK_OK) {
		LOG_INF("ctap: %u Dienste", (unsigned)creds);
	}
	return r;
}

/* --- transport ---------------------------------------------------------- */
static osk_result_t walk_transport(void)
{
	/* CTAP laeuft ueber USB-HID und NFC gleich; die Chipkarte kommt
	 * ueber CCID herein. Die Zephyr-Seite dieser Wege ist noch nicht
	 * geschrieben - hier wird nur eingehaengt. */
	osk_result_t r = osk_transport_register(OSK_LINK_USB_HID, osk_ctap_handle);

	if (r != OSK_OK) {
		return r;
	}
	r = osk_transport_register(OSK_LINK_NFC, osk_ctap_handle);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_transport_register(OSK_LINK_USB_CCID, osk_applets_handle);
	if (r != OSK_OK) {
		return r;
	}
	return osk_transport_enable(OSK_LINK_USB_HID, true);
}

/* --- wallet ------------------------------------------------------------- */
static osk_result_t walk_wallet(void)
{
	/* Ein Satz aus 24 Woertern pruefen reicht, um die Geldboerse mit
	 * ihrer Wortliste ins Abbild zu ziehen. Erzeugt wird hier nichts:
	 * ein echter Satz entsteht nur auf Ansage des Nutzers. */
	static const char *const known =
	    "legal winner thank year wave sausage worth useful legal winner thank year "
	    "wave sausage worth useful legal will";

	return osk_bip39_check(known);
}

int main(void)
{
	LOG_INF("opensecurekey %s - Durchlauf durch die sieben Teile", OSK_VERSION);

	step("se050", walk_se050());
	step("store", walk_store());
	step("ctap", walk_ctap());
	step("applets", osk_applets_init());
	step("transport", walk_transport());
	step("ui", osk_ui_port_nrf_start(not_verified));
	step("wallet", walk_wallet());

	osk_ui_status("status.ready");
	LOG_INF("Durchlauf beendet");

	/* Noch keine Ereignisschleife: die kommt mit den Transporten. */
	while (1) {
		k_sleep(K_SECONDS(10));
	}
	return 0;
}
