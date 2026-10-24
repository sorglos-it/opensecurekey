/* Texte nachschlagen - siehe osk_lang.h. */

#include <stdbool.h>
#include <string.h>
#include "osk_lang.h"
#include "osk_lang_table.h"

#define LANG_COUNT (sizeof(osk_lang_all) / sizeof(osk_lang_all[0]))

/* Rueckfall ist Englisch. Der Platz wird gesucht, nicht angenommen - die
 * Reihenfolge in der erzeugten Tabelle darf sich aendern. */
static size_t fallback_index(void)
{
	for (size_t i = 0; i < LANG_COUNT; i++) {
		if (strcmp(osk_lang_all[i].name, "en") == 0) {
			return i;
		}
	}
	return 0;
}

static bool g_chosen;
static size_t g_lang_index;

static size_t current_index(void)
{
	return g_chosen ? g_lang_index : fallback_index();
}

bool osk_lang_set(const char *name)
{
	if (name != NULL) {
		for (size_t i = 0; i < LANG_COUNT; i++) {
			if (strcmp(osk_lang_all[i].name, name) == 0) {
				g_lang_index = i;
				g_chosen = true;
				return true;
			}
		}
	}
	g_lang_index = fallback_index();
	g_chosen = true;
	return false;
}

const char *osk_lang_current(void)
{
	return osk_lang_all[current_index()].name;
}

/* Die Schluessel stehen sortiert in der Tabelle - darum halbieren statt suchen. */
static bool find_key(const char *key, size_t *index_out)
{
	size_t low = 0;
	size_t high = OSK_LANG_KEY_COUNT;

	if (key == NULL) {
		return false;
	}
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		int order = strcmp(key, osk_lang_keys[middle]);

		if (order == 0) {
			*index_out = middle;
			return true;
		}
		if (order < 0) {
			high = middle;
		} else {
			low = middle + 1;
		}
	}
	return false;
}

const char *osk_lang_text(const char *key)
{
	size_t index = 0;

	if (!find_key(key, &index)) {
		return "?";
	}
	const char *text = osk_lang_all[current_index()].texts[index];

	if (text == NULL || text[0] == '\0') {
		text = osk_lang_all[fallback_index()].texts[index];
	}
	return (text != NULL) ? text : "?";
}

size_t osk_lang_count(void)
{
	return LANG_COUNT;
}

const char *osk_lang_name(size_t index)
{
	return (index < LANG_COUNT) ? osk_lang_all[index].name : "";
}

size_t osk_lang_key_count(void)
{
	return OSK_LANG_KEY_COUNT;
}

const char *osk_lang_key(size_t index)
{
	return (index < OSK_LANG_KEY_COUNT) ? osk_lang_keys[index] : "";
}
