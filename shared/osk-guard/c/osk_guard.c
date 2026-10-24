/* Auf POSIX-Systemen muessen die Namen aus den Normen sichtbar gemacht
 * werden, bevor irgendein Kopf eingebunden wird - mit -std=c11 allein
 * kennt die Standardbibliothek weder nanosleep noch getaddrinfo. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

/* osk-guard fuer C - der Klient zum lokalen Dienst.
 *
 * Das Drahtprotokoll ist absichtlich klein: TCP auf der Rueckschleife, eine
 * Zeile hin, eine Zeile zurueck, jeder Wert in Hex mit Kleinbuchstaben. Ein
 * Bindestrich heisst "nicht gesetzt". Mehr als 4 MiB je Zeile gibt es nicht.
 *
 * Je Anfrage eine eigene Verbindung: das ist langsamer, aber ein Programm
 * kann so nie die Antwort einer fremden Anfrage lesen.
 *
 * Nur die Standardbibliothek und die Sockets des Systems.
 */
#include "osk_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ed25519_verify.h"
#include "sha2.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <bcrypt.h>
typedef SOCKET osk_socket;
#  define OSK_BAD_SOCKET INVALID_SOCKET
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
typedef int osk_socket;
#  define OSK_BAD_SOCKET (-1)
#endif

/* Die Zwecke, die das Geraet selbst vor die Nachricht setzt. Dadurch sind
 * eine Auftragsunterschrift und eine Praesenzantwort nie dieselben Bytes. */
static const char attest_domain[] = "osk-guard/attest/1";
static const char sign_domain[] = "osk-guard/sign/1";

#define MAX_LINE (4u << 20)   /* Obergrenze einer Protokollzeile */
#define MAX_BINARY (512u << 20)
#define MAX_FIELDS 16
#define MAX_LABEL 128
#define READ_TIMEOUT_MS 30000
#define CONNECT_TIMEOUT_MS 3000

/* Fehlerschluessel, die hier entstehen. Sie gehen unveraendert hinaus. */
static const char err_key[] = "error.guard.key";
static const char err_request[] = "error.guard.request";
static const char err_nodevice[] = "error.guard.nodevice";
static const char err_answer[] = "error.guard.answer";
static const char err_internal[] = "error.guard.internal";
static const char err_signature[] = "error.guard.signature";
static const char err_wrongdevice[] = "error.guard.wrongdevice";
static const char err_random[] = "error.guard.random";
static const char err_binary[] = "error.guard.binary";
static const char err_size[] = "error.guard.size";
static const char err_label[] = "error.guard.label";
static const char err_peerkey[] = "error.guard.peerkey";
static const char err_noecdh[] = "error.guard.noecdh";
static const char err_issuer[] = "error.guard.issuer";
static const char err_lic_format[] = "error.license.format";
static const char err_lic_version[] = "error.license.version";
static const char err_lic_signature[] = "error.license.signature";
static const char err_lic_size[] = "error.license.size";
static const char err_lic_product[] = "error.license.product";
static const char err_lic_tier[] = "error.license.tier";

/* Ein Schluessel, den der Dienst geschickt hat, braucht einen Platz, der
 * den Aufruf ueberlebt. Je Faden einer - so stoeren sich zwei Faeden nicht. */
static _Thread_local char wire_code[96];

struct osk_guard {
    char address[256];
    unsigned char device_public[OSK_GUARD_KEY_LEN];
    unsigned char issuer_public[OSK_GUARD_KEY_LEN];
    int has_issuer;
};

/* Kopf des Lizenzblocks: "OSKLIC1" | Fassung(1) | Laenge(2, groesstes Byte
 * zuerst) | Nutzlast | Unterschrift(64). */
static const char license_magic[] = "OSKLIC1";
#define LICENSE_MAGIC_LEN 7
#define LICENSE_HEAD_LEN (LICENSE_MAGIC_LEN + 1 + 2)
#define LICENSE_VERSION 1
#define LICENSE_MAX_PAYLOAD 4096

/* ------------------------------------------------------------ Hilfsmittel */

/* grow_buffer ist der einzige Ort, an dem Nutzdaten Speicher bekommen.
 * Daten unbekannter Laenge liegen nie in einem festen Puffer. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} text_buffer;

static void buffer_free(text_buffer *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static int buffer_reserve(text_buffer *buf, size_t extra)
{
    size_t want;
    char *bigger;

    if (buf->failed) {
        return 0;
    }
    if (extra > MAX_LINE || buf->len > MAX_LINE - extra) {
        buf->failed = 1;
        return 0;
    }
    want = buf->len + extra + 1;
    if (want <= buf->cap) {
        return 1;
    }
    while (buf->cap < want) {
        buf->cap = (buf->cap == 0) ? 256 : buf->cap * 2;
        if (buf->cap > MAX_LINE + 2) {
            buf->cap = MAX_LINE + 2;
            break;
        }
    }
    bigger = (char *)realloc(buf->data, buf->cap);
    if (bigger == NULL) {
        buf->failed = 1;
        return 0;
    }
    buf->data = bigger;
    return 1;
}

static int buffer_add(text_buffer *buf, const char *text, size_t len)
{
    if (!buffer_reserve(buf, len)) {
        return 0;
    }
    memcpy(buf->data + buf->len, text, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return 1;
}

static int buffer_add_text(text_buffer *buf, const char *text)
{
    return buffer_add(buf, text, strlen(text));
}

static const char hex_digits[] = "0123456789abcdef";

/* buffer_add_hex haengt Rohbytes als Hex an. */
static int buffer_add_hex(text_buffer *buf, const unsigned char *raw,
                          size_t len)
{
    size_t i;

    if (!buffer_reserve(buf, 2 * len)) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        buf->data[buf->len++] = hex_digits[raw[i] >> 4];
        buf->data[buf->len++] = hex_digits[raw[i] & 0x0fu];
    }
    buf->data[buf->len] = '\0';
    return 1;
}

/* buffer_add_hex_or_dash schreibt einen Bindestrich fuer "nicht gesetzt". */
static int buffer_add_hex_or_dash(text_buffer *buf, const unsigned char *raw,
                                  size_t len)
{
    if (raw == NULL || len == 0) {
        return buffer_add_text(buf, "-");
    }
    return buffer_add_hex(buf, raw, len);
}

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

/* hex_decode legt die Rohbytes auf dem Haufen ab. want > 0 verlangt genau
 * diese Laenge. */
static const char *hex_decode(const char *text, size_t want,
                              unsigned char **out, size_t *out_len)
{
    size_t text_len = strlen(text);
    size_t bytes;
    unsigned char *raw;
    size_t i;

    *out = NULL;
    *out_len = 0;
    if (text_len % 2 != 0) {
        return err_answer;
    }
    bytes = text_len / 2;
    if (want > 0 && bytes != want) {
        return err_answer;
    }
    if (bytes > MAX_LINE / 2) {
        return err_size;
    }
    raw = (unsigned char *)malloc(bytes > 0 ? bytes : 1);
    if (raw == NULL) {
        return err_internal;
    }
    for (i = 0; i < bytes; i++) {
        int high = hex_value(text[2 * i]);
        int low = hex_value(text[2 * i + 1]);
        if (high < 0 || low < 0) {
            osk_wipe(raw, bytes);
            free(raw);
            return err_answer;
        }
        raw[i] = (unsigned char)((high << 4) | low);
    }
    *out = raw;
    *out_len = bytes;
    return NULL;
}

/* equal_bytes vergleicht ohne Verzweigung je Byte. */
static int equal_bytes(const unsigned char *a, const unsigned char *b,
                       size_t len)
{
    unsigned char diff = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/* check_label prueft einen Namen: nicht leer, nicht zu lang, keine
 * Steuerzeichen und kein Leerraum - beides wuerde die Protokollzeile
 * zerlegen. */
static const char *check_label(const char *label)
{
    size_t i;

    if (label == NULL || label[0] == '\0') {
        return err_label;
    }
    for (i = 0; label[i] != '\0'; i++) {
        unsigned char c = (unsigned char)label[i];
        if (i >= MAX_LABEL) {
            return err_label;
        }
        if (c < 0x20 || c == 0x7f) {
            return err_label;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------- Zufall */

static const char *fill_random(unsigned char *out, size_t len)
{
#ifdef _WIN32
    if (BCryptGenRandom(NULL, out, (ULONG)len,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return err_random;
    }
    return NULL;
#else
    FILE *source = fopen("/dev/urandom", "rb");
    size_t got;

    if (source == NULL) {
        return err_random;
    }
    got = fread(out, 1, len, source);
    fclose(source);
    if (got != len) {
        return err_random;
    }
    return NULL;
#endif
}

/* ------------------------------------------------------------- Sockets */

#ifdef _WIN32
/* Winsock muss einmal je Prozess angemeldet werden. */
static int winsock_ready = 0;

static const char *sockets_start(void)
{
    WSADATA info;

    if (winsock_ready) {
        return NULL;
    }
    if (WSAStartup(MAKEWORD(2, 2), &info) != 0) {
        return err_nodevice;
    }
    winsock_ready = 1;
    return NULL;
}

static void socket_close(osk_socket sock)
{
    closesocket(sock);
}
#else
static const char *sockets_start(void)
{
    return NULL;
}

static void socket_close(osk_socket sock)
{
    close(sock);
}
#endif

static void socket_set_timeouts(osk_socket sock)
{
#ifdef _WIN32
    DWORD millis = READ_TIMEOUT_MS;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&millis,
               sizeof(millis));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&millis,
               sizeof(millis));
#else
    struct timeval limit;
    limit.tv_sec = READ_TIMEOUT_MS / 1000;
    limit.tv_usec = (READ_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof(limit));
#endif
}

/* address_is_loopback sagt, ob eine aufgeloeste Adresse auf diesem Rechner
 * liegt. Der Stick steckt hier, nicht im Netz - alles andere wird
 * abgewiesen. */
static int address_is_loopback(const struct addrinfo *info)
{
    if (info->ai_family == AF_INET) {
        const struct sockaddr_in *v4 = (const struct sockaddr_in *)
            (const void *)info->ai_addr;
        uint32_t host = ntohl(v4->sin_addr.s_addr);
        return (host >> 24) == 127u;
    }
    if (info->ai_family == AF_INET6) {
        const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)
            (const void *)info->ai_addr;
        static const unsigned char one[16] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
        };
        return memcmp(&v6->sin6_addr, one, 16) == 0;
    }
    return 0;
}

/* split_address trennt Rechner und Port. Die letzte Doppelpunktstelle
 * zaehlt, damit IPv6 in Klammern funktioniert. */
static const char *split_address(const char *address, char *host,
                                 size_t host_size, char *port,
                                 size_t port_size)
{
    const char *colon;
    size_t host_len;
    size_t port_len;

    if (address == NULL) {
        return err_request;
    }
    colon = strrchr(address, ':');
    if (colon == NULL || colon == address || colon[1] == '\0') {
        return err_request;
    }
    host_len = (size_t)(colon - address);
    port_len = strlen(colon + 1);
    if (host_len + 1 > host_size || port_len + 1 > port_size) {
        return err_request;
    }
    memcpy(host, address, host_len);
    host[host_len] = '\0';
    memcpy(port, colon + 1, port_len + 1);
    /* Klammern um eine IPv6-Adresse abstreifen */
    if (host_len >= 2 && host[0] == '[' && host[host_len - 1] == ']') {
        memmove(host, host + 1, host_len - 2);
        host[host_len - 2] = '\0';
    }
    if (port[strspn(port, "0123456789")] != '\0') {
        return err_request;
    }
    return NULL;
}

/* resolve_loopback loest die Adresse auf und gibt nur Ergebnisse auf der
 * Rueckschleife heraus. */
static const char *resolve_loopback(const char *address,
                                    struct addrinfo **out)
{
    char host[200];
    char port[16];
    struct addrinfo hints;
    struct addrinfo *list = NULL;
    struct addrinfo *walk;
    const char *problem;

    *out = NULL;
    problem = split_address(address, host, sizeof(host), port, sizeof(port));
    if (problem != NULL) {
        return problem;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;
    if (getaddrinfo(host, port, &hints, &list) != 0 || list == NULL) {
        return err_request;
    }
    for (walk = list; walk != NULL; walk = walk->ai_next) {
        if (address_is_loopback(walk)) {
            *out = list;
            return NULL;
        }
    }
    freeaddrinfo(list);
    return err_request;
}

/* send_all schickt den ganzen Puffer, auch wenn das System ihn stueckelt. */
static int send_all(osk_socket sock, const char *data, size_t len)
{
    while (len > 0) {
        size_t chunk = (len > 65536u) ? 65536u : len;
#ifdef _WIN32
        int sent = send(sock, data, (int)chunk, 0);
#else
        ssize_t sent = send(sock, data, chunk, 0);
#endif
        if (sent <= 0) {
            return 0;
        }
        data += (size_t)sent;
        len -= (size_t)sent;
    }
    return 1;
}

/* ------------------------------------------------------------- Antwort */

typedef struct {
    char *line;
    const char *names[MAX_FIELDS];
    const char *values[MAX_FIELDS];
    size_t count;
} reply;

static void reply_free(reply *answer)
{
    free(answer->line);
    answer->line = NULL;
    answer->count = 0;
}

/* reply_value sucht einen Wert. Fehlt er oder steht ein Bindestrich da,
 * kommt NULL heraus. */
static const char *reply_value(const reply *answer, const char *name)
{
    size_t i;

    for (i = 0; i < answer->count; i++) {
        if (strcmp(answer->names[i], name) == 0) {
            if (answer->values[i][0] == '-' && answer->values[i][1] == '\0') {
                return NULL;
            }
            return answer->values[i];
        }
    }
    return NULL;
}

/* read_line liest bis zum Zeilenende, mit Obergrenze. */
static const char *read_line(osk_socket sock, char **out)
{
    text_buffer buf;
    char chunk[4096];

    memset(&buf, 0, sizeof(buf));
    *out = NULL;
    for (;;) {
#ifdef _WIN32
        int got = recv(sock, chunk, (int)sizeof(chunk), 0);
#else
        ssize_t got = recv(sock, chunk, sizeof(chunk), 0);
#endif
        char *newline;
        size_t take;

        if (got <= 0) {
            buffer_free(&buf);
            return err_nodevice;
        }
        newline = (char *)memchr(chunk, '\n', (size_t)got);
        take = (newline != NULL) ? (size_t)(newline - chunk) : (size_t)got;
        if (!buffer_add(&buf, chunk, take)) {
            buffer_free(&buf);
            return err_size;
        }
        if (newline != NULL) {
            break;
        }
    }
    if (buf.data == NULL && !buffer_add(&buf, "", 0)) {
        buffer_free(&buf);
        return err_internal;
    }
    /* ein Wagenruecklauf vor dem Zeilenende gehoert nicht zum Inhalt */
    while (buf.len > 0 && buf.data[buf.len - 1] == '\r') {
        buf.data[--buf.len] = '\0';
    }
    *out = buf.data;
    return NULL;
}

/* parse_reply zerlegt die Zeile an Ort und Stelle in Name-Wert-Paare. */
static const char *parse_reply(char *line, reply *answer)
{
    char *cursor = line;
    char *token;
    int first = 1;

    answer->line = line;
    answer->count = 0;
    for (;;) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        token = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
        if (first) {
            first = 0;
            if (strcmp(token, "ERR") == 0) {
                /* der Schluessel des Dienstes geht unveraendert hinaus */
                while (*cursor == ' ') {
                    cursor++;
                }
                token = cursor;
                while (*cursor != '\0' && *cursor != ' ') {
                    cursor++;
                }
                *cursor = '\0';
                if (token[0] == '\0' || strlen(token) >= sizeof(wire_code)) {
                    return err_internal;
                }
                memcpy(wire_code, token, strlen(token) + 1);
                return wire_code;
            }
            if (strcmp(token, "OK") != 0) {
                return err_answer;
            }
            continue;
        }
        if (answer->count < MAX_FIELDS) {
            char *split = strchr(token, '=');
            if (split != NULL) {
                *split = '\0';
                answer->names[answer->count] = token;
                answer->values[answer->count] = split + 1;
                answer->count++;
            }
        }
    }
    if (first) {
        return err_answer;   /* leere Zeile */
    }
    return NULL;
}

/* ask schickt eine Zeile und liest die Antwort. */
static const char *ask(osk_guard *guard, const char *request, reply *answer)
{
    struct addrinfo *list = NULL;
    struct addrinfo *walk;
    osk_socket sock = OSK_BAD_SOCKET;
    const char *problem;
    char *line = NULL;

    memset(answer, 0, sizeof(*answer));
    problem = sockets_start();
    if (problem != NULL) {
        return problem;
    }
    problem = resolve_loopback(guard->address, &list);
    if (problem != NULL) {
        return problem;
    }
    for (walk = list; walk != NULL; walk = walk->ai_next) {
        sock = socket(walk->ai_family, walk->ai_socktype, walk->ai_protocol);
        if (sock == OSK_BAD_SOCKET) {
            continue;
        }
        socket_set_timeouts(sock);
        if (connect(sock, walk->ai_addr, (int)walk->ai_addrlen) == 0) {
            break;
        }
        socket_close(sock);
        sock = OSK_BAD_SOCKET;
    }
    freeaddrinfo(list);
    if (sock == OSK_BAD_SOCKET) {
        return err_nodevice;
    }
    if (!send_all(sock, request, strlen(request)) ||
        !send_all(sock, "\n", 1)) {
        socket_close(sock);
        return err_nodevice;
    }
    problem = read_line(sock, &line);
    socket_close(sock);
    if (problem != NULL) {
        return problem;
    }
    problem = parse_reply(line, answer);
    if (problem != NULL) {
        reply_free(answer);
        return problem;
    }
    return NULL;
}

/* ask_raw ist der Weg fuer einen Wert bekannter Laenge. */
static const char *ask_value(osk_guard *guard, const char *request,
                             const char *name, size_t want,
                             unsigned char **out, size_t *out_len)
{
    reply answer;
    const char *problem;
    const char *text;

    *out = NULL;
    *out_len = 0;
    problem = ask(guard, request, &answer);
    if (problem != NULL) {
        return problem;
    }
    text = reply_value(&answer, name);
    if (text == NULL) {
        reply_free(&answer);
        return err_answer;
    }
    problem = hex_decode(text, want, out, out_len);
    reply_free(&answer);
    return problem;
}

/* ------------------------------------------------------- Programmdatei */

/* own_path liefert den Pfad der laufenden Datei. */
static const char *own_path(char *out, size_t size)
{
#ifdef _WIN32
    DWORD got = GetModuleFileNameA(NULL, out, (DWORD)size);
    if (got == 0 || got >= size) {
        return err_binary;
    }
    return NULL;
#else
    ssize_t got = readlink("/proc/self/exe", out, size - 1);
    if (got <= 0 || (size_t)got >= size - 1) {
        return err_binary;
    }
    out[got] = '\0';
    return NULL;
#endif
}

const char *osk_guard_binary_hash(const char *path,
                                  unsigned char out[OSK_GUARD_HASH_LEN])
{
    char own[4096];
    FILE *file;
    osk_sha256_ctx digest;
    unsigned char chunk[65536];
    uint64_t total = 0;

    if (out == NULL) {
        return err_binary;
    }
    if (path == NULL || path[0] == '\0') {
        const char *problem = own_path(own, sizeof(own));
        if (problem != NULL) {
            return problem;
        }
        path = own;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return err_binary;
    }
    osk_sha256_init(&digest);
    for (;;) {
        size_t got = fread(chunk, 1, sizeof(chunk), file);
        if (got == 0) {
            break;
        }
        total += (uint64_t)got;
        if (total > MAX_BINARY) {
            fclose(file);
            return err_binary;
        }
        osk_sha256_update(&digest, chunk, got);
    }
    if (ferror(file)) {
        fclose(file);
        return err_binary;
    }
    fclose(file);
    osk_sha256_final(&digest, out);
    return NULL;
}

/* ------------------------------------------------------------- Oeffnen */

const char *osk_guard_open(const char *address,
                           const unsigned char *device_public_key,
                           size_t key_len,
                           const unsigned char *issuer_public_key,
                           size_t issuer_len, osk_guard **out)
{
    osk_guard *guard;
    reply answer;
    const char *problem;

    if (out == NULL) {
        return err_request;
    }
    *out = NULL;
    if (device_public_key == NULL || key_len != OSK_GUARD_KEY_LEN) {
        return err_key;
    }
    /* Der Herstellerschluessel darf fehlen - dann gibt es nur keine
     * Lizenzpruefung. Eine falsche Laenge ist dagegen ein Fehler. */
    if (issuer_public_key != NULL && issuer_len != 0 &&
        issuer_len != OSK_GUARD_KEY_LEN) {
        return err_key;
    }
    if (address == NULL || strlen(address) + 1 > sizeof(guard->address)) {
        return err_request;
    }
    guard = (osk_guard *)calloc(1, sizeof(*guard));
    if (guard == NULL) {
        return err_internal;
    }
    memcpy(guard->address, address, strlen(address) + 1);
    memcpy(guard->device_public, device_public_key, OSK_GUARD_KEY_LEN);
    if (issuer_public_key != NULL && issuer_len == OSK_GUARD_KEY_LEN) {
        memcpy(guard->issuer_public, issuer_public_key, OSK_GUARD_KEY_LEN);
        guard->has_issuer = 1;
    }

    problem = ask(guard, "ping", &answer);
    if (problem != NULL) {
        osk_guard_close(guard);
        return problem;
    }
    reply_free(&answer);
    *out = guard;
    return NULL;
}

void osk_guard_close(osk_guard *guard)
{
    if (guard == NULL) {
        return;
    }
    osk_wipe(guard, sizeof(*guard));
    free(guard);
}

int osk_guard_is_no_ecdh(const char *problem)
{
    return problem != NULL && strcmp(problem, err_noecdh) == 0;
}

/* ------------------------------------------------------------- Praesenz */

/* build_attest_message baut die Bytefolge, die das Geraet unterschreibt:
 * Zweck, Nulltrenner, Zufallswert, Pruefsumme der Programmdatei. */
static void build_attest_message(unsigned char *out,
                                 const unsigned char *nonce,
                                 const unsigned char *bin_hash)
{
    size_t domain_len = sizeof(attest_domain) - 1;

    memcpy(out, attest_domain, domain_len);
    out[domain_len] = 0;
    memcpy(out + domain_len + 1, nonce, OSK_GUARD_NONCE_LEN);
    memcpy(out + domain_len + 1 + OSK_GUARD_NONCE_LEN, bin_hash,
           OSK_GUARD_HASH_LEN);
}

const char *osk_guard_present(osk_guard *guard, const char *binary_path)
{
    unsigned char bin_hash[OSK_GUARD_HASH_LEN];
    unsigned char nonce[OSK_GUARD_NONCE_LEN];
    unsigned char signed_message[sizeof(attest_domain) + OSK_GUARD_NONCE_LEN +
                                 OSK_GUARD_HASH_LEN];
    text_buffer request;
    reply answer;
    const char *problem;
    const char *text;
    unsigned char *public_key = NULL;
    unsigned char *signature = NULL;
    size_t public_len = 0;
    size_t signature_len = 0;
    int good;

    if (guard == NULL) {
        return err_request;
    }
    problem = osk_guard_binary_hash(binary_path, bin_hash);
    if (problem != NULL) {
        return problem;
    }
    problem = fill_random(nonce, sizeof(nonce));
    if (problem != NULL) {
        return problem;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, "attest ");
    buffer_add_hex(&request, nonce, sizeof(nonce));
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, bin_hash, sizeof(bin_hash));
    if (request.failed) {
        buffer_free(&request);
        return err_internal;
    }
    problem = ask(guard, request.data, &answer);
    buffer_free(&request);
    if (problem != NULL) {
        return problem;
    }

    text = reply_value(&answer, "pub");
    if (text == NULL) {
        reply_free(&answer);
        return err_answer;
    }
    problem = hex_decode(text, OSK_GUARD_KEY_LEN, &public_key, &public_len);
    if (problem != NULL) {
        reply_free(&answer);
        return problem;
    }
    text = reply_value(&answer, "sig");
    if (text == NULL) {
        free(public_key);
        reply_free(&answer);
        return err_answer;
    }
    problem = hex_decode(text, OSK_GUARD_SIG_LEN, &signature,
                         &signature_len);
    reply_free(&answer);
    if (problem != NULL) {
        free(public_key);
        return problem;
    }

    /* Erst: ist es ueberhaupt der Stick, auf den das Programm gebaut ist? */
    good = equal_bytes(public_key, guard->device_public, OSK_GUARD_KEY_LEN);
    free(public_key);
    if (!good) {
        free(signature);
        return err_wrongdevice;
    }
    /* Dann: stimmt die Unterschrift ueber genau unsere Frage? Geprueft wird
     * hier, nicht beim Dienst. */
    build_attest_message(signed_message, nonce, bin_hash);
    good = osk_ed25519_verify(guard->device_public, signed_message,
                              sizeof(signed_message), signature);
    free(signature);
    if (!good) {
        return err_signature;
    }
    return NULL;
}

/* ------------------------------------------------------- Seal und Unseal */

/* seal_call fasst seal und unseal zusammen - der einzige Unterschied sind
 * das Verb und der Name des Wertes in der Antwort. */
static const char *seal_call(osk_guard *guard, const char *verb,
                             const char *result_name, const char *label,
                             const unsigned char *extra, size_t extra_len,
                             const unsigned char *body, size_t body_len,
                             unsigned char **out, size_t *out_len)
{
    text_buffer request;
    const char *problem;

    if (out == NULL || out_len == NULL) {
        return err_request;
    }
    *out = NULL;
    *out_len = 0;
    if (guard == NULL || body == NULL) {
        return err_request;
    }
    problem = check_label(label);
    if (problem != NULL) {
        return problem;
    }
    if (body_len == 0 || body_len > OSK_GUARD_MAX_PLAIN) {
        return err_size;
    }
    if (extra_len > OSK_GUARD_MAX_PLAIN) {
        return err_size;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, verb);
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, (const unsigned char *)label, strlen(label));
    buffer_add_text(&request, " ");
    buffer_add_hex_or_dash(&request, extra, extra_len);
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, body, body_len);
    if (request.failed) {
        buffer_free(&request);
        return err_size;
    }
    problem = ask_value(guard, request.data, result_name, 0, out, out_len);
    /* die Anfrage enthaelt Nutzdaten - sie wird ueberschrieben */
    osk_wipe(request.data, request.len);
    buffer_free(&request);
    return problem;
}

const char *osk_guard_seal(osk_guard *guard, const char *label,
                           const unsigned char *extra, size_t extra_len,
                           const unsigned char *plain, size_t plain_len,
                           unsigned char **blob, size_t *blob_len)
{
    return seal_call(guard, "seal", "blob", label, extra, extra_len,
                     plain, plain_len, blob, blob_len);
}

const char *osk_guard_unseal(osk_guard *guard, const char *label,
                             const unsigned char *extra, size_t extra_len,
                             const unsigned char *blob, size_t blob_len,
                             unsigned char **plain, size_t *plain_len)
{
    /* der verschlossene Block ist laenger als die Nutzdaten: Kopf und
     * Pruefsumme kommen dazu */
    if (blob_len > OSK_GUARD_MAX_PLAIN + 64) {
        return err_size;
    }
    return seal_call(guard, "unseal", "plain", label, extra, extra_len,
                     blob, blob_len, plain, plain_len);
}

/* ------------------------------------------------------------- Lizenz */

/* Der Dienst liefert nur den unterschriebenen Block. Geprueft wird hier:
 * sonst muesste die Bibliothek dem Dienst glauben - genau das, was bei der
 * Praesenz ausdruecklich nicht erwuenscht ist. Eine falsche Attrappe auf der
 * Rueckschleife kann so keine Lizenz erfinden. */

/* Ein winziger CBOR-Leser, nur fuer diese Nutzlast: vorzeichenlose Zahl
 * (Hauptsorte 0), Text (3), Feld (4) und Abbildung (5), und von den
 * Laengenformen nur "unter 24", "ein Byte" und "zwei Byte". Alles andere
 * wird abgewiesen, nicht geraten. Jede Laenge wird gegen das Pufferende
 * geprueft, bevor gelesen wird. */
typedef struct {
    const unsigned char *data;
    size_t len;
    size_t pos;
} cbor_reader;

#define CBOR_UNSIGNED 0
#define CBOR_TEXT 3
#define CBOR_ARRAY 4
#define CBOR_MAP 5

/* cbor_head liest Hauptsorte und Beiwert eines Kopfes.
 *
 * Die niedrigen fuenf Bit sagen, wie der Beiwert kommt: unter 24 ist er die
 * Zahl selbst, 24 bis 27 heissen 1, 2, 4 oder 8 folgende Byte mit dem
 * groessten zuerst, 28 bis 31 sind hier nicht erlaubt.
 *
 * Jede laengere Form muss noetig sein - sonst gaebe es zwei Schreibweisen
 * derselben Zahl, und das darf es in unterschriebenen Daten nicht geben. */
static int cbor_head(cbor_reader *in, unsigned *major, uint64_t *argument)
{
    unsigned char first;
    unsigned info;
    unsigned extra;
    uint64_t value = 0;
    uint64_t smallest;
    unsigned i;

    if (in->pos >= in->len) {
        return 0;
    }
    first = in->data[in->pos++];
    *major = (unsigned)(first >> 5);
    info = (unsigned)(first & 0x1fu);
    if (info < 24) {
        *argument = info;
        return 1;
    }
    switch (info) {
    case 24: extra = 1; smallest = 24u; break;
    case 25: extra = 2; smallest = 0x100ull; break;
    case 26: extra = 4; smallest = 0x10000ull; break;
    case 27: extra = 8; smallest = 0x100000000ull; break;
    default: return 0;
    }
    if (in->len - in->pos < extra) {
        return 0;
    }
    for (i = 0; i < extra; i++) {
        value = (value << 8) | (uint64_t)in->data[in->pos + i];
    }
    in->pos += extra;
    if (value < smallest) {
        return 0;   /* nicht die kuerzeste Schreibweise */
    }
    *argument = value;
    return 1;
}

/* cbor_text liest einen Text in ein festes Feld. Zu lang heisst Fehler, nie
 * Abschneiden. Ein Nullbyte im Text wird abgewiesen.
 *
 * kind_error ist der Schluessel, den eine falsche Sorte ergibt: bei einer
 * Ausbaustufe ist das ein anderer als bei einem Schluessel der Abbildung.
 * Ein kaputter Kopf bleibt immer ein Formatfehler. */
static const char *cbor_text(cbor_reader *in, char *out, size_t size,
                             const char *kind_error)
{
    unsigned major = 0;
    uint64_t length = 0;
    size_t i;

    if (!cbor_head(in, &major, &length)) {
        return err_lic_format;
    }
    if (major != CBOR_TEXT) {
        return kind_error;
    }
    if (length > (uint64_t)(in->len - in->pos)) {
        return err_lic_format;
    }
    if (length + 1 > (uint64_t)size) {
        return err_lic_size;
    }
    for (i = 0; i < (size_t)length; i++) {
        if (in->data[in->pos + i] == 0) {
            return err_lic_format;
        }
    }
    memcpy(out, in->data + in->pos, (size_t)length);
    out[length] = '\0';
    in->pos += (size_t)length;
    return NULL;
}

/* cbor_unsigned liest eine vorzeichenlose Zahl. Eine negative Zahl
 * (Hauptsorte 1) kommt in einer Lizenz nicht vor. */
static const char *cbor_unsigned(cbor_reader *in, uint64_t *out,
                                 uint64_t limit)
{
    unsigned major = 0;

    if (!cbor_head(in, &major, out) || major != CBOR_UNSIGNED) {
        return err_lic_format;
    }
    if (*out > limit) {
        return err_lic_format;
    }
    return NULL;
}

/* Die erlaubten Schluessel der Abbildung. Jeder darf genau einmal
 * vorkommen - sonst waere nicht entschieden, welcher Wert gilt. */
static const struct {
    const char *key;
    unsigned bit;
} license_keys[] = {
    { "p",  1u << 0 },
    { "h",  1u << 1 },
    { "nr", 1u << 2 },
    { "t",  1u << 3 },
    { "s",  1u << 4 },
    { "i",  1u << 5 },
    { "e",  1u << 6 }
};

#define LICENSE_KEY_COUNT 7
#define LICENSE_BIT_PRODUCT (1u << 0)

/* license_parse_payload liest die GEPRUEFTE Nutzlast. */
static const char *license_parse_payload(const unsigned char *payload,
                                         size_t payload_len,
                                         osk_license *out)
{
    cbor_reader in;
    unsigned major = 0;
    unsigned seen = 0;
    uint64_t pairs = 0;
    uint64_t index;

    in.data = payload;
    in.len = payload_len;
    in.pos = 0;
    if (!cbor_head(&in, &major, &pairs)) {
        return err_lic_format;
    }
    if (major != CBOR_MAP) {
        return err_lic_format;
    }
    if (pairs > LICENSE_KEY_COUNT) {
        return err_lic_format;
    }
    /* Je Paar kommen mindestens zwei Byte - mehr Paare als Puffer kann es
     * nicht geben. Geprueft wird das, bevor irgendetwas angelegt wird. */
    if (pairs > (uint64_t)(in.len - in.pos)) {
        return err_lic_format;
    }

    for (index = 0; index < pairs; index++) {
        char key[32];
        const char *problem;
        uint64_t number = 0;
        size_t which;

        problem = cbor_text(&in, key, sizeof(key), err_lic_format);
        if (problem != NULL) {
            /* ein zu langer Schluessel ist kein Groessenproblem der
             * Lizenz, sondern ein Schluessel, den es nicht gibt */
            return (problem == err_lic_size) ? err_lic_format : problem;
        }
        for (which = 0; which < LICENSE_KEY_COUNT; which++) {
            if (strcmp(key, license_keys[which].key) == 0) {
                break;
            }
        }
        if (which == LICENSE_KEY_COUNT) {
            return err_lic_format;   /* unbekannter Schluessel */
        }
        if (seen & license_keys[which].bit) {
            return err_lic_format;   /* derselbe Schluessel zweimal */
        }
        seen |= license_keys[which].bit;

        switch (which) {
        case 0:
            problem = cbor_text(&in, out->product, sizeof(out->product),
                                err_lic_format);
            break;
        case 1:
            problem = cbor_text(&in, out->holder, sizeof(out->holder),
                                err_lic_format);
            break;
        case 2:
            problem = cbor_text(&in, out->number, sizeof(out->number),
                                err_lic_format);
            break;
        case 3: {
            uint64_t count = 0;
            uint64_t tier;

            if (!cbor_head(&in, &major, &count) || major != CBOR_ARRAY) {
                return err_lic_format;
            }
            if (count > OSK_GUARD_MAX_TIERS) {
                return err_lic_size;
            }
            if (count > (uint64_t)(in.len - in.pos)) {
                return err_lic_format;
            }
            for (tier = 0; tier < count; tier++) {
                problem = cbor_text(&in, out->tiers[out->tier_count],
                                    OSK_GUARD_MAX_TIER, err_lic_tier);
                if (problem != NULL) {
                    return problem;
                }
                /* eine Ausbaustufe ohne Namen kann kein Programm pruefen */
                if (out->tiers[out->tier_count][0] == '\0') {
                    return err_lic_tier;
                }
                out->tier_count++;
            }
            break;
        }
        case 4:
            problem = cbor_unsigned(&in, &number, 0xffffffffull);
            out->seats = (uint32_t)number;
            break;
        case 5:
            problem = cbor_unsigned(&in, &number, 0x7fffffffffffffffull);
            out->issued_utc = (int64_t)number;
            break;
        default:
            problem = cbor_unsigned(&in, &number, 0x7fffffffffffffffull);
            out->expires_utc = (int64_t)number;
            break;
        }
        if (problem != NULL) {
            return problem;
        }
    }

    /* Hinter der Abbildung darf nichts mehr stehen - ein angehaengtes Byte
     * wird nicht uebergangen. */
    if (in.pos != in.len) {
        return err_lic_format;
    }
    /* Ohne Produktnamen ist eine Lizenz fuer nichts gueltig. */
    if (!(seen & LICENSE_BIT_PRODUCT) || out->product[0] == '\0') {
        return err_lic_product;
    }
    return NULL;
}

const char *osk_guard_license_check(const unsigned char *block,
                                    size_t block_len,
                                    const unsigned char *issuer_public_key,
                                    size_t issuer_len, osk_license *out)
{
    size_t payload_len;
    unsigned char *signed_message;
    const char *problem;
    int good;

    if (out == NULL) {
        return err_request;
    }
    memset(out, 0, sizeof(*out));
    if (issuer_public_key == NULL || issuer_len != OSK_GUARD_KEY_LEN) {
        return err_issuer;
    }
    if (block == NULL) {
        return err_lic_format;
    }
    if (block_len < LICENSE_HEAD_LEN + OSK_GUARD_SIG_LEN) {
        return err_lic_format;
    }
    if (memcmp(block, license_magic, LICENSE_MAGIC_LEN) != 0) {
        return err_lic_format;
    }
    if (block[LICENSE_MAGIC_LEN] != LICENSE_VERSION) {
        return err_lic_version;
    }
    payload_len = ((size_t)block[LICENSE_MAGIC_LEN + 1] << 8) |
                  (size_t)block[LICENSE_MAGIC_LEN + 2];
    if (payload_len == 0 || payload_len > LICENSE_MAX_PAYLOAD) {
        return err_lic_format;
    }
    /* Die Gesamtlaenge muss genau stimmen - kein Rest, kein Fehlbetrag. */
    if (block_len != LICENSE_HEAD_LEN + payload_len + OSK_GUARD_SIG_LEN) {
        return err_lic_format;
    }

    /* Unterschrieben wird "OSKLIC1" 0x00 Nutzlast. Geprueft wird das, BEVOR
     * aus der Nutzlast ein Wert gelesen wird. */
    signed_message = (unsigned char *)malloc(LICENSE_MAGIC_LEN + 1 +
                                             payload_len);
    if (signed_message == NULL) {
        return err_internal;
    }
    memcpy(signed_message, license_magic, LICENSE_MAGIC_LEN);
    signed_message[LICENSE_MAGIC_LEN] = 0;
    memcpy(signed_message + LICENSE_MAGIC_LEN + 1, block + LICENSE_HEAD_LEN,
           payload_len);
    good = osk_ed25519_verify(issuer_public_key, signed_message,
                              LICENSE_MAGIC_LEN + 1 + payload_len,
                              block + LICENSE_HEAD_LEN + payload_len);
    free(signed_message);
    if (!good) {
        return err_lic_signature;
    }

    problem = license_parse_payload(block + LICENSE_HEAD_LEN, payload_len,
                                   out);
    if (problem != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return problem;
}

const char *osk_guard_license(osk_guard *guard, const char *product,
                              osk_license *out)
{
    text_buffer request;
    unsigned char *block = NULL;
    size_t block_len = 0;
    const char *problem;

    if (guard == NULL || out == NULL) {
        return err_request;
    }
    memset(out, 0, sizeof(*out));
    problem = check_label(product);
    if (problem != NULL) {
        return problem;
    }
    /* Ohne Herstellerschluessel gibt es nichts zu pruefen - und dann wird
     * auch nichts geliefert. */
    if (!guard->has_issuer) {
        return err_issuer;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, "license ");
    buffer_add_hex(&request, (const unsigned char *)product, strlen(product));
    if (request.failed) {
        buffer_free(&request);
        return err_internal;
    }
    problem = ask_value(guard, request.data, "lic", 0, &block, &block_len);
    buffer_free(&request);
    if (problem != NULL) {
        return problem;
    }
    problem = osk_guard_license_check(block, block_len,
                                      guard->issuer_public,
                                      OSK_GUARD_KEY_LEN, out);
    free(block);
    return problem;
}

int osk_guard_license_expired(const osk_license *license,
                              int64_t now_utc)
{
    if (license == NULL) {
        return 1;
    }
    if (license->expires_utc == 0) {
        return 0;
    }
    if (now_utc < 0) {
        now_utc = (int64_t)time(NULL);
    }
    return now_utc > license->expires_utc;
}

static int same_text_ignoring_case(const char *a, const char *b)
{
    size_t i;

    for (i = 0; a[i] != '\0' && b[i] != '\0'; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
    }
    return a[i] == '\0' && b[i] == '\0';
}

int osk_guard_license_has_tier(const osk_license *license,
                               const char *name)
{
    size_t i;

    if (license == NULL || name == NULL) {
        return 0;
    }
    for (i = 0; i < license->tier_count; i++) {
        if (same_text_ignoring_case(license->tiers[i], name)) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- Knoten */

const char *osk_guard_node_public(osk_guard *guard, const char *label,
                                  const char *curve,
                                  unsigned char public_key[OSK_GUARD_KEY_LEN])
{
    text_buffer request;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    const char *problem;

    if (guard == NULL || public_key == NULL) {
        return err_request;
    }
    problem = check_label(label);
    if (problem != NULL) {
        return problem;
    }
    problem = check_label(curve);
    if (problem != NULL) {
        return problem;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, "nodepub ");
    buffer_add_hex(&request, (const unsigned char *)label, strlen(label));
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, (const unsigned char *)curve, strlen(curve));
    if (request.failed) {
        buffer_free(&request);
        return err_internal;
    }
    problem = ask_value(guard, request.data, "pub", OSK_GUARD_KEY_LEN,
                        &raw, &raw_len);
    buffer_free(&request);
    if (problem != NULL) {
        return problem;
    }
    memcpy(public_key, raw, OSK_GUARD_KEY_LEN);
    free(raw);
    return NULL;
}

const char *osk_guard_node_sign(osk_guard *guard, const char *label,
                                const unsigned char *job, size_t job_len,
                                unsigned char signature[OSK_GUARD_SIG_LEN])
{
    text_buffer request;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    const char *problem;

    if (guard == NULL || signature == NULL || job == NULL) {
        return err_request;
    }
    problem = check_label(label);
    if (problem != NULL) {
        return problem;
    }
    if (job_len == 0 || job_len > OSK_GUARD_MAX_PLAIN) {
        return err_size;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, "nodesign ");
    buffer_add_hex(&request, (const unsigned char *)label, strlen(label));
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, job, job_len);
    if (request.failed) {
        buffer_free(&request);
        return err_size;
    }
    problem = ask_value(guard, request.data, "sig", OSK_GUARD_SIG_LEN,
                        &raw, &raw_len);
    buffer_free(&request);
    if (problem != NULL) {
        return problem;
    }
    memcpy(signature, raw, OSK_GUARD_SIG_LEN);
    free(raw);
    return NULL;
}

const char *osk_guard_node_verify(const unsigned char *public_key,
                                  size_t key_len, const char *label,
                                  const unsigned char *job, size_t job_len,
                                  const unsigned char *signature,
                                  size_t signature_len)
{
    unsigned char job_hash[OSK_SHA256_LEN];
    unsigned char *signed_message;
    size_t domain_len = sizeof(sign_domain) - 1;
    size_t label_len;
    size_t total;
    int good;

    if (public_key == NULL || key_len != OSK_GUARD_KEY_LEN) {
        return err_key;
    }
    if (signature == NULL || signature_len != OSK_GUARD_SIG_LEN) {
        return err_signature;
    }
    if (label == NULL || label[0] == '\0' || job == NULL || job_len == 0) {
        return err_size;
    }
    label_len = strlen(label);
    if (label_len > MAX_LABEL) {
        return err_label;
    }

    /* Der Zweck steht vorne, dann Name und Pruefsumme des Auftrags - genau
     * so, wie das Geraet es unterschreibt. */
    osk_sha256(job, job_len, job_hash);
    total = domain_len + 1 + label_len + 1 + sizeof(job_hash);
    signed_message = (unsigned char *)malloc(total);
    if (signed_message == NULL) {
        return err_internal;
    }
    memcpy(signed_message, sign_domain, domain_len);
    signed_message[domain_len] = 0;
    memcpy(signed_message + domain_len + 1, label, label_len);
    signed_message[domain_len + 1 + label_len] = 0;
    memcpy(signed_message + domain_len + 2 + label_len, job_hash,
           sizeof(job_hash));

    good = osk_ed25519_verify(public_key, signed_message, total, signature);
    free(signed_message);
    return good ? NULL : err_signature;
}

const char *osk_guard_node_agree(osk_guard *guard, const char *label,
                                 const unsigned char *peer_public,
                                 size_t peer_len,
                                 unsigned char secret[OSK_GUARD_SECRET_LEN])
{
    text_buffer request;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    const char *problem;

    if (guard == NULL || secret == NULL) {
        return err_request;
    }
    if (peer_public == NULL || peer_len != OSK_GUARD_SECRET_LEN) {
        return err_peerkey;
    }
    problem = check_label(label);
    if (problem != NULL) {
        return problem;
    }

    memset(&request, 0, sizeof(request));
    buffer_add_text(&request, "nodeagree ");
    buffer_add_hex(&request, (const unsigned char *)label, strlen(label));
    buffer_add_text(&request, " ");
    buffer_add_hex(&request, peer_public, peer_len);
    if (request.failed) {
        buffer_free(&request);
        return err_internal;
    }
    problem = ask_value(guard, request.data, "sec", OSK_GUARD_SECRET_LEN,
                        &raw, &raw_len);
    buffer_free(&request);
    if (problem != NULL) {
        return problem;
    }
    memcpy(secret, raw, OSK_GUARD_SECRET_LEN);
    osk_wipe(raw, raw_len);
    free(raw);
    return NULL;
}
