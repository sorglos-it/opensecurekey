/* Keccak-f[1600] nach FIPS 202. Siehe pqc_keccak.h. */
#include "pqc_keccak.h"
#include "osk_mem.h"

/* Die 24 Rundenkonstanten. Sie sind das Ergebnis eines Schieberegisters
 * (FIPS 202, Abschnitt 3.2.5) und stehen hier als Tabelle, weil sie
 * feststehen. Nachzurechnen mit der dort beschriebenen Folge rc(t). */
static const uint64_t k_round[24] = {
    0x0000000000000001u, 0x0000000000008082u, 0x800000000000808au, 0x8000000080008000u,
    0x000000000000808bu, 0x0000000080000001u, 0x8000000080008081u, 0x8000000000008009u,
    0x000000000000008au, 0x0000000000000088u, 0x0000000080008009u, 0x000000008000000au,
    0x000000008000808bu, 0x800000000000008bu, 0x8000000000008089u, 0x8000000000008003u,
    0x8000000000008002u, 0x8000000000000080u, 0x000000000000800au, 0x800000008000000au,
    0x8000000080008081u, 0x8000000000008080u, 0x0000000080000001u, 0x8000000080008008u,
};

/* Drehweiten der rho-Stufe, gelesen als [x + 5*y]. FIPS 202, Tabelle 2. */
static const uint8_t k_rho[25] = {
    0,	1,  62, 28, 27, /* y=0 */
    36, 44, 6,	55, 20, /* y=1 */
    3,	10, 43, 25, 39, /* y=2 */
    41, 45, 15, 21, 8,	/* y=3 */
    18, 2,  61, 56, 14, /* y=4 */
};

/* Drehen nach links. Die Weite 0 muss abgefangen werden: x >> 64 ist in C
 * nicht festgelegt, und k_rho[0] ist 0. */
static uint64_t rotl(uint64_t x, unsigned int n)
{
	if (n == 0u) {
		return x;
	}
	return (x << n) | (x >> (64u - n));
}

static void permute(uint64_t *a)
{
	for (unsigned int round = 0; round < 24u; round++) {
		uint64_t c[5], d[5], b[25];

		/* theta */
		for (unsigned int x = 0; x < 5u; x++) {
			c[x] = a[x] ^ a[x + 5u] ^ a[x + 10u] ^ a[x + 15u] ^ a[x + 20u];
		}
		for (unsigned int x = 0; x < 5u; x++) {
			d[x] = c[(x + 4u) % 5u] ^ rotl(c[(x + 1u) % 5u], 1u);
		}
		for (unsigned int y = 0; y < 5u; y++) {
			for (unsigned int x = 0; x < 5u; x++) {
				a[x + 5u * y] ^= d[x];
			}
		}

		/* rho und pi in einem Durchgang: (x,y) wandert nach (y, 2x+3y) */
		for (unsigned int y = 0; y < 5u; y++) {
			for (unsigned int x = 0; x < 5u; x++) {
				unsigned int from = x + 5u * y;
				unsigned int to = y + 5u * ((2u * x + 3u * y) % 5u);

				b[to] = rotl(a[from], k_rho[from]);
			}
		}

		/* chi */
		for (unsigned int y = 0; y < 5u; y++) {
			for (unsigned int x = 0; x < 5u; x++) {
				a[x + 5u * y] = b[x + 5u * y] ^ (~b[(x + 1u) % 5u + 5u * y] &
								 b[(x + 2u) % 5u + 5u * y]);
			}
		}

		/* iota */
		a[0] ^= k_round[round];
	}
}

/* Ein Byte an Stelle i des Zustands verodern. Keccak liest den Zustand
 * little-endian je Spur, darum i/8 und i%8. */
static void xor_byte(uint64_t *a, size_t i, uint8_t value)
{
	a[i / 8u] ^= (uint64_t)value << (8u * (i % 8u));
}

static uint8_t get_byte(const uint64_t *a, size_t i)
{
	return (uint8_t)(a[i / 8u] >> (8u * (i % 8u)));
}

void osk_keccak_init(struct osk_keccak *st, size_t rate, uint8_t pad)
{
	for (size_t i = 0; i < 25u; i++) {
		st->lane[i] = 0u;
	}
	st->rate = rate;
	st->pos = 0u;
	st->pad = pad;
	st->squeezing = 0;
}

void osk_keccak_absorb(struct osk_keccak *st, const uint8_t *in, size_t len)
{
	if (st->squeezing) {
		return;
	}
	for (size_t i = 0; i < len; i++) {
		xor_byte(st->lane, st->pos, in[i]);
		st->pos++;
		if (st->pos == st->rate) {
			permute(st->lane);
			st->pos = 0u;
		}
	}
}

/* Abschluss nach FIPS 202: Bereichsbyte an den Fuellstand, hoechstes Bit
 * des letzten Blockbytes, dann permutieren. */
static void finish(struct osk_keccak *st)
{
	xor_byte(st->lane, st->pos, st->pad);
	xor_byte(st->lane, st->rate - 1u, 0x80u);
	permute(st->lane);
	st->pos = 0u;
	st->squeezing = 1;
}

void osk_keccak_squeeze(struct osk_keccak *st, uint8_t *out, size_t len)
{
	if (!st->squeezing) {
		finish(st);
	}
	for (size_t i = 0; i < len; i++) {
		if (st->pos == st->rate) {
			permute(st->lane);
			st->pos = 0u;
		}
		out[i] = get_byte(st->lane, st->pos);
		st->pos++;
	}
}

void osk_keccak_wipe(struct osk_keccak *st)
{
	osk_wipe(st, sizeof(*st));
}

void osk_sha3_256(uint8_t out[32], const uint8_t *in, size_t len)
{
	struct osk_keccak st;

	osk_keccak_init(&st, OSK_SHA3_256_RATE, OSK_KECCAK_PAD_SHA3);
	osk_keccak_absorb(&st, in, len);
	osk_keccak_squeeze(&st, out, 32u);
	osk_keccak_wipe(&st);
}

void osk_sha3_512(uint8_t out[64], const uint8_t *in, size_t len)
{
	struct osk_keccak st;

	osk_keccak_init(&st, OSK_SHA3_512_RATE, OSK_KECCAK_PAD_SHA3);
	osk_keccak_absorb(&st, in, len);
	osk_keccak_squeeze(&st, out, 64u);
	osk_keccak_wipe(&st);
}

void osk_shake256(uint8_t *out, size_t out_len, const uint8_t *in, size_t len)
{
	struct osk_keccak st;

	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, in, len);
	osk_keccak_squeeze(&st, out, out_len);
	osk_keccak_wipe(&st);
}
