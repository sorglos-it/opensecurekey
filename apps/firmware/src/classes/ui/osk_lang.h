/* Texte der Anzeige in fuenf Sprachen (de, en, fr, it, es).
 *
 * Kein sichtbarer Text steht im Code: jeder Text kommt ueber einen Schluessel
 * aus apps/firmware/lang/<sprache>.json. Daraus erzeugt generate.py die
 * Tabelle osk_lang_table.h, die mit der Firmware ins Flash wandert - ein
 * Dateisystem gibt es dort nicht.
 *
 * Fehlt ein Text in der gewaehlten Sprache, kommt der englische. */
#ifndef OSK_LANG_H
#define OSK_LANG_H

#include <stdbool.h>
#include <stddef.h>

/* Waehlt die Sprache ueber ihr Kuerzel ("de"). Unbekanntes Kuerzel -> Englisch.
 * Liefert true, wenn das Kuerzel bekannt war. */
bool osk_lang_set(const char *name);

const char *osk_lang_current(void);

/* Liefert nie NULL und nie den nackten Schluessel: unbekannte Schluessel
 * geben "?" - und der Pruefsatz unter test/ui sorgt dafuer, dass kein Bild
 * einen Schluessel benutzt, den die Tabelle nicht kennt. */
const char *osk_lang_text(const char *key);

size_t osk_lang_count(void);
const char *osk_lang_name(size_t index);
size_t osk_lang_key_count(void);
const char *osk_lang_key(size_t index);

#endif
