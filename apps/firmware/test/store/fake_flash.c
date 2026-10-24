/* Flash-Attrappe im RAM - siehe fake_flash.h. */

#include <stdlib.h>
#include <string.h>
#include "fake_flash.h"
#include "osk_flash.h"
#include "osk_vault_format.h"

#define BANKS 2
#define SECTORS (OSK_VAULT_SIZE / OSK_SECTOR)

struct bank {
	uint8_t *mem;
	uint32_t erases[SECTORS];
	/* Lesezugriffe je Sektor. Damit laesst sich beweisen, dass ein Weg
	 * einen bestimmten Bereich NICHT anfasst - etwa den Datenplatz eines
	 * Geheimnisses. Eine Zusicherung, die niemand nachzaehlt, ist nur eine
	 * Behauptung. */
	uint32_t reads[SECTORS];
};

static struct bank g_bank[BANKS];
static int g_cur;
static uint32_t g_writes;
static uint32_t g_fail_after;
static bool g_dead;
static bool g_torn; /* der eine zerrissene Vorgang ist schon passiert */

void fake_flash_init(void)
{
	for (int i = 0; i < BANKS; i++) {
		if (g_bank[i].mem == NULL) {
			g_bank[i].mem = malloc(OSK_VAULT_SIZE);
		}
		memset(g_bank[i].mem, 0xFF, OSK_VAULT_SIZE);
		memset(g_bank[i].erases, 0, sizeof(g_bank[i].erases));
		memset(g_bank[i].reads, 0, sizeof(g_bank[i].reads));
	}
	g_cur = 0;
	g_writes = 0;
	g_fail_after = 0;
	g_dead = false;
	g_torn = false;
}

void fake_flash_free(void)
{
	for (int i = 0; i < BANKS; i++) {
		free(g_bank[i].mem);
		g_bank[i].mem = NULL;
	}
}

void fake_flash_select(int bank)
{
	if (bank >= 0 && bank < BANKS) {
		g_cur = bank;
	}
}

int fake_flash_bank(void)
{
	return g_cur;
}

void fake_flash_wipe(void)
{
	memset(g_bank[g_cur].mem, 0xFF, OSK_VAULT_SIZE);
	memset(g_bank[g_cur].erases, 0, sizeof(g_bank[g_cur].erases));
	memset(g_bank[g_cur].reads, 0, sizeof(g_bank[g_cur].reads));
	g_writes = 0;
	g_dead = false;
	g_torn = false;
}

void fake_flash_fail_after(uint32_t n)
{
	g_fail_after = n;
	g_dead = false;
	g_torn = false;
}

uint32_t fake_flash_writes(void)
{
	return g_writes;
}

void fake_flash_count_reset(void)
{
	g_writes = 0;
}

bool fake_flash_is_dead(void)
{
	return g_dead;
}

void fake_flash_revive(void)
{
	g_dead = false;
	g_torn = false;
	g_fail_after = 0;
}

uint32_t fake_flash_erase_count(uint32_t addr)
{
	uint32_t s = addr / OSK_SECTOR;

	return (s < SECTORS) ? g_bank[g_cur].erases[s] : 0u;
}

uint32_t fake_flash_erase_total(void)
{
	uint32_t sum = 0;

	for (uint32_t s = 0; s < SECTORS; s++) {
		sum += g_bank[g_cur].erases[s];
	}
	return sum;
}

uint32_t fake_flash_erase_max(uint32_t area_off, uint32_t area_len)
{
	uint32_t max = 0;

	for (uint32_t a = area_off; a < area_off + area_len; a += OSK_SECTOR) {
		uint32_t c = fake_flash_erase_count(a);

		if (c > max) {
			max = c;
		}
	}
	return max;
}

uint32_t fake_flash_sectors_touched(uint32_t area_off, uint32_t area_len)
{
	uint32_t n = 0;

	for (uint32_t a = area_off; a < area_off + area_len; a += OSK_SECTOR) {
		if (fake_flash_erase_count(a) > 0u) {
			n++;
		}
	}
	return n;
}

void fake_flash_read_reset(void)
{
	memset(g_bank[g_cur].reads, 0, sizeof(g_bank[g_cur].reads));
}

uint32_t fake_flash_reads_in(uint32_t area_off, uint32_t area_len)
{
	uint32_t n = 0;

	for (uint32_t a = area_off; a < area_off + area_len; a += OSK_SECTOR) {
		uint32_t s = a / OSK_SECTOR;

		if (s < SECTORS) {
			n += g_bank[g_cur].reads[s];
		}
	}
	return n;
}

/* ---- die Schnittstelle, die der Tresor benutzt ----------------------- */

osk_result_t osk_flash_geometry(osk_flash_geometry_t *out)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	out->size = OSK_VAULT_SIZE;
	out->sector_size = OSK_SECTOR;
	out->page_size = OSK_PAGE;
	return OSK_OK;
}

osk_result_t osk_flash_read(uint32_t addr, void *buf, size_t len)
{
	if (g_dead) {
		return OSK_ERR_IO;
	}
	if (buf == NULL || (uint64_t)addr + len > OSK_VAULT_SIZE) {
		return OSK_ERR_ARG;
	}
	/* Jeder gelesene Sektor wird gezaehlt, auch wenn nur ein Byte daraus
	 * geholt wird: gefragt ist, OB ein Bereich angefasst wurde. */
	for (uint64_t a = addr; a < (uint64_t)addr + len; a = (a / OSK_SECTOR + 1u) * OSK_SECTOR) {
		uint32_t s = (uint32_t)(a / OSK_SECTOR);

		if (s < SECTORS) {
			g_bank[g_cur].reads[s]++;
		}
	}
	memcpy(buf, g_bank[g_cur].mem + addr, len);
	return OSK_OK;
}

/* Nimmt der Strom mitten im Schreiben ab, steht die halbe Seite im Flash.
 * Genau das soll der Tresor ueberleben. */
static bool spend_write(void)
{
	if (g_dead) {
		return false;
	}
	if (g_fail_after != 0u && g_writes >= g_fail_after) {
		g_dead = true;
		return false;
	}
	g_writes++;
	return true;
}

osk_result_t osk_flash_program(uint32_t addr, const void *buf, size_t len)
{
	if (buf == NULL || (uint64_t)addr + len > OSK_VAULT_SIZE || (addr % OSK_PAGE) != 0u ||
	    (len % OSK_PAGE) != 0u) {
		return OSK_ERR_ARG;
	}
	if (!spend_write()) {
		if (!g_torn) {
			/* Genau dieser eine Vorgang reisst mitten entzwei:
			 * die erste Haelfte steht im Flash, der Rest nicht.
			 * Danach ist das Geraet aus und schreibt nichts mehr. */
			g_torn = true;
			size_t half = len / 2u;

			for (size_t i = 0; i < half; i++) {
				g_bank[g_cur].mem[addr + i] &= ((const uint8_t *)buf)[i];
			}
		}
		return OSK_ERR_IO;
	}
	/* Programmieren kann Bits nur von 1 auf 0 ziehen - wie echter Flash. */
	for (size_t i = 0; i < len; i++) {
		g_bank[g_cur].mem[addr + i] &= ((const uint8_t *)buf)[i];
	}
	return OSK_OK;
}

osk_result_t osk_flash_erase(uint32_t addr, size_t len)
{
	if ((uint64_t)addr + len > OSK_VAULT_SIZE || (addr % OSK_SECTOR) != 0u ||
	    (len % OSK_SECTOR) != 0u) {
		return OSK_ERR_ARG;
	}
	if (!spend_write()) {
		if (!g_torn && len > 0u) {
			/* halb geloescht: die erste Haelfte des ersten Sektors
			 * ist weg, der Rest bleibt stehen */
			g_torn = true;
			memset(g_bank[g_cur].mem + addr, 0xFF, OSK_SECTOR / 2u);
			g_bank[g_cur].erases[addr / OSK_SECTOR]++;
		}
		return OSK_ERR_IO;
	}
	memset(g_bank[g_cur].mem + addr, 0xFF, len);
	for (size_t off = 0; off < len; off += OSK_SECTOR) {
		g_bank[g_cur].erases[(addr + off) / OSK_SECTOR]++;
	}
	return OSK_OK;
}

osk_result_t osk_flash_sync(void)
{
	return g_dead ? OSK_ERR_IO : OSK_OK;
}
