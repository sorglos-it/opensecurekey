/* Ed25519-Pruefung nach RFC 8032 - ohne Fremdcode, ohne Unterschreiben.
 *
 * Aufbau:
 *  1. Feldarithmetik ueber dem Primkoerper 2^255-19. Ein Feldelement sind
 *     acht 32-Bit-Glieder, kleinstes zuerst, immer kleiner als p.
 *  2. Kurvenpunkte in erweiterten Koordinaten (X:Y:Z:T) mit T = XY/Z. Die
 *     Additionsformel aus RFC 8032, Abschnitt 5.1.4, gilt dort auch fuer das
 *     Verdoppeln - darum gibt es nur eine Formel.
 *  3. Die Pruefung selbst: S gegen die Gruppenordnung, beide Punkte
 *     entpacken, k = SHA-512(R || A || M) mod L, dann [S]B gegen R + [k]A.
 *
 * Es wird nirgends mit einem geheimen Wert gerechnet; darum ist hier auch
 * keine Laufzeitgleichheit gegen Seitenkanaele noetig. Die Vergleiche sind
 * trotzdem ohne Verzweigung geschrieben, weil es nichts kostet.
 */
#include "ed25519_verify.h"

#include <stdint.h>
#include <string.h>

#include "sha2.h"

/* ------------------------------------------------------ Feld 2^255-19 */

typedef uint32_t fe[8];

/* p = 2^255 - 19 */
static const uint32_t fe_p[8] = {
    0xffffffedu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0x7fffffffu
};

/* 2p = 2^256 - 38; wird beim Abziehen aufgeschlagen, damit nie ein
 * negativer Zwischenwert entsteht. */
static const uint32_t fe_two_p[8] = {
    0xffffffdau, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu
};

/* Exponenten als Zahl mit kleinstem Byte zuerst. */

/* p - 2, fuer das Kehrbild */
static const unsigned char exp_inverse[32] = {
    0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

/* (p + 3) / 8 = 2^252 - 2, fuer die Quadratwurzel */
static const unsigned char exp_sqrt[32] = {
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f
};

/* (p - 1) / 4 = 2^253 - 5; 2 hoch diesem Wert ist die Wurzel aus -1 */
static const unsigned char exp_sqrt_minus_one[32] = {
    0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f
};

/* Die Gruppenordnung L = 2^252 + 27742317777372353535851937790883648493,
 * kleinstes Glied zuerst. Eine Unterschrift mit S >= L wird abgewiesen. */
static const uint32_t sc_l[8] = {
    0x5cf5d3edu, 0x5812631au, 0xa2f79cd6u, 0x14def9deu,
    0x00000000u, 0x00000000u, 0x00000000u, 0x10000000u
};

static void fe_zero(fe out)
{
    memset(out, 0, sizeof(fe));
}

static void fe_small(fe out, uint32_t value)
{
    fe_zero(out);
    out[0] = value;
}

static void fe_copy(fe out, const fe in)
{
    memcpy(out, in, sizeof(fe));
}

/* fe_add_small schlaegt eine kleine Zahl auf und gibt den Ueberlauf aus
 * dem obersten Glied zurueck. */
static uint32_t fe_add_small(uint32_t t[8], uint64_t add)
{
    int i;

    for (i = 0; i < 8; i++) {
        uint64_t sum = (uint64_t)t[i] + (add & 0xffffffffu);
        add >>= 32;
        t[i] = (uint32_t)sum;
        add += sum >> 32;
    }
    return (uint32_t)add;
}

/* fe_reduce_once zieht p ab, wenn der Wert nicht kleiner als p ist. */
static void fe_reduce_once(uint32_t t[8])
{
    uint32_t smaller[8];
    uint64_t borrow = 0;
    uint32_t mask;
    int i;

    for (i = 0; i < 8; i++) {
        uint64_t diff = (uint64_t)t[i] - (uint64_t)fe_p[i] - borrow;
        smaller[i] = (uint32_t)diff;
        borrow = (diff >> 32) & 1u;
    }
    /* borrow == 0 heisst: t war nicht kleiner als p, also nimm die Differenz */
    mask = (uint32_t)borrow - 1u;
    for (i = 0; i < 8; i++) {
        t[i] = (t[i] & ~mask) | (smaller[i] & mask);
    }
}

/* fe_norm bringt neun Glieder auf die kanonische Form. 2^256 ist 38 und
 * 2^255 ist 19 modulo p - beides klappt oben herunter. */
static void fe_norm(uint32_t t[9])
{
    int round;

    for (round = 0; round < 4 && t[8] != 0; round++) {
        uint64_t fold = 38ull * (uint64_t)t[8];
        t[8] = fe_add_small(t, fold);
    }
    for (round = 0; round < 3; round++) {
        uint32_t top = t[7] >> 31;
        if (top == 0) {
            break;
        }
        t[7] &= 0x7fffffffu;
        (void)fe_add_small(t, 19ull * (uint64_t)top);
    }
    fe_reduce_once(t);
}

static void fe_add(fe out, const fe a, const fe b)
{
    uint32_t t[9];
    uint64_t carry = 0;
    int i;

    for (i = 0; i < 8; i++) {
        uint64_t sum = (uint64_t)a[i] + (uint64_t)b[i] + carry;
        t[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    t[8] = (uint32_t)carry;
    fe_norm(t);
    memcpy(out, t, sizeof(fe));
}

static void fe_sub(fe out, const fe a, const fe b)
{
    uint32_t t[9];
    uint64_t carry = 0;
    uint64_t borrow = 0;
    int i;

    /* erst 2p aufschlagen, dann abziehen - so bleibt alles nichtnegativ */
    for (i = 0; i < 8; i++) {
        uint64_t sum = (uint64_t)a[i] + (uint64_t)fe_two_p[i] + carry;
        t[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    t[8] = (uint32_t)carry;
    for (i = 0; i < 8; i++) {
        uint64_t diff = (uint64_t)t[i] - (uint64_t)b[i] - borrow;
        t[i] = (uint32_t)diff;
        borrow = (diff >> 32) & 1u;
    }
    t[8] -= (uint32_t)borrow;
    fe_norm(t);
    memcpy(out, t, sizeof(fe));
}

static void fe_neg(fe out, const fe a)
{
    fe zero;
    fe_zero(zero);
    fe_sub(out, zero, a);
}

static void fe_mul(fe out, const fe a, const fe b)
{
    uint32_t wide[16];
    uint32_t t[9];
    uint64_t carry;
    int i, j;

    memset(wide, 0, sizeof(wide));
    for (i = 0; i < 8; i++) {
        carry = 0;
        for (j = 0; j < 8; j++) {
            uint64_t sum = (uint64_t)wide[i + j] +
                           (uint64_t)a[i] * (uint64_t)b[j] + carry;
            wide[i + j] = (uint32_t)sum;
            carry = sum >> 32;
        }
        wide[i + 8] = (uint32_t)carry;
    }
    carry = 0;
    for (i = 0; i < 8; i++) {
        uint64_t sum = (uint64_t)wide[i] + 38ull * (uint64_t)wide[i + 8] + carry;
        t[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    t[8] = (uint32_t)carry;
    fe_norm(t);
    memcpy(out, t, sizeof(fe));
}

static void fe_sq(fe out, const fe a)
{
    fe_mul(out, a, a);
}

/* fe_pow rechnet a hoch exp. exp steht als Zahl mit kleinstem Byte zuerst. */
static void fe_pow(fe out, const fe a, const unsigned char *exp, int exp_len)
{
    fe acc;
    int started = 0;
    int i;

    fe_small(acc, 1);
    for (i = exp_len * 8 - 1; i >= 0; i--) {
        int bit = (exp[i >> 3] >> (i & 7)) & 1;
        if (started) {
            fe_sq(acc, acc);
            if (bit) {
                fe_mul(acc, acc, a);
            }
        } else if (bit) {
            fe_copy(acc, a);
            started = 1;
        }
    }
    fe_copy(out, acc);
}

static void fe_inverse(fe out, const fe a)
{
    fe_pow(out, a, exp_inverse, 32);
}

static int fe_is_zero(const fe a)
{
    uint32_t any = 0;
    int i;
    for (i = 0; i < 8; i++) {
        any |= a[i];
    }
    return any == 0;
}

static int fe_equal(const fe a, const fe b)
{
    uint32_t diff = 0;
    int i;
    for (i = 0; i < 8; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

/* fe_from_bytes liest 32 Byte, kleinstes zuerst. Gibt 0 zurueck, wenn der
 * Wert nicht kleiner als p ist - RFC 8032 verlangt die kanonische Form. */
static int fe_from_bytes(fe out, const unsigned char in[32])
{
    int i;
    uint64_t borrow = 0;

    for (i = 0; i < 8; i++) {
        out[i] = (uint32_t)in[4 * i] |
                 ((uint32_t)in[4 * i + 1] << 8) |
                 ((uint32_t)in[4 * i + 2] << 16) |
                 ((uint32_t)in[4 * i + 3] << 24);
    }
    for (i = 0; i < 8; i++) {
        uint64_t diff = (uint64_t)out[i] - (uint64_t)fe_p[i] - borrow;
        borrow = (diff >> 32) & 1u;
    }
    return borrow == 1;
}

/* ---------------------------------------------- Kurve und ihre Konstanten */

/* curve haelt die Werte, die sich aus der Kurvengleichung ergeben. Sie
 * werden je Pruefung frisch gerechnet; das kostet wenige Tausend
 * Multiplikationen und erspart feste Zahlen im Code, die niemand nachrechnen
 * kann. */
typedef struct {
    fe d;        /* -121665 / 121666 */
    fe two_d;    /* 2d, so wie die Additionsformel es braucht */
    fe sqrt_m1;  /* Wurzel aus -1 */
    fe one;
} curve;

typedef struct {
    fe x;
    fe y;
    fe z;
    fe t;
} point;

static void point_identity(point *out)
{
    fe_zero(out->x);
    fe_small(out->y, 1);
    fe_small(out->z, 1);
    fe_zero(out->t);
}

/* point_add ist die vereinheitlichte Formel aus RFC 8032, Abschnitt 5.1.4.
 * Sie gilt auch, wenn beide Punkte gleich sind - darum wird nicht zwischen
 * Addieren und Verdoppeln unterschieden. */
static void point_add(point *out, const point *p, const point *q,
                      const curve *cv)
{
    fe a, b, c, d, e, f, g, h, u, v;

    fe_sub(u, p->y, p->x);
    fe_sub(v, q->y, q->x);
    fe_mul(a, u, v);

    fe_add(u, p->y, p->x);
    fe_add(v, q->y, q->x);
    fe_mul(b, u, v);

    fe_mul(c, p->t, q->t);
    fe_mul(c, c, cv->two_d);

    fe_mul(d, p->z, q->z);
    fe_add(d, d, d);

    fe_sub(e, b, a);
    fe_sub(f, d, c);
    fe_add(g, d, c);
    fe_add(h, b, a);

    fe_mul(out->x, e, f);
    fe_mul(out->y, g, h);
    fe_mul(out->t, e, h);
    fe_mul(out->z, f, g);
}

/* point_mul rechnet [scalar]p mit Verdoppeln und Addieren, hoechstes Bit
 * zuerst. scalar steht als 32 Byte, kleinstes zuerst. */
static void point_mul(point *out, const unsigned char scalar[32],
                      const point *p, const curve *cv)
{
    point acc;
    int i;

    point_identity(&acc);
    for (i = 255; i >= 0; i--) {
        point_add(&acc, &acc, &acc, cv);
        if ((scalar[i >> 3] >> (i & 7)) & 1) {
            point_add(&acc, &acc, p, cv);
        }
    }
    *out = acc;
}

/* point_equal vergleicht zwei Punkte, ohne Z zu beseitigen: X1*Z2 == X2*Z1
 * und Y1*Z2 == Y2*Z1. Das erspart zwei Kehrbilder. */
static int point_equal(const point *p, const point *q)
{
    fe left, right;
    int same;

    fe_mul(left, p->x, q->z);
    fe_mul(right, q->x, p->z);
    same = fe_equal(left, right);
    fe_mul(left, p->y, q->z);
    fe_mul(right, q->y, p->z);
    return same && fe_equal(left, right);
}

/* point_from_y holt x zu einem gegebenen y. sign ist das Bit 255 der
 * Kodierung: es sagt, welche der beiden Wurzeln gemeint ist.
 * Gibt 0 zurueck, wenn es kein x gibt - dann ist der Punkt nicht auf der
 * Kurve und die Unterschrift wird abgewiesen. */
static int point_from_y(point *out, const fe y, int sign, const curve *cv)
{
    fe y_squared, u, v, x, check;

    fe_sq(y_squared, y);
    fe_sub(u, y_squared, cv->one);        /* u = y^2 - 1 */
    fe_mul(v, y_squared, cv->d);
    fe_add(v, v, cv->one);                /* v = d*y^2 + 1 */
    if (fe_is_zero(v)) {
        return 0;
    }
    fe_inverse(check, v);
    fe_mul(u, u, check);                  /* u = (y^2-1) / (d*y^2+1) = x^2 */

    fe_pow(x, u, exp_sqrt, 32);
    fe_sq(check, x);
    if (!fe_equal(check, u)) {
        fe_mul(x, x, cv->sqrt_m1);
        fe_sq(check, x);
        if (!fe_equal(check, u)) {
            return 0;
        }
    }
    /* x = 0 mit gesetztem Vorzeichenbit ist keine gueltige Kodierung */
    if (fe_is_zero(x) && sign) {
        return 0;
    }
    if ((int)(x[0] & 1u) != sign) {
        fe_neg(x, x);
    }
    fe_copy(out->x, x);
    fe_copy(out->y, y);
    fe_small(out->z, 1);
    fe_mul(out->t, x, y);
    return 1;
}

/* point_decode entpackt 32 Byte in einen Punkt. */
static int point_decode(point *out, const unsigned char in[32],
                        const curve *cv)
{
    unsigned char bytes[32];
    fe y;
    int sign;

    memcpy(bytes, in, 32);
    sign = (bytes[31] >> 7) & 1;
    bytes[31] &= 0x7f;
    if (!fe_from_bytes(y, bytes)) {
        return 0;   /* y nicht kanonisch */
    }
    return point_from_y(out, y, sign, cv);
}

/* curve_setup rechnet d, 2d und die Wurzel aus -1 aus. */
static void curve_setup(curve *cv)
{
    fe numerator, denominator, inverse, two;

    fe_small(cv->one, 1);
    fe_small(numerator, 121665);
    fe_neg(numerator, numerator);
    fe_small(denominator, 121666);
    fe_inverse(inverse, denominator);
    fe_mul(cv->d, numerator, inverse);
    fe_add(cv->two_d, cv->d, cv->d);

    fe_small(two, 2);
    fe_pow(cv->sqrt_m1, two, exp_sqrt_minus_one, 32);
}

/* curve_base liefert den Grundpunkt B: y = 4/5, x das gerade der beiden. */
static int curve_base(point *out, const curve *cv)
{
    fe four, five, inverse, y;

    fe_small(four, 4);
    fe_small(five, 5);
    fe_inverse(inverse, five);
    fe_mul(y, four, inverse);
    return point_from_y(out, y, 0, cv);
}

/* ------------------------------------------------------------- Skalare */

/* sc_less_than_l sagt, ob die 32 Byte (kleinstes zuerst) kleiner als L
 * sind. RFC 8032 verlangt das fuer S. */
static int sc_less_than_l(const unsigned char s[32])
{
    int i;
    uint64_t borrow = 0;

    for (i = 0; i < 8; i++) {
        uint32_t limb = (uint32_t)s[4 * i] |
                        ((uint32_t)s[4 * i + 1] << 8) |
                        ((uint32_t)s[4 * i + 2] << 16) |
                        ((uint32_t)s[4 * i + 3] << 24);
        uint64_t diff = (uint64_t)limb - (uint64_t)sc_l[i] - borrow;
        borrow = (diff >> 32) & 1u;
    }
    return borrow == 1;
}

/* sc_sub_l_if_ge zieht L ab, wenn der Wert nicht kleiner ist. */
static void sc_sub_l_if_ge(uint32_t r[8])
{
    uint32_t smaller[8];
    uint64_t borrow = 0;
    uint32_t mask;
    int i;

    for (i = 0; i < 8; i++) {
        uint64_t diff = (uint64_t)r[i] - (uint64_t)sc_l[i] - borrow;
        smaller[i] = (uint32_t)diff;
        borrow = (diff >> 32) & 1u;
    }
    mask = (uint32_t)borrow - 1u;
    for (i = 0; i < 8; i++) {
        r[i] = (r[i] & ~mask) | (smaller[i] & mask);
    }
}

/* sc_reduce rechnet eine 64-Byte-Zahl modulo L. Bit fuer Bit von oben:
 * verdoppeln, Bit anhaengen, bei Bedarf L abziehen. Das ist langsam und
 * dafuer offensichtlich richtig - und es laeuft nur einmal je Pruefung. */
static void sc_reduce(unsigned char out[32], const unsigned char in[64])
{
    uint32_t r[8];
    int bit, i;

    memset(r, 0, sizeof(r));
    for (bit = 511; bit >= 0; bit--) {
        uint32_t carry = (uint32_t)((in[bit >> 3] >> (bit & 7)) & 1);
        for (i = 0; i < 8; i++) {
            uint32_t next = r[i] >> 31;
            r[i] = (r[i] << 1) | carry;
            carry = next;
        }
        /* r bleibt kleiner als 2^254, darum faellt hier nichts heraus */
        sc_sub_l_if_ge(r);
    }
    for (i = 0; i < 8; i++) {
        out[4 * i] = (unsigned char)r[i];
        out[4 * i + 1] = (unsigned char)(r[i] >> 8);
        out[4 * i + 2] = (unsigned char)(r[i] >> 16);
        out[4 * i + 3] = (unsigned char)(r[i] >> 24);
    }
}

/* ------------------------------------------------------------- Pruefung */

int osk_ed25519_verify(const unsigned char public_key[OSK_ED25519_KEY_LEN],
                       const unsigned char *message, size_t message_len,
                       const unsigned char signature[OSK_ED25519_SIG_LEN])
{
    curve cv;
    point a_point, r_point, base, left, right;
    osk_sha512_ctx hash;
    unsigned char digest[OSK_SHA512_LEN];
    unsigned char k[32];

    if (public_key == NULL || signature == NULL) {
        return 0;
    }
    if (message_len > 0 && message == NULL) {
        return 0;
    }
    /* S muss kleiner als die Gruppenordnung sein (RFC 8032, 5.1.7) */
    if (!sc_less_than_l(signature + 32)) {
        return 0;
    }

    curve_setup(&cv);
    if (!curve_base(&base, &cv)) {
        return 0;
    }
    if (!point_decode(&a_point, public_key, &cv)) {
        return 0;
    }
    if (!point_decode(&r_point, signature, &cv)) {
        return 0;
    }

    /* k = SHA-512(R || A || Nachricht) mod L */
    osk_sha512_init(&hash);
    osk_sha512_update(&hash, signature, 32);
    osk_sha512_update(&hash, public_key, 32);
    if (message_len > 0) {
        osk_sha512_update(&hash, message, message_len);
    }
    osk_sha512_final(&hash, digest);
    sc_reduce(k, digest);

    /* [S]B muss gleich R + [k]A sein */
    point_mul(&left, signature + 32, &base, &cv);
    point_mul(&right, k, &a_point, &cv);
    point_add(&right, &right, &r_point, &cv);

    return point_equal(&left, &right);
}
