/* SHA-256 und SHA-512 nach FIPS 180-4.
 *
 * Geschrieben nach der Beschreibung des Verfahrens: die Rundenkonstanten
 * sind die Nachkommastellen der Kubikwurzeln der ersten Primzahlen, die
 * Startwerte die der Quadratwurzeln. Beides steht so in der Norm.
 */
#include "sha2.h"

#include <string.h>

void osk_wipe(void *data, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)data;
    while (len-- > 0) {
        *p++ = 0;
    }
}

/* ---------------------------------------------------------------- SHA-256 */

static const uint32_t sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotr32(uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32u - bits));
}

static uint32_t load_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void store_be32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void sha256_block(uint32_t state[8], const unsigned char block[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned i;

    for (i = 0; i < 16; i++) {
        w[i] = load_be32(block + 4u * i);
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (i = 0; i < 64; i++) {
        uint32_t big_s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + big_s1 + choose + sha256_k[i] + w[i];
        uint32_t big_s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t major = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = big_s0 + major;
        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    osk_wipe(w, sizeof(w));
}

void osk_sha256_init(osk_sha256_ctx *ctx)
{
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->count = 0;
    ctx->used = 0;
    memset(ctx->block, 0, sizeof(ctx->block));
}

void osk_sha256_update(osk_sha256_ctx *ctx, const void *data, size_t len)
{
    const unsigned char *in = (const unsigned char *)data;

    if (len == 0) {
        return;
    }
    ctx->count += (uint64_t)len;
    if (ctx->used > 0) {
        size_t room = sizeof(ctx->block) - ctx->used;
        size_t take = (len < room) ? len : room;
        memcpy(ctx->block + ctx->used, in, take);
        ctx->used += take;
        in += take;
        len -= take;
        if (ctx->used < sizeof(ctx->block)) {
            return;
        }
        sha256_block(ctx->state, ctx->block);
        ctx->used = 0;
    }
    while (len >= sizeof(ctx->block)) {
        sha256_block(ctx->state, in);
        in += sizeof(ctx->block);
        len -= sizeof(ctx->block);
    }
    if (len > 0) {
        memcpy(ctx->block, in, len);
        ctx->used = len;
    }
}

void osk_sha256_final(osk_sha256_ctx *ctx, unsigned char out[OSK_SHA256_LEN])
{
    uint64_t bits = ctx->count * 8u;
    unsigned i;

    ctx->block[ctx->used++] = 0x80;
    if (ctx->used > 56) {
        memset(ctx->block + ctx->used, 0, sizeof(ctx->block) - ctx->used);
        sha256_block(ctx->state, ctx->block);
        ctx->used = 0;
    }
    memset(ctx->block + ctx->used, 0, 56 - ctx->used);
    for (i = 0; i < 8; i++) {
        ctx->block[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    }
    sha256_block(ctx->state, ctx->block);
    for (i = 0; i < 8; i++) {
        store_be32(out + 4u * i, ctx->state[i]);
    }
    osk_wipe(ctx, sizeof(*ctx));
}

void osk_sha256(const void *data, size_t len, unsigned char out[OSK_SHA256_LEN])
{
    osk_sha256_ctx ctx;
    osk_sha256_init(&ctx);
    osk_sha256_update(&ctx, data, len);
    osk_sha256_final(&ctx, out);
}

/* ---------------------------------------------------------------- SHA-512 */

static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full,
    0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull, 0x59f111f1b605d019ull,
    0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull,
    0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
    0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
    0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull,
    0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull, 0x2de92c6f592b0275ull,
    0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
    0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full,
    0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
    0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull,
    0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
    0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull,
    0x92722c851482353bull, 0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull,
    0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
    0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
    0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull,
    0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull,
    0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull,
    0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull,
    0xc67178f2e372532bull, 0xca273eceea26619cull, 0xd186b8c721c0c207ull,
    0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull,
    0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
    0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
    0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull,
    0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

static uint64_t rotr64(uint64_t value, unsigned bits)
{
    return (value >> bits) | (value << (64u - bits));
}

static uint64_t load_be64(const unsigned char *p)
{
    uint64_t out = 0;
    unsigned i;
    for (i = 0; i < 8; i++) {
        out = (out << 8) | (uint64_t)p[i];
    }
    return out;
}

static void store_be64(unsigned char *p, uint64_t value)
{
    unsigned i;
    for (i = 0; i < 8; i++) {
        p[i] = (unsigned char)(value >> (56 - 8 * i));
    }
}

static void sha512_block(uint64_t state[8], const unsigned char block[128])
{
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h;
    unsigned i;

    for (i = 0; i < 16; i++) {
        w[i] = load_be64(block + 8u * i);
    }
    for (i = 16; i < 80; i++) {
        uint64_t s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (i = 0; i < 80; i++) {
        uint64_t big_s1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        uint64_t choose = (e & f) ^ ((~e) & g);
        uint64_t t1 = h + big_s1 + choose + sha512_k[i] + w[i];
        uint64_t big_s0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        uint64_t major = (a & b) ^ (a & c) ^ (b & c);
        uint64_t t2 = big_s0 + major;
        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    osk_wipe(w, sizeof(w));
}

void osk_sha512_init(osk_sha512_ctx *ctx)
{
    ctx->state[0] = 0x6a09e667f3bcc908ull;
    ctx->state[1] = 0xbb67ae8584caa73bull;
    ctx->state[2] = 0x3c6ef372fe94f82bull;
    ctx->state[3] = 0xa54ff53a5f1d36f1ull;
    ctx->state[4] = 0x510e527fade682d1ull;
    ctx->state[5] = 0x9b05688c2b3e6c1full;
    ctx->state[6] = 0x1f83d9abfb41bd6bull;
    ctx->state[7] = 0x5be0cd19137e2179ull;
    ctx->count_lo = 0;
    ctx->count_hi = 0;
    ctx->used = 0;
    memset(ctx->block, 0, sizeof(ctx->block));
}

void osk_sha512_update(osk_sha512_ctx *ctx, const void *data, size_t len)
{
    const unsigned char *in = (const unsigned char *)data;

    if (len == 0) {
        return;
    }
    ctx->count_lo += (uint64_t)len;
    if (ctx->count_lo < (uint64_t)len) {
        ctx->count_hi++;
    }
    if (ctx->used > 0) {
        size_t room = sizeof(ctx->block) - ctx->used;
        size_t take = (len < room) ? len : room;
        memcpy(ctx->block + ctx->used, in, take);
        ctx->used += take;
        in += take;
        len -= take;
        if (ctx->used < sizeof(ctx->block)) {
            return;
        }
        sha512_block(ctx->state, ctx->block);
        ctx->used = 0;
    }
    while (len >= sizeof(ctx->block)) {
        sha512_block(ctx->state, in);
        in += sizeof(ctx->block);
        len -= sizeof(ctx->block);
    }
    if (len > 0) {
        memcpy(ctx->block, in, len);
        ctx->used = len;
    }
}

void osk_sha512_final(osk_sha512_ctx *ctx, unsigned char out[OSK_SHA512_LEN])
{
    /* Die Laenge geht als 128-Bit-Zahl in Bit hinein. */
    uint64_t bits_lo = ctx->count_lo << 3;
    uint64_t bits_hi = (ctx->count_hi << 3) | (ctx->count_lo >> 61);
    unsigned i;

    ctx->block[ctx->used++] = 0x80;
    if (ctx->used > 112) {
        memset(ctx->block + ctx->used, 0, sizeof(ctx->block) - ctx->used);
        sha512_block(ctx->state, ctx->block);
        ctx->used = 0;
    }
    memset(ctx->block + ctx->used, 0, 112 - ctx->used);
    store_be64(ctx->block + 112, bits_hi);
    store_be64(ctx->block + 120, bits_lo);
    sha512_block(ctx->state, ctx->block);
    for (i = 0; i < 8; i++) {
        store_be64(out + 8u * i, ctx->state[i]);
    }
    osk_wipe(ctx, sizeof(*ctx));
}

void osk_sha512(const void *data, size_t len, unsigned char out[OSK_SHA512_LEN])
{
    osk_sha512_ctx ctx;
    osk_sha512_init(&ctx);
    osk_sha512_update(&ctx, data, len);
    osk_sha512_final(&ctx, out);
}
