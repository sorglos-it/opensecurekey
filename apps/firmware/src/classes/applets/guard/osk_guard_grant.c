/* Die Freischaltung. Siehe osk_guard_grant.h. */
#include <string.h>
#include "osk_guard_grant.h"

/* Ein Eintrag der Freischaltung. Der Name liegt als Kopie fester Laenge
 * darin: der Puffer der Nachricht ist nach der Antwort weg. */
typedef struct {
	bool used;
	uint8_t len;
	char label[OSK_GUARD_LABEL_MAX];
	uint64_t until_ms;
} grant_entry_t;

static grant_entry_t g_grant[OSK_GUARD_GRANT_MAX];

void osk_guard_grant_clear(void)
{
	memset(g_grant, 0, sizeof(g_grant));
}

/* Ein Name ohne Nullbyte, nicht leer, nicht zu lang. Dieselbe Regel wie in
 * osk_guard_msg_sign() - sonst koennte ein Name freigeschaltet sein, der
 * beim Unterschreiben abgelehnt wird oder umgekehrt. */
static bool label_ok(const char *label, size_t len)
{
	return label != NULL && len > 0u && len <= OSK_GUARD_LABEL_MAX &&
	       memchr(label, 0, len) == NULL;
}

/* Abgelaufene Eintraege verfallen beim Hinsehen. Das ist der ganze Ablauf:
 * es gibt keinen Zeitgeber, der etwas abschalten muesste - und damit auch
 * keinen, der ausfallen koennte. */
static void expire(uint64_t now_ms)
{
	for (size_t i = 0; i < OSK_GUARD_GRANT_MAX; i++) {
		if (g_grant[i].used && now_ms >= g_grant[i].until_ms) {
			memset(&g_grant[i], 0, sizeof(g_grant[i]));
		}
	}
}

osk_result_t osk_guard_grant_set(const char *const *labels, const size_t *label_lens, size_t count,
				 uint32_t ttl_s, uint64_t now_ms)
{
	if (labels == NULL || label_lens == NULL) {
		return OSK_ERR_ARG;
	}
	if (count == 0u || count > OSK_GUARD_GRANT_MAX) {
		return OSK_ERR_ARG;
	}
	if (ttl_s == 0u || ttl_s > OSK_GUARD_TTL_MAX) {
		return OSK_ERR_ARG;
	}
	/* Erst alles pruefen, dann erst etwas aendern: ein krummer Name in der
	 * Mitte darf keine halb eingeschaltete Freischaltung hinterlassen. */
	for (size_t i = 0; i < count; i++) {
		if (!label_ok(labels[i], label_lens[i])) {
			return OSK_ERR_ARG;
		}
	}

	const uint64_t until = now_ms + (uint64_t)ttl_s * 1000u;

	/* Eine neue Freischaltung ersetzt die alte vollstaendig. Sonst
	 * sammelten sich Namen an, die beim Einschalten niemand genannt hat. */
	memset(g_grant, 0, sizeof(g_grant));
	for (size_t i = 0; i < count; i++) {
		g_grant[i].used = true;
		g_grant[i].len = (uint8_t)label_lens[i];
		memcpy(g_grant[i].label, labels[i], label_lens[i]);
		g_grant[i].until_ms = until;
	}
	return OSK_OK;
}

bool osk_guard_grant_allows(const char *label, size_t label_len, uint64_t now_ms)
{
	if (!label_ok(label, label_len)) {
		return false;
	}
	expire(now_ms);
	for (size_t i = 0; i < OSK_GUARD_GRANT_MAX; i++) {
		if (!g_grant[i].used || g_grant[i].len != label_len) {
			continue;
		}
		if (memcmp(g_grant[i].label, label, label_len) == 0) {
			return true;
		}
	}
	return false;
}

void osk_guard_grant_state(osk_guard_grant_state_t *out, uint64_t now_ms)
{
	if (out == NULL) {
		return;
	}
	expire(now_ms);
	out->on = false;
	out->count = 0u;
	out->ttl_left_s = 0u;
	for (size_t i = 0; i < OSK_GUARD_GRANT_MAX; i++) {
		if (!g_grant[i].used) {
			continue;
		}
		out->count++;
		out->on = true;
		const uint64_t left = (g_grant[i].until_ms - now_ms) / 1000u;
		if (left > (uint64_t)out->ttl_left_s) {
			out->ttl_left_s = (uint32_t)left;
		}
	}
}

const char *osk_guard_grant_label(size_t index, size_t *len_out, uint64_t now_ms)
{
	expire(now_ms);
	size_t seen = 0;
	for (size_t i = 0; i < OSK_GUARD_GRANT_MAX; i++) {
		if (!g_grant[i].used) {
			continue;
		}
		if (seen == index) {
			if (len_out != NULL) {
				*len_out = g_grant[i].len;
			}
			return g_grant[i].label;
		}
		seen++;
	}
	return NULL;
}
