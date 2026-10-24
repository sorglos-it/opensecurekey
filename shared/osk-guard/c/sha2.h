/* SHA-256 und SHA-512 nach FIPS 180-4, selbst geschrieben.
 *
 * Nur die Standardbibliothek. Zwei Wege je Verfahren: alles in einem Zug
 * (osk_sha256) oder stueckweise (init/update/final) fuer grosse Dateien,
 * die nicht in den Arbeitsspeicher sollen.
 */
#ifndef OSK_SHA2_H
#define OSK_SHA2_H

#include <stddef.h>
#include <stdint.h>

#define OSK_SHA256_LEN 32
#define OSK_SHA512_LEN 64

/* Zustand von SHA-256. Nicht von aussen anfassen. */
typedef struct {
    uint32_t state[8];
    uint64_t count;          /* Zahl der schon aufgenommenen Bytes */
    unsigned char block[64]; /* Rest, der noch keinen vollen Block fuellt */
    size_t used;
} osk_sha256_ctx;

/* Zustand von SHA-512. Nicht von aussen anfassen. */
typedef struct {
    uint64_t state[8];
    uint64_t count_lo; /* Bytes, 128-Bit-Zaehler in zwei Haelften */
    uint64_t count_hi;
    unsigned char block[128];
    size_t used;
} osk_sha512_ctx;

void osk_sha256_init(osk_sha256_ctx *ctx);
void osk_sha256_update(osk_sha256_ctx *ctx, const void *data, size_t len);
void osk_sha256_final(osk_sha256_ctx *ctx, unsigned char out[OSK_SHA256_LEN]);
void osk_sha256(const void *data, size_t len, unsigned char out[OSK_SHA256_LEN]);

void osk_sha512_init(osk_sha512_ctx *ctx);
void osk_sha512_update(osk_sha512_ctx *ctx, const void *data, size_t len);
void osk_sha512_final(osk_sha512_ctx *ctx, unsigned char out[OSK_SHA512_LEN]);
void osk_sha512(const void *data, size_t len, unsigned char out[OSK_SHA512_LEN]);

/* Ueberschreibt einen Speicherbereich, damit Geheimnisse nicht liegen
 * bleiben. Der Zeiger wird ueber volatile gefuehrt, damit der Uebersetzer
 * das Schreiben nicht wegwirft. */
void osk_wipe(void *data, size_t len);

#endif /* OSK_SHA2_H */
