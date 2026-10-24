/* Prueflauf der C-Fassung gegen die Vektoren in ..\tests\.
 *
 *   vectors.json          9 Hashwerte und 21 Unterschriften (5 gueltig)
 *   license-vectors.json  24 Lizenzen (3 gueltig, 21 abzuweisen)
 *
 * Alle muessen genau so beurteilt werden wie dort vermerkt. Bei den
 * gueltigen Lizenzen werden alle sieben Feldwerte verglichen, nicht nur
 * "kein Fehler" - sonst wuerde ein Leser durchkommen, der die Nutzlast gar
 * nicht liest.
 *
 * Dazu die Gegenprobe: ein Bit im Herstellerschluessel gedreht, dann muss
 * jede der drei gueltigen Lizenzen error.license.signature ergeben.
 *
 * Darum ist hier auch ein winziger JSON-Leser drin: er kann genau diese
 * zwei Dateien und nichts sonst. Wichtig ist der letzte Schritt - wurden
 * ueberhaupt alle Faelle gelesen? Null gelesene Faelle sind ein Fehler,
 * kein Erfolg.
 *
 *     make test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ed25519_verify.h"
#include "osk_guard.h"
#include "sha2.h"

#define WANT_HASH_CASES 9
#define WANT_VERIFY_CASES 21
#define WANT_VALID_CASES 5
#define WANT_INVALID_CASES 16

#define WANT_LICENSE_CASES 24
#define WANT_LICENSE_VALID 3
#define WANT_LICENSE_INVALID 21

#define MAX_FILE (4u << 20)

static int failures = 0;

static void report(const char *name, const char *what)
{
    printf("FEHLER: %s - %s\n", name, what);
    failures++;
}

/* ------------------------------------------------------------- Hilfsmittel */

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* hex_to_bytes legt die Rohbytes auf dem Haufen ab. Laenge 0 ist erlaubt
 * (die leere Nachricht und die leere Lizenz sind Faelle). */
static int hex_to_bytes(const char *text, unsigned char **out, size_t *out_len)
{
    size_t len = strlen(text);
    size_t i;
    unsigned char *raw;

    *out = NULL;
    *out_len = 0;
    if (len % 2 != 0) {
        return 0;
    }
    raw = (unsigned char *)malloc(len / 2 + 1);
    if (raw == NULL) {
        return 0;
    }
    for (i = 0; i < len / 2; i++) {
        int high = hex_value(text[2 * i]);
        int low = hex_value(text[2 * i + 1]);
        if (high < 0 || low < 0) {
            free(raw);
            return 0;
        }
        raw[i] = (unsigned char)((high << 4) | low);
    }
    *out = raw;
    *out_len = len / 2;
    return 1;
}

static void bytes_to_hex(const unsigned char *raw, size_t len, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < len; i++) {
        out[2 * i] = digits[raw[i] >> 4];
        out[2 * i + 1] = digits[raw[i] & 0x0fu];
    }
    out[2 * len] = '\0';
}

/* --------------------------------------------------------- JSON, nur hier */

/* find_block sucht ab from den naechsten Block in den Klammern open/close
 * und zaehlt die Verschachtelung mit - ein Lizenzfall enthaelt eine
 * Unterabbildung ("want"). */
static int find_block(const char *text, size_t from, size_t limit,
                      char open, char close, size_t *start, size_t *end)
{
    size_t i;

    for (i = from; i < limit; i++) {
        if (text[i] == open) {
            int depth = 0;
            size_t j;
            for (j = i; j < limit; j++) {
                if (text[j] == open) {
                    depth++;
                } else if (text[j] == close) {
                    depth--;
                    if (depth == 0) {
                        *start = i;
                        *end = j;
                        return 1;
                    }
                }
            }
            return 0;
        }
        if (text[i] == close) {
            return 0;   /* das Feld ist zu Ende */
        }
    }
    return 0;
}

/* key_at liefert die Stelle hinter "<key>" samt Doppelpunkt, oder 0. */
static size_t key_at(const char *text, size_t start, size_t end,
                     const char *key)
{
    char pattern[32];
    size_t key_len = strlen(key);
    size_t i;

    if (key_len + 3 > sizeof(pattern)) {
        return 0;
    }
    pattern[0] = '"';
    memcpy(pattern + 1, key, key_len);
    pattern[key_len + 1] = '"';
    pattern[key_len + 2] = '\0';

    for (i = start; i + key_len + 2 <= end; i++) {
        if (memcmp(text + i, pattern, key_len + 2) != 0) {
            continue;
        }
        i += key_len + 2;
        while (i < end && (text[i] == ' ' || text[i] == ':' ||
                           text[i] == '\t' || text[i] == '\n' ||
                           text[i] == '\r')) {
            i++;
        }
        return i;
    }
    return 0;
}

/* field_string holt den Textwert zu einem Schluessel aus einem Block.
 * Gibt einen Zeiger auf eine frisch angelegte Zeichenkette zurueck. */
static char *field_string(const char *text, size_t start, size_t end,
                          const char *key)
{
    size_t i = key_at(text, start, end, key);
    size_t from;
    char *out;

    if (i == 0 || i >= end || text[i] != '"') {
        return NULL;
    }
    from = ++i;
    while (i < end && text[i] != '"') {
        i++;
    }
    out = (char *)malloc(i - from + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, text + from, i - from);
    out[i - from] = '\0';
    return out;
}

/* field_bool liest true oder false. Gibt -1, wenn der Schluessel fehlt. */
static int field_bool(const char *text, size_t start, size_t end,
                      const char *key)
{
    size_t i = key_at(text, start, end, key);

    if (i == 0) {
        return -1;
    }
    if (i + 4 <= end && memcmp(text + i, "true", 4) == 0) {
        return 1;
    }
    if (i + 5 <= end && memcmp(text + i, "false", 5) == 0) {
        return 0;
    }
    return -1;
}

/* field_number liest eine nichtnegative Ganzzahl. Gibt 0 bei Fehlern. */
static int field_number(const char *text, size_t start, size_t end,
                        const char *key, long long *out)
{
    size_t i = key_at(text, start, end, key);
    long long value = 0;
    int digits = 0;

    if (i == 0) {
        return 0;
    }
    while (i < end && text[i] >= '0' && text[i] <= '9') {
        if (value > (9223372036854775807LL - (text[i] - '0')) / 10) {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        digits++;
        i++;
    }
    return (digits > 0) ? (*out = value, 1) : 0;
}

/* string_array liest ein Feld aus Texten in feste Felder. */
static int string_array(const char *text, size_t start, size_t end,
                        const char *key,
                        char out[OSK_GUARD_MAX_TIERS][OSK_GUARD_MAX_TIER],
                        size_t *count)
{
    size_t at = key_at(text, start, end, key);
    size_t list_start, list_end, i;

    *count = 0;
    if (at == 0) {
        return 1;   /* nicht vorhanden heisst: leer */
    }
    if (!find_block(text, at, end + 1, '[', ']', &list_start, &list_end)) {
        return 0;
    }
    i = list_start + 1;
    while (i < list_end) {
        size_t from;

        if (text[i] != '"') {
            i++;
            continue;
        }
        from = ++i;
        while (i < list_end && text[i] != '"') {
            i++;
        }
        if (*count >= OSK_GUARD_MAX_TIERS ||
            i - from + 1 > OSK_GUARD_MAX_TIER) {
            return 0;
        }
        memcpy(out[*count], text + from, i - from);
        out[*count][i - from] = '\0';
        (*count)++;
        i++;
    }
    return 1;
}

/* array_bounds findet Anfang und Ende eines benannten Feldes. */
static int array_bounds(const char *text, size_t len, const char *key,
                        size_t *start, size_t *end)
{
    size_t at = key_at(text, 0, len, key);
    size_t list_start, list_end;

    if (at == 0) {
        return 0;
    }
    if (!find_block(text, at, len, '[', ']', &list_start, &list_end)) {
        return 0;
    }
    *start = list_start + 1;
    *end = list_end;
    return 1;
}

static char *read_file(const char *path, size_t *len)
{
    FILE *file = fopen(path, "rb");
    char *data;
    size_t got;
    long size;

    *len = 0;
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size <= 0 || (size_t)size > MAX_FILE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    data = (char *)malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(file);
        return NULL;
    }
    got = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[got] = '\0';
    *len = got;
    return data;
}

/* ---------------------------------------------------------- Hashwerte */

static int run_hash_cases(const char *text, size_t len)
{
    size_t start, end, cursor;
    int count = 0;

    if (!array_bounds(text, len, "hash", &start, &end)) {
        printf("FEHLER: das Feld \"hash\" steht nicht in der Datei\n");
        failures++;
        return 0;
    }
    cursor = start;
    for (;;) {
        size_t object_start, object_end;
        char *input;
        char *want256;
        char *want512;
        unsigned char *raw = NULL;
        size_t raw_len = 0;
        unsigned char got256[OSK_SHA256_LEN];
        unsigned char got512[OSK_SHA512_LEN];
        char hex[2 * OSK_SHA512_LEN + 1];

        if (!find_block(text, cursor, end, '{', '}', &object_start,
                        &object_end)) {
            break;
        }
        cursor = object_end + 1;
        input = field_string(text, object_start, object_end, "input");
        want256 = field_string(text, object_start, object_end, "sha256");
        want512 = field_string(text, object_start, object_end, "sha512");
        if (input == NULL || want256 == NULL || want512 == NULL) {
            report("hash", "ein Feld fehlt");
            free(input);
            free(want256);
            free(want512);
            continue;
        }
        if (!hex_to_bytes(input, &raw, &raw_len)) {
            report("hash", "input ist kein Hex");
        } else {
            osk_sha256(raw, raw_len, got256);
            bytes_to_hex(got256, sizeof(got256), hex);
            if (strcmp(hex, want256) != 0) {
                report(input, "SHA-256 stimmt nicht");
            }
            osk_sha512(raw, raw_len, got512);
            bytes_to_hex(got512, sizeof(got512), hex);
            if (strcmp(hex, want512) != 0) {
                report(input, "SHA-512 stimmt nicht");
            }
            count++;
        }
        free(raw);
        free(input);
        free(want256);
        free(want512);
    }
    return count;
}

/* ------------------------------------------------------ Unterschriften */

static int run_verify_cases(const char *text, size_t len, int *valid_seen,
                            int *invalid_seen)
{
    size_t start, end, cursor;
    int count = 0;

    *valid_seen = 0;
    *invalid_seen = 0;
    if (!array_bounds(text, len, "verify", &start, &end)) {
        printf("FEHLER: das Feld \"verify\" steht nicht in der Datei\n");
        failures++;
        return 0;
    }
    cursor = start;
    for (;;) {
        size_t object_start, object_end;
        char *name;
        char *public_hex;
        char *message_hex;
        char *sig_hex;
        int want;
        unsigned char *public_key = NULL;
        unsigned char *message = NULL;
        unsigned char *signature = NULL;
        size_t public_len = 0, message_len = 0, signature_len = 0;

        if (!find_block(text, cursor, end, '{', '}', &object_start,
                        &object_end)) {
            break;
        }
        cursor = object_end + 1;
        name = field_string(text, object_start, object_end, "name");
        public_hex = field_string(text, object_start, object_end, "public");
        message_hex = field_string(text, object_start, object_end, "message");
        sig_hex = field_string(text, object_start, object_end, "sig");
        want = field_bool(text, object_start, object_end, "valid");

        if (name == NULL || public_hex == NULL || message_hex == NULL ||
            sig_hex == NULL || want < 0) {
            report("verify", "ein Feld fehlt");
        } else if (!hex_to_bytes(public_hex, &public_key, &public_len) ||
                   !hex_to_bytes(message_hex, &message, &message_len) ||
                   !hex_to_bytes(sig_hex, &signature, &signature_len)) {
            report(name, "ein Wert ist kein Hex");
        } else if (public_len != OSK_ED25519_KEY_LEN ||
                   signature_len != OSK_ED25519_SIG_LEN) {
            report(name,
                   "Schluessel oder Unterschrift haben die falsche Laenge");
        } else {
            int got = osk_ed25519_verify(public_key, message, message_len,
                                         signature);
            if (got != want) {
                report(name, want ? "haette gelten muessen"
                                  : "haette abgewiesen werden muessen");
            }
            if (want) {
                (*valid_seen)++;
            } else {
                (*invalid_seen)++;
            }
            count++;
        }
        free(public_key);
        free(message);
        free(signature);
        free(name);
        free(public_hex);
        free(message_hex);
        free(sig_hex);
    }
    return count;
}

/* ------------------------------------------------------------- Lizenzen */

/* check_wanted_fields vergleicht alle sieben Felder einer gueltigen Lizenz
 * gegen das, was in "want" steht. Nur "kein Fehler" wuerde einen Leser
 * durchlassen, der die Nutzlast gar nicht liest. */
static void check_wanted_fields(const char *text, size_t start, size_t end,
                                const char *name, const osk_license *got)
{
    size_t want_start, want_end;
    size_t at = key_at(text, start, end, "want");
    char *product;
    char *holder;
    char *number;
    char tiers[OSK_GUARD_MAX_TIERS][OSK_GUARD_MAX_TIER];
    size_t tier_count = 0;
    long long seats = -1, issued = -1, expires = -1;
    size_t i;

    if (at == 0 ||
        !find_block(text, at, end + 1, '{', '}', &want_start, &want_end)) {
        report(name, "der Block \"want\" fehlt");
        return;
    }
    product = field_string(text, want_start, want_end, "product");
    holder = field_string(text, want_start, want_end, "holder");
    number = field_string(text, want_start, want_end, "number");
    if (!field_number(text, want_start, want_end, "seats", &seats) ||
        !field_number(text, want_start, want_end, "issued", &issued) ||
        !field_number(text, want_start, want_end, "expires", &expires) ||
        !string_array(text, want_start, want_end, "tiers", tiers,
                      &tier_count)) {
        report(name, "der Block \"want\" liess sich nicht lesen");
        free(product);
        free(holder);
        free(number);
        return;
    }
    if (product == NULL || strcmp(got->product, product) != 0) {
        report(name, "das Produkt stimmt nicht");
    }
    if (holder == NULL || strcmp(got->holder, holder) != 0) {
        report(name, "der Lizenznehmer stimmt nicht");
    }
    if (number == NULL || strcmp(got->number, number) != 0) {
        report(name, "die Nummer stimmt nicht");
    }
    if ((long long)got->seats != seats) {
        report(name, "die Zahl der Plaetze stimmt nicht");
    }
    if ((long long)got->issued_utc != issued) {
        report(name, "das Ausstellungsdatum stimmt nicht");
    }
    if ((long long)got->expires_utc != expires) {
        report(name, "das Ablaufdatum stimmt nicht");
    }
    if (got->tier_count != tier_count) {
        report(name, "die Zahl der Ausbaustufen stimmt nicht");
    } else {
        for (i = 0; i < tier_count; i++) {
            if (strcmp(got->tiers[i], tiers[i]) != 0) {
                report(name, "eine Ausbaustufe stimmt nicht");
            }
        }
    }
    free(product);
    free(holder);
    free(number);
}

static int run_license_cases(const char *text, size_t len, int *valid_seen,
                             int *invalid_seen, int *bent_key_rejected)
{
    size_t start, end, cursor;
    int count = 0;
    char *issuer_hex;
    unsigned char *issuer = NULL;
    unsigned char bent[OSK_GUARD_KEY_LEN];
    size_t issuer_len = 0;

    *valid_seen = 0;
    *invalid_seen = 0;
    *bent_key_rejected = 0;

    issuer_hex = field_string(text, 0, len, "issuerPublicKey");
    if (issuer_hex == NULL ||
        !hex_to_bytes(issuer_hex, &issuer, &issuer_len) ||
        issuer_len != OSK_GUARD_KEY_LEN) {
        printf("FEHLER: issuerPublicKey fehlt oder ist nicht 32 Byte\n");
        failures++;
        free(issuer_hex);
        free(issuer);
        return 0;
    }
    free(issuer_hex);
    /* Die Gegenprobe: ein Bit im Herstellerschluessel gedreht. */
    memcpy(bent, issuer, OSK_GUARD_KEY_LEN);
    bent[0] ^= 1;

    if (!array_bounds(text, len, "cases", &start, &end)) {
        printf("FEHLER: das Feld \"cases\" steht nicht in der Datei\n");
        failures++;
        free(issuer);
        return 0;
    }
    cursor = start;
    for (;;) {
        size_t object_start, object_end;
        char *name;
        char *blob_hex;
        char *want_error;
        int want_valid;
        unsigned char *blob = NULL;
        size_t blob_len = 0;
        osk_license license;
        const char *problem;

        if (!find_block(text, cursor, end, '{', '}', &object_start,
                        &object_end)) {
            break;
        }
        cursor = object_end + 1;
        name = field_string(text, object_start, object_end, "name");
        blob_hex = field_string(text, object_start, object_end, "blob");
        want_error = field_string(text, object_start, object_end, "error");
        want_valid = field_bool(text, object_start, object_end, "valid");

        if (name == NULL || blob_hex == NULL || want_valid < 0) {
            report("license", "ein Feld fehlt");
        } else if (!hex_to_bytes(blob_hex, &blob, &blob_len)) {
            report(name, "blob ist kein Hex");
        } else {
            problem = osk_guard_license_check(blob, blob_len, issuer,
                                              OSK_GUARD_KEY_LEN, &license);
            if (want_valid) {
                if (problem != NULL) {
                    report(name, problem);
                } else {
                    check_wanted_fields(text, object_start, object_end, name,
                                        &license);
                }
                /* Mit verbogenem Herstellerschluessel darf keine davon
                 * durchgehen. */
                problem = osk_guard_license_check(blob, blob_len, bent,
                                                  OSK_GUARD_KEY_LEN,
                                                  &license);
                if (problem == NULL ||
                    strcmp(problem, "error.license.signature") != 0) {
                    report(name, "mit verbogenem Herstellerschluessel kam "
                                 "nicht error.license.signature");
                } else {
                    (*bent_key_rejected)++;
                }
                (*valid_seen)++;
            } else {
                if (problem == NULL) {
                    report(name, "haette abgewiesen werden muessen");
                } else if (want_error == NULL || want_error[0] == '\0') {
                    report(name, "im Fall fehlt der erwartete Fehler");
                } else if (strcmp(problem, want_error) != 0) {
                    printf("FEHLER: %s - erwartet %s, bekommen %s\n", name,
                           want_error, problem);
                    failures++;
                }
                (*invalid_seen)++;
            }
            count++;
        }
        free(blob);
        free(name);
        free(blob_hex);
        free(want_error);
    }
    free(issuer);
    return count;
}

/* ------------------------------------------------------------- Hauptlauf */

static void expect(int got, int want, const char *what)
{
    if (got != want) {
        printf("FEHLER: es wurden nicht genau %d %s gelesen (es waren %d)\n",
               want, what, got);
        failures++;
    }
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "../tests/vectors.json";
    const char *license_path = (argc > 2) ? argv[2]
                                          : "../tests/license-vectors.json";
    char *text;
    size_t len = 0;
    int hash_cases, verify_cases, valid_seen = 0, invalid_seen = 0;
    int license_cases, license_valid = 0, license_invalid = 0, bent = 0;

    text = read_file(path, &len);
    if (text == NULL) {
        printf("FEHLER: %s liess sich nicht lesen\n", path);
        return 1;
    }
    hash_cases = run_hash_cases(text, len);
    verify_cases = run_verify_cases(text, len, &valid_seen, &invalid_seen);
    free(text);

    text = read_file(license_path, &len);
    if (text == NULL) {
        printf("FEHLER: %s liess sich nicht lesen\n", license_path);
        return 1;
    }
    license_cases = run_license_cases(text, len, &license_valid,
                                      &license_invalid, &bent);
    free(text);

    printf("hashwerte geprueft: %d (erwartet %d)\n", hash_cases,
           WANT_HASH_CASES);
    printf("unterschriften geprueft: %d (erwartet %d), davon gueltig %d "
           "(erwartet %d) und ungueltig %d (erwartet %d)\n",
           verify_cases, WANT_VERIFY_CASES, valid_seen, WANT_VALID_CASES,
           invalid_seen, WANT_INVALID_CASES);
    printf("lizenzen geprueft: %d (erwartet %d), davon gueltig %d "
           "(erwartet %d) und abgewiesen %d (erwartet %d)\n",
           license_cases, WANT_LICENSE_CASES, license_valid,
           WANT_LICENSE_VALID, license_invalid, WANT_LICENSE_INVALID);
    printf("mit verbogenem herstellerschluessel abgewiesen: %d "
           "(erwartet %d)\n", bent, WANT_LICENSE_VALID);

    /* Null gelesene Faelle sind ein Fehler, kein Erfolg. */
    expect(hash_cases, WANT_HASH_CASES, "Hashwerte");
    expect(verify_cases, WANT_VERIFY_CASES, "Unterschriften");
    expect(valid_seen, WANT_VALID_CASES, "gueltige Unterschriften");
    expect(invalid_seen, WANT_INVALID_CASES, "ungueltige Unterschriften");
    expect(license_cases, WANT_LICENSE_CASES, "Lizenzen");
    expect(license_valid, WANT_LICENSE_VALID, "gueltige Lizenzen");
    expect(license_invalid, WANT_LICENSE_INVALID, "abzuweisende Lizenzen");
    expect(bent, WANT_LICENSE_VALID,
           "Lizenzen mit verbogenem Herstellerschluessel");

    if (failures > 0) {
        printf("%d Fehler\n", failures);
        return 1;
    }
    printf("alles gruen\n");
    return 0;
}
