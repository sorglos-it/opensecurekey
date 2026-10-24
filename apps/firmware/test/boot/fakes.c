/* Umsetzung der Attrappen - siehe fakes.h, warum keine davon Krypto ist. */

#include <string.h>
#include "fakes.h"

/* --- Flash --------------------------------------------------------------- */

void fake_flash_reset(fake_flash_t *f)
{
	memset(f, 0xFF, sizeof(f->data)); /* geloeschter Flash ist 0xFF */
	f->written_bytes = 0;
	f->erases = 0;
	f->fail_after = 0;
	f->erase_fails = false;
}

osk_result_t fake_flash_erase(void *ctx, uint32_t offset, uint32_t len)
{
	fake_flash_t *f = (fake_flash_t *)ctx;

	if (f->erase_fails) {
		return OSK_ERR_IO;
	}
	if (offset > FAKE_SLOT_LEN || len > FAKE_SLOT_LEN - offset) {
		return OSK_ERR_ARG;
	}
	memset(f->data + offset, 0xFF, len);
	f->erases++;
	return OSK_OK;
}

osk_result_t fake_flash_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len)
{
	fake_flash_t *f = (fake_flash_t *)ctx;
	uint32_t room;

	if (offset > FAKE_SLOT_LEN || len > (size_t)(FAKE_SLOT_LEN - offset)) {
		return OSK_ERR_ARG;
	}
	if (f->fail_after != 0 && f->written_bytes >= f->fail_after) {
		return OSK_ERR_IO;
	}
	if (f->fail_after != 0 && f->written_bytes + len > f->fail_after) {
		/* Teilweise angenommen, dann weg - so sieht ein gezogener
		 * Stecker mitten im Schreiben aus. */
		room = f->fail_after - f->written_bytes;
		memcpy(f->data + offset, data, room);
		f->written_bytes += room;
		return OSK_ERR_IO;
	}
	memcpy(f->data + offset, data, len);
	f->written_bytes += (uint32_t)len;
	return OSK_OK;
}

/* --- Signatur und Hash ---------------------------------------------------- */

/* Eine kurze, eindeutige Ableitung aus der Nachricht, auf 64 Byte gestreckt.
 * Reicht fuer "stimmt / stimmt nicht", und mehr wird hier nicht gebraucht. */
void fake_sign(const uint8_t *msg, size_t len, uint8_t sig[OSK_BOOT_SIG_LEN])
{
	uint32_t acc = 0x1234u;

	for (size_t i = 0; i < len; i++) {
		acc = acc * 131u + msg[i] + (uint32_t)i;
	}
	for (int i = 0; i < OSK_BOOT_SIG_LEN; i++) {
		acc = acc * 1664525u + 1013904223u;
		sig[i] = (uint8_t)(acc >> 24);
	}
}

bool fake_verify(const uint8_t *msg, size_t len, const uint8_t *sig)
{
	uint8_t want[OSK_BOOT_SIG_LEN];

	if (msg == NULL || sig == NULL) {
		return false;
	}
	fake_sign(msg, len, want);
	return memcmp(want, sig, OSK_BOOT_SIG_LEN) == 0;
}

void fake_hash_init(void *ctx)
{
	fake_hash_ctx_t *h = (fake_hash_ctx_t *)ctx;

	h->acc = 0xcbf29ce484222325ull;
	h->count = 0;
}

void fake_hash_update(void *ctx, const uint8_t *data, size_t len)
{
	fake_hash_ctx_t *h = (fake_hash_ctx_t *)ctx;

	for (size_t i = 0; i < len; i++) {
		h->acc = (h->acc ^ data[i]) * 0x100000001b3ull;
		h->count++;
	}
}

void fake_hash_final(void *ctx, uint8_t out[OSK_BOOT_HASH_LEN])
{
	fake_hash_ctx_t *h = (fake_hash_ctx_t *)ctx;
	uint64_t a = h->acc ^ (h->count * 0x9e3779b97f4a7c15ull);

	for (int i = 0; i < OSK_BOOT_HASH_LEN; i++) {
		a = a * 6364136223846793005ull + 1442695040888963407ull;
		out[i] = (uint8_t)(a >> 56);
	}
}

void fake_hash_all(const uint8_t *data, size_t len, uint8_t out[OSK_BOOT_HASH_LEN])
{
	fake_hash_ctx_t h;

	fake_hash_init(&h);
	fake_hash_update(&h, data, len);
	fake_hash_final(&h, out);
}

/* --- Zaehler ------------------------------------------------------------- */

static uint32_t g_counter;
static bool g_counter_broken;

void fake_counter_set(uint32_t value)
{
	g_counter = value;
	g_counter_broken = false;
}

uint32_t fake_counter_get(void)
{
	return g_counter;
}

void fake_counter_break(bool broken)
{
	g_counter_broken = broken;
}

osk_result_t fake_counter_read(uint32_t *out)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (g_counter_broken) {
		return OSK_ERR_IO;
	}
	*out = g_counter;
	return OSK_OK;
}

osk_result_t fake_counter_raise(uint32_t value)
{
	if (g_counter_broken) {
		return OSK_ERR_IO;
	}
	/* Absenken geht nie - so verhaelt sich der Zaehler im Chip. */
	if (value <= g_counter) {
		return OSK_ERR_EXISTS;
	}
	g_counter = value;
	return OSK_OK;
}

/* --- Zusammenbau --------------------------------------------------------- */

void fake_env_init(fake_env_t *env, uint32_t board_id, const uint32_t running[4])
{
	memset(env, 0, sizeof(*env));
	env->be.verify = fake_verify;
	env->be.hash_init = fake_hash_init;
	env->be.hash_update = fake_hash_update;
	env->be.hash_final = fake_hash_final;
	env->be.hash_ctx = &env->hash;
	env->be.counter_read = fake_counter_read;
	env->be.counter_raise = fake_counter_raise;
	env->be.board_id = board_id;
	for (int i = 0; i < 4; i++) {
		env->be.running_version[i] = running[i];
	}
}

static void put_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
	p[2] = (uint8_t)((v >> 16) & 0xFFu);
	p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

size_t fake_pkg(uint8_t *buf, size_t cap, uint32_t board_id, const uint32_t version[4],
		uint32_t counter, const uint8_t *image, uint32_t image_len)
{
	size_t total = (size_t)OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + image_len;

	if (cap < total) {
		return 0;
	}
	memset(buf, 0, total);
	memcpy(buf, OSK_BOOT_MAGIC, OSK_BOOT_MAGIC_LEN);
	buf[8] = (uint8_t)(OSK_BOOT_HEADER_LEN & 0xFFu);
	buf[9] = (uint8_t)(OSK_BOOT_HEADER_LEN >> 8);
	buf[10] = (uint8_t)OSK_BOOT_FORMAT;
	buf[11] = 0;
	put_u32(buf + 12, board_id);
	for (int i = 0; i < 4; i++) {
		put_u32(buf + 16 + 4 * i, version[i]);
	}
	put_u32(buf + 32, counter);
	put_u32(buf + 36, image_len);
	fake_hash_all(image, image_len, buf + 40);
	fake_sign(buf, OSK_BOOT_HEADER_LEN, buf + OSK_BOOT_HEADER_LEN);
	memcpy(buf + OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN, image, image_len);
	return total;
}

void fake_session_init(osk_boot_session_t *se, fake_flash_t *flash)
{
	memset(se, 0, sizeof(*se));
	se->ctx = flash;
	se->slot_len = FAKE_SLOT_LEN;
	se->erase = fake_flash_erase;
	se->write = fake_flash_write;
}
