/* Karten-Attrappe - siehe fake_card.h. KEINE Hardware beteiligt. */

#include <stdlib.h>
#include <string.h>
#include "fake_card.h"
#include "osk_volume.h"
#include "osk_volume_card.h"

#define SLOTS 2

struct slot {
	uint8_t *mem;
	uint8_t *snap;
	uint32_t blocks;
	bool present;
	bool wp;
};

static struct slot g_slot[SLOTS];
static int g_cur;
static uint32_t g_reads;
static uint32_t g_writes;

static struct slot *cur(void)
{
	return &g_slot[g_cur];
}

void fake_card_select(int slot)
{
	if (slot >= 0 && slot < SLOTS) {
		g_cur = slot;
	}
}

void fake_card_init(uint32_t blocks)
{
	struct slot *s = cur();

	free(s->mem);
	free(s->snap);
	s->mem = calloc(blocks, OSK_VOL_BLOCK_LEN);
	s->snap = NULL;
	s->blocks = blocks;
	s->present = s->mem != NULL;
	s->wp = false;
	g_reads = 0u;
	g_writes = 0u;
}

void fake_card_free(void)
{
	for (int i = 0; i < SLOTS; i++) {
		free(g_slot[i].mem);
		free(g_slot[i].snap);
		g_slot[i].mem = NULL;
		g_slot[i].snap = NULL;
		g_slot[i].blocks = 0u;
		g_slot[i].present = false;
	}
}

void fake_card_eject(void)
{
	cur()->present = false;
}

void fake_card_insert(void)
{
	cur()->present = cur()->mem != NULL;
}

void fake_card_write_protect(bool on)
{
	cur()->wp = on;
}

void fake_card_count_reset(void)
{
	g_reads = 0u;
	g_writes = 0u;
}

uint32_t fake_card_reads(void)
{
	return g_reads;
}

uint32_t fake_card_writes(void)
{
	return g_writes;
}

void fake_card_flip(uint32_t block, uint32_t offset)
{
	struct slot *s = cur();

	if (s->mem == NULL || block >= s->blocks || offset >= OSK_VOL_BLOCK_LEN) {
		return;
	}
	s->mem[block * OSK_VOL_BLOCK_LEN + offset] ^= 0x40u;
}

void fake_card_snapshot(void)
{
	struct slot *s = cur();

	if (s->mem == NULL) {
		return;
	}
	free(s->snap);
	s->snap = malloc((size_t)s->blocks * OSK_VOL_BLOCK_LEN);
	if (s->snap != NULL) {
		memcpy(s->snap, s->mem, (size_t)s->blocks * OSK_VOL_BLOCK_LEN);
	}
}

void fake_card_rollback(void)
{
	struct slot *s = cur();

	if (s->mem != NULL && s->snap != NULL) {
		memcpy(s->mem, s->snap, (size_t)s->blocks * OSK_VOL_BLOCK_LEN);
	}
}

/* --- die Schnittstelle selbst ----------------------------------------- */

osk_result_t osk_card_info(osk_card_info_t *out)
{
	struct slot *s = cur();

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	out->blocks = s->blocks;
	out->present = s->present;
	out->write_protected = s->wp;
	return OSK_OK;
}

osk_result_t osk_card_read(uint32_t block, void *buf, size_t len)
{
	struct slot *s = cur();

	if (buf == NULL || len != OSK_VOL_BLOCK_LEN) {
		return OSK_ERR_ARG;
	}
	if (!s->present || s->mem == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (block >= s->blocks) {
		return OSK_ERR_ARG;
	}
	g_reads++;
	memcpy(buf, s->mem + (size_t)block * OSK_VOL_BLOCK_LEN, OSK_VOL_BLOCK_LEN);
	return OSK_OK;
}

osk_result_t osk_card_write(uint32_t block, const void *buf, size_t len)
{
	struct slot *s = cur();

	if (buf == NULL || len != OSK_VOL_BLOCK_LEN) {
		return OSK_ERR_ARG;
	}
	if (!s->present || s->mem == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (s->wp) {
		return OSK_ERR_DENIED;
	}
	if (block >= s->blocks) {
		return OSK_ERR_ARG;
	}
	g_writes++;
	memcpy(s->mem + (size_t)block * OSK_VOL_BLOCK_LEN, buf, OSK_VOL_BLOCK_LEN);
	return OSK_OK;
}

osk_result_t osk_card_sync(void)
{
	return OSK_OK;
}
