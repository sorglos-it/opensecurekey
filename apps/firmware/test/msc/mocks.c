/* Siehe mocks.h. */
#include <stdlib.h>
#include <string.h>
#include "mocks.h"

/* --- Transportliste --------------------------------------------------- */

#define LINK_COUNT (OSK_LINK_BLE + 1)

static bool g_registered[LINK_COUNT];
static int g_enabled[LINK_COUNT];

void mock_transport_reset(void)
{
	memset(g_registered, 0, sizeof(g_registered));
	for (unsigned i = 0; i < LINK_COUNT; i++)
		g_enabled[i] = -1;
}

bool mock_transport_registered(osk_link_t link)
{
	return g_registered[(unsigned)link];
}

int mock_transport_enabled(osk_link_t link)
{
	return g_enabled[(unsigned)link];
}

osk_result_t osk_transport_register(osk_link_t link, osk_handler_fn handler)
{
	if (!handler)
		return OSK_ERR_ARG;
	g_registered[(unsigned)link] = true;
	return OSK_OK;
}

osk_result_t osk_transport_enable(osk_link_t link, bool on)
{
	g_enabled[(unsigned)link] = on ? 1 : 0;
	return OSK_OK;
}

/* --- Laufwerk im RAM --------------------------------------------------- */

static uint8_t *g_image;
static uint32_t g_blocks;
static uint32_t g_fail_at = UINT32_MAX;
static uint32_t g_reads;
static bool g_written;

void fake_drive_pattern(uint32_t lba, uint8_t out[OSK_MSC_BLOCK_LEN])
{
	for (unsigned i = 0; i < OSK_MSC_BLOCK_LEN; i++)
		out[i] = (uint8_t)((lba * 7u + i * 31u + 0x5au) & 0xffu);
	/* Die ersten vier Byte sind die Blocknummer im Klartext. */
	out[0] = (uint8_t)(lba >> 24);
	out[1] = (uint8_t)(lba >> 16);
	out[2] = (uint8_t)(lba >> 8);
	out[3] = (uint8_t)lba;
}

void fake_drive_init(uint32_t blocks)
{
	fake_drive_free();
	g_image = (uint8_t *)malloc((size_t)blocks * OSK_MSC_BLOCK_LEN);
	g_blocks = g_image ? blocks : 0u;
	for (uint32_t b = 0; b < g_blocks; b++)
		fake_drive_pattern(b, g_image + (size_t)b * OSK_MSC_BLOCK_LEN);
	g_fail_at = UINT32_MAX;
	g_reads = 0;
	g_written = false;
}

void fake_drive_free(void)
{
	free(g_image);
	g_image = NULL;
	g_blocks = 0;
}

uint32_t fake_drive_blocks(void)
{
	return g_blocks;
}

void fake_drive_fail_at(uint32_t lba)
{
	g_fail_at = lba;
}

uint32_t fake_drive_reads(void)
{
	return g_reads;
}

void fake_drive_reads_reset(void)
{
	g_reads = 0;
}

bool fake_drive_was_written(void)
{
	return g_written;
}

osk_result_t fake_drive_read(uint32_t lba, uint8_t out[OSK_MSC_BLOCK_LEN], void *ctx)
{
	(void)ctx;
	if (!g_image || lba >= g_blocks)
		return OSK_ERR_ARG;
	if (lba == g_fail_at)
		return OSK_ERR_IO;
	g_reads++;
	memcpy(out, g_image + (size_t)lba * OSK_MSC_BLOCK_LEN, OSK_MSC_BLOCK_LEN);
	return OSK_OK;
}

/* --- Mitschrift -------------------------------------------------------- */

static uint8_t g_sink[MOCK_SINK_MAX];
static size_t g_sink_len;
static unsigned g_sink_calls;

void mock_sink_reset(void)
{
	g_sink_len = 0;
	g_sink_calls = 0;
}

void mock_sink_send(const uint8_t *data, size_t len, void *ctx)
{
	(void)ctx;
	g_sink_calls++;
	if (!data || len == 0u)
		return;
	if (g_sink_len + len > sizeof(g_sink))
		len = sizeof(g_sink) - g_sink_len;
	memcpy(g_sink + g_sink_len, data, len);
	g_sink_len += len;
}

size_t mock_sink_len(void)
{
	return g_sink_len;
}

const uint8_t *mock_sink_data(void)
{
	return g_sink;
}

unsigned mock_sink_calls(void)
{
	return g_sink_calls;
}

bool mock_sink_csw(uint32_t *tag_out, uint32_t *residue_out, uint8_t *status_out)
{
	if (g_sink_len < OSK_MSC_CSW_LEN)
		return false;

	const uint8_t *p = g_sink + g_sink_len - OSK_MSC_CSW_LEN;
	const uint32_t sig = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
			     ((uint32_t)p[3] << 24);

	if (sig != OSK_MSC_CSW_SIG)
		return false;
	if (tag_out)
		*tag_out = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) |
			   ((uint32_t)p[7] << 24);
	if (residue_out)
		*residue_out = (uint32_t)p[8] | ((uint32_t)p[9] << 8) | ((uint32_t)p[10] << 16) |
			       ((uint32_t)p[11] << 24);
	if (status_out)
		*status_out = p[12];
	return true;
}

/* --- Bequemlichkeit ---------------------------------------------------- */

void mock_cbw(uint32_t tag, uint32_t host_len, bool in, uint8_t lun, const uint8_t *cdb,
	      uint8_t cdb_len)
{
	uint8_t cbw[OSK_MSC_CBW_LEN];

	memset(cbw, 0, sizeof(cbw));
	cbw[0] = (uint8_t)OSK_MSC_CBW_SIG;
	cbw[1] = (uint8_t)(OSK_MSC_CBW_SIG >> 8);
	cbw[2] = (uint8_t)(OSK_MSC_CBW_SIG >> 16);
	cbw[3] = (uint8_t)(OSK_MSC_CBW_SIG >> 24);
	cbw[4] = (uint8_t)tag;
	cbw[5] = (uint8_t)(tag >> 8);
	cbw[6] = (uint8_t)(tag >> 16);
	cbw[7] = (uint8_t)(tag >> 24);
	cbw[8] = (uint8_t)host_len;
	cbw[9] = (uint8_t)(host_len >> 8);
	cbw[10] = (uint8_t)(host_len >> 16);
	cbw[11] = (uint8_t)(host_len >> 24);
	cbw[12] = in ? 0x80u : 0x00u;
	cbw[13] = lun;
	cbw[14] = cdb_len;

	const uint8_t copy = (cdb_len > OSK_MSC_CDB_MAX) ? OSK_MSC_CDB_MAX : cdb_len;

	if (cdb && copy)
		memcpy(cbw + 15, cdb, copy);
	osk_msc_rx(cbw, sizeof(cbw));
}
