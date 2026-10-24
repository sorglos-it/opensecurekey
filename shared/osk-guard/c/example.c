/* Auf POSIX-Systemen muessen die Namen aus den Normen sichtbar gemacht
 * werden, bevor irgendein Kopf eingebunden wird - mit -std=c11 allein
 * kennt die Standardbibliothek weder nanosleep noch getaddrinfo. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

/* Beispiel in C: ein Programm, das nur mit Stick laeuft, eine Datei
 * verschliesst, die ohne Stick unlesbar bleibt, seine Lizenz selbst prueft
 * und einen Auftrag unterschreibt.
 *
 *     example -stand stand.json
 *
 * stand.json enthaelt address, deviceKey, issuerKey, product, label und
 * nodeKey. Jede Zeile, die mit "ergebnis: " anfaengt, liest der Pruefstand
 * mit. Der Ausstiegscode ist 0 nur bei Erfolg.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osk_guard.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <time.h>
#endif

#define MAX_STAND (64u << 10)

static void nap(unsigned millis)
{
#ifdef _WIN32
    Sleep(millis);
#else
    struct timespec wait;
    wait.tv_sec = millis / 1000;
    wait.tv_nsec = (long)(millis % 1000) * 1000000L;
    nanosleep(&wait, NULL);
#endif
}

static void complain(const char *what, const char *problem)
{
    fprintf(stderr, "FEHLER: %s", what);
    if (problem != NULL) {
        fprintf(stderr, ": %s", problem);
    }
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------- stand.json lesen */

/* Ein winziger Leser fuer genau diese Datei: flache Abbildung aus Texten.
 * Jede Laenge wird geprueft, bevor kopiert wird. */
static int stand_field(const char *text, const char *key, char *out,
                       size_t size)
{
    char pattern[32];
    size_t key_len = strlen(key);
    size_t i;
    size_t len = strlen(text);

    out[0] = '\0';
    if (key_len + 3 > sizeof(pattern)) {
        return 0;
    }
    pattern[0] = '"';
    memcpy(pattern + 1, key, key_len);
    pattern[key_len + 1] = '"';
    pattern[key_len + 2] = '\0';

    for (i = 0; i + key_len + 2 <= len; i++) {
        if (memcmp(text + i, pattern, key_len + 2) != 0) {
            continue;
        }
        i += key_len + 2;
        while (i < len && (text[i] == ' ' || text[i] == ':' ||
                           text[i] == '\t')) {
            i++;
        }
        if (i >= len || text[i] != '"') {
            return 0;
        }
        i++;
        {
            size_t from = i;
            while (i < len && text[i] != '"') {
                i++;
            }
            if (i >= len || i - from + 1 > size) {
                return 0;
            }
            memcpy(out, text + from, i - from);
            out[i - from] = '\0';
        }
        return 1;
    }
    return 0;
}

static char *read_whole_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *data;
    size_t got;

    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size <= 0 || (size_t)size > MAX_STAND ||
        fseek(file, 0, SEEK_SET) != 0) {
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
    return data;
}

static int hex_digit(char c)
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

/* hex_to_key liest genau 32 Byte aus einer Hex-Zeichenkette. */
static int hex_to_key(const char *text, unsigned char out[32])
{
    size_t i;

    if (strlen(text) != 64) {
        return 0;
    }
    for (i = 0; i < 32; i++) {
        int high = hex_digit(text[2 * i]);
        int low = hex_digit(text[2 * i + 1]);
        if (high < 0 || low < 0) {
            return 0;
        }
        out[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}

/* --------------------------------------------------------------- Ablauf */

typedef struct {
    char address[256];
    char product[128];
    char label[128];
    char node_key[128];
    unsigned char device_key[32];
    unsigned char issuer_key[32];
} stand;

static int load_stand(const char *path, stand *out)
{
    char *text = read_whole_file(path);
    char device_hex[128];
    char issuer_hex[128];
    int good = 1;

    if (text == NULL) {
        complain("der Pruefstand liess sich nicht lesen", path);
        return 0;
    }
    memset(out, 0, sizeof(*out));
    good &= stand_field(text, "address", out->address, sizeof(out->address));
    good &= stand_field(text, "product", out->product, sizeof(out->product));
    good &= stand_field(text, "label", out->label, sizeof(out->label));
    good &= stand_field(text, "nodeKey", out->node_key, sizeof(out->node_key));
    good &= stand_field(text, "deviceKey", device_hex, sizeof(device_hex));
    good &= stand_field(text, "issuerKey", issuer_hex, sizeof(issuer_hex));
    free(text);
    if (!good) {
        complain("im Pruefstand fehlt ein Feld", path);
        return 0;
    }
    if (!hex_to_key(device_hex, out->device_key)) {
        complain("deviceKey ist nicht 32 Byte in Hex", NULL);
        return 0;
    }
    if (!hex_to_key(issuer_hex, out->issuer_key)) {
        complain("issuerKey ist nicht 32 Byte in Hex", NULL);
        return 0;
    }
    return 1;
}

/* step_seal verschliesst etwas und macht es wieder auf - und zeigt, dass es
 * mit dem falschen Zusatz NICHT aufgeht. */
static int step_seal(osk_guard *guard, const stand *s)
{
    static const char plain[] = "die Tabelle, ohne die das Programm nichts kann";
    const unsigned char *version_one = (const unsigned char *)"version-1";
    const unsigned char *version_two = (const unsigned char *)"version-2";
    unsigned char *blob = NULL;
    unsigned char *back = NULL;
    unsigned char *wrong = NULL;
    size_t blob_len = 0, back_len = 0, wrong_len = 0;
    const char *problem;
    int good = 0;

    problem = osk_guard_seal(guard, s->label, version_one, 9,
                             (const unsigned char *)plain,
                             sizeof(plain) - 1, &blob, &blob_len);
    if (problem != NULL) {
        complain("verschliessen ging nicht", problem);
        return 0;
    }
    problem = osk_guard_unseal(guard, s->label, version_one, 9, blob,
                               blob_len, &back, &back_len);
    if (problem != NULL) {
        complain("oeffnen ging nicht", problem);
    } else if (back_len != sizeof(plain) - 1 ||
               memcmp(back, plain, back_len) != 0) {
        complain("heraus kam etwas anderes", NULL);
    } else {
        /* Mit der falschen Programmversion geht sie nicht auf - das muss so
         * sein, sonst waere der Zusatz wertlos. */
        problem = osk_guard_unseal(guard, s->label, version_two, 9, blob,
                                   blob_len, &wrong, &wrong_len);
        if (problem == NULL) {
            complain("mit falschem Zusatz aufgegangen", NULL);
        } else {
            printf("ergebnis: verschlossen %zu byte, wieder offen; mit "
                   "falschem zusatz abgewiesen (%s)\n", blob_len, problem);
            good = 1;
        }
    }
    if (back != NULL) {
        memset(back, 0, back_len);
    }
    free(blob);
    free(back);
    free(wrong);
    return good;
}

/* step_license liest die Lizenz. Die Werte kommen aus der selbst geprueften
 * Nutzlast, nicht aus etwas, das der Dienst dazugesagt hat. */
static int step_license(osk_guard *guard, const stand *s)
{
    osk_license license;
    const char *problem;
    size_t i;

    problem = osk_guard_license(guard, s->product, &license);
    if (problem != NULL) {
        complain("die Lizenz ging nicht durch", problem);
        return 0;
    }
    if (osk_guard_license_expired(&license, -1)) {
        complain("Lizenz abgelaufen", NULL);
        return 0;
    }
    if (!osk_guard_license_has_tier(&license, "berichte")) {
        complain("Ausbaustufe fehlt", NULL);
        return 0;
    }
    printf("ergebnis: lizenz %s, %u plaetze, stufen", license.product,
           (unsigned)license.seats);
    for (i = 0; i < license.tier_count; i++) {
        printf("%s %s", (i == 0) ? "" : ",", license.tiers[i]);
    }
    printf("\n");
    return 1;
}

/* step_job laesst einen Auftrag unterschreiben und prueft ihn - und zeigt,
 * dass ein veraendertes Byte auffaellt. */
static int step_job(osk_guard *guard, const stand *s)
{
    static const char job[] = "{\"tat\":\"abbild-holen\",\"wann\":1700000000}";
    unsigned char public_key[OSK_GUARD_KEY_LEN];
    unsigned char signature[OSK_GUARD_SIG_LEN];
    unsigned char bent[sizeof(job) - 1];
    const char *problem;

    problem = osk_guard_node_public(guard, s->node_key, "ed25519",
                                    public_key);
    if (problem != NULL) {
        complain("der oeffentliche Knotenschluessel kam nicht", problem);
        return 0;
    }
    problem = osk_guard_node_sign(guard, s->node_key,
                                  (const unsigned char *)job,
                                  sizeof(job) - 1, signature);
    if (problem != NULL) {
        complain("unterschreiben ging nicht", problem);
        return 0;
    }
    problem = osk_guard_node_verify(public_key, sizeof(public_key),
                                    s->node_key,
                                    (const unsigned char *)job,
                                    sizeof(job) - 1, signature,
                                    sizeof(signature));
    if (problem != NULL) {
        complain("die eigene Unterschrift galt nicht", problem);
        return 0;
    }
    memcpy(bent, job, sizeof(bent));
    bent[3] ^= 1;
    problem = osk_guard_node_verify(public_key, sizeof(public_key),
                                    s->node_key, bent, sizeof(bent),
                                    signature, sizeof(signature));
    if (problem == NULL) {
        complain("ein veraenderter Auftrag galt", NULL);
        return 0;
    }
    printf("ergebnis: auftrag unterschrieben und geprueft; veraendertes byte "
           "abgewiesen (%s)\n", problem);
    return 1;
}

/* step_agree ist der Schluesselaustausch - und der Fall, dass der Chip ihn
 * nicht kann. Dann wird der Anwendungsfall benannt, nicht abgebrochen. */
static int step_agree(osk_guard *guard, const stand *s)
{
    unsigned char peer[OSK_GUARD_KEY_LEN];
    unsigned char secret[OSK_GUARD_SECRET_LEN];
    const char *problem;

    problem = osk_guard_node_public(guard, s->node_key, "x25519", peer);
    if (problem == NULL) {
        problem = osk_guard_node_agree(guard, s->node_key, peer,
                                       sizeof(peer), secret);
        if (problem == NULL) {
            printf("ergebnis: gemeinsames geheimnis %zu byte\n",
                   sizeof(secret));
            memset(secret, 0, sizeof(secret));
            return 1;
        }
    }
    if (osk_guard_is_no_ecdh(problem)) {
        printf("ergebnis: kein schluesselaustausch - serverbetrieb mit "
               "schluessel im chip faellt weg\n");
        return 1;
    }
    complain("der Schluesselaustausch ging nicht", problem);
    return 0;
}

static int run(const char *path)
{
    stand s;
    osk_guard *guard = NULL;
    const char *problem;

    if (!load_stand(path, &s)) {
        return 0;
    }

    /* 1. Ohne Stick laeuft das Programm nicht los. */
    problem = osk_guard_open(s.address, s.device_key, sizeof(s.device_key),
                             s.issuer_key, sizeof(s.issuer_key), &guard);
    if (problem != NULL) {
        complain("kein Sicherheitsschluessel", problem);
        return 0;
    }
    problem = osk_guard_present(guard, NULL);
    if (problem != NULL) {
        complain("der Stick steckt nicht", problem);
        osk_guard_close(guard);
        return 0;
    }
    printf("ergebnis: praesenz ok\n");

    if (!step_seal(guard, &s) || !step_license(guard, &s) ||
        !step_job(guard, &s) || !step_agree(guard, &s)) {
        osk_guard_close(guard);
        return 0;
    }

    /* Weiterlaufen und beim Abziehen abbrechen - hier nur eine Runde, damit
     * das Beispiel von selbst endet. Ein geschuetztes Programm fragt in
     * festem Abstand weiter und beendet sich, sobald eine Runde scheitert. */
    nap(200);
    problem = osk_guard_present(guard, NULL);
    if (problem != NULL) {
        complain("abgebrochen", problem);
        osk_guard_close(guard);
        return 0;
    }
    printf("ergebnis: ueberwachung laeuft\n");

    osk_guard_close(guard);
    return 1;
}

int main(int argc, char **argv)
{
    const char *path = "stand.json";
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-stand") == 0 && i + 1 < argc) {
            path = argv[++i];
            continue;
        }
        fprintf(stderr, "Aufruf: %s -stand <pfad zu stand.json>\n", argv[0]);
        return 2;
    }
    return run(path) ? 0 : 1;
}
