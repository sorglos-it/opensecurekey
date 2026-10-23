/* Flash fuer das Geraet: der QSPI-Baustein ueber die Flash-Schnittstelle von
 * Zephyr. Diese Datei rechnet nur Adressen um und prueft die Vorbedingungen,
 * die osk_flash.h zusagt.
 *
 * STAND: am PC nicht uebersetzt (dort laeuft die Attrappe aus
 * apps/firmware/test/store/fake_flash.c), am Geraet nicht gelaufen - es ist
 * noch keine Hardware da. Was zu pruefen ist, sobald sie da ist:
 *  - dass die Partition "vault_partition" im Geraetebaum liegt und mindestens
 *    OSK_VAULT_SIZE gross ist
 *  - dass der Baustein wirklich 4096 Byte Sektoren und 256 Byte Seiten hat;
 *    sonst meldet osk_store_open() OSK_ERR_UNSUPPORTED
 *  - wie lange ein Sektor zum Loeschen braucht (davon haengt ab, wie lange
 *    eine Aenderung dauert) */

#include "osk_flash.h"

#ifdef OSK_TARGET_DEVICE

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include "osk_vault_format.h"

#define VAULT_NODE vault_partition
#define VAULT_OFF FIXED_PARTITION_OFFSET(VAULT_NODE)
#define VAULT_SIZE FIXED_PARTITION_SIZE(VAULT_NODE)
#define VAULT_DEV FIXED_PARTITION_DEVICE(VAULT_NODE)

static const struct device *dev(void)
{
	static const struct device *d;

	if (d == NULL) {
		d = VAULT_DEV;
	}
	return d;
}

static osk_result_t check(uint32_t addr, size_t len)
{
	if (!device_is_ready(dev())) {
		return OSK_ERR_IO;
	}
	if ((uint64_t)addr + len > VAULT_SIZE) {
		return OSK_ERR_ARG;
	}
	return OSK_OK;
}

osk_result_t osk_flash_geometry(osk_flash_geometry_t *out)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!device_is_ready(dev())) {
		return OSK_ERR_IO;
	}
	const struct flash_parameters *p = flash_get_parameters(dev());

	if (p == NULL) {
		return OSK_ERR_IO;
	}
	out->size = VAULT_SIZE;
	out->page_size = p->write_block_size < OSK_PAGE ? OSK_PAGE : p->write_block_size;
	/* Die Sektorgroesse steht nicht in den Parametern; sie kommt aus dem
	 * Geraetebaum des Bausteins. Stimmt sie nicht, sagt der Tresor beim
	 * Oeffnen OSK_ERR_UNSUPPORTED - und zwar bevor er etwas schreibt. */
	out->sector_size = OSK_SECTOR;
	return OSK_OK;
}

osk_result_t osk_flash_read(uint32_t addr, void *buf, size_t len)
{
	osk_result_t r = check(addr, len);

	if (r != OSK_OK) {
		return r;
	}
	if (buf == NULL) {
		return OSK_ERR_ARG;
	}
	return (flash_read(dev(), VAULT_OFF + addr, buf, len) == 0) ? OSK_OK : OSK_ERR_IO;
}

osk_result_t osk_flash_program(uint32_t addr, const void *buf, size_t len)
{
	osk_result_t r = check(addr, len);

	if (r != OSK_OK) {
		return r;
	}
	if (buf == NULL || (addr % OSK_PAGE) != 0u || (len % OSK_PAGE) != 0u) {
		return OSK_ERR_ARG;
	}
	return (flash_write(dev(), VAULT_OFF + addr, buf, len) == 0) ? OSK_OK : OSK_ERR_IO;
}

osk_result_t osk_flash_erase(uint32_t addr, size_t len)
{
	osk_result_t r = check(addr, len);

	if (r != OSK_OK) {
		return r;
	}
	if ((addr % OSK_SECTOR) != 0u || (len % OSK_SECTOR) != 0u) {
		return OSK_ERR_ARG;
	}
	return (flash_erase(dev(), VAULT_OFF + addr, len) == 0) ? OSK_OK : OSK_ERR_IO;
}

osk_result_t osk_flash_sync(void)
{
	/* Der QSPI-Treiber von Zephyr kehrt erst zurueck, wenn der Baustein
	 * fertig ist - es gibt keinen Schreibpuffer, der noch zu leeren waere.
	 * Die Funktion bleibt trotzdem, weil der Tresor sie an den richtigen
	 * Stellen ruft und ein anderer Baustein sie brauchen koennte. */
	return device_is_ready(dev()) ? OSK_OK : OSK_ERR_IO;
}

#endif /* OSK_TARGET_DEVICE */
