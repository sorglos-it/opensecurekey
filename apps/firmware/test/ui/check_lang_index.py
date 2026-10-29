# -*- coding: utf-8 -*-
"""Prueft, dass jeder Index in osk_lang_table.h in allen fuenf Sprachen
denselben Begriff meint.

    python apps/firmware/test/ui/check_lang_index.py

Warum eigene Probe: generate.py vergleicht nur die Schluesselmengen. Faellt ein
Eintrag weg, rutscht alles danach eine Stelle - die Tabelle uebersetzt dann
noch, zeigt aber den falschen Text. Ein verschobener Index ist still und faellt
sonst erst am Geraet auf.

Gelesen wird die erzeugte Tabelle, verglichen wird gegen die fuenf
Sprachdateien. Nur Standardbibliothek.
"""
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.dirname(os.path.dirname(HERE))
TABLE = os.path.join(FIRMWARE, 'src', 'classes', 'ui', 'osk_lang_table.h')
LANG_DIR = os.path.join(FIRMWARE, 'lang')
LANGUAGES = ('de', 'en', 'fr', 'it', 'es')


def flatten(data, prefix=''):
    flat = {}
    if isinstance(data, dict):
        for key, value in sorted(data.items()):
            flat.update(flatten(value, '%s%s.' % (prefix, key)))
    else:
        flat[prefix.rstrip('.')] = data
    return flat


def read_array(text, name):
    """Die Zeichenketten eines Feldes aus der erzeugten Tabelle, in Reihenfolge."""
    head = re.search(r'\*const\s+%s\[[^\]]*\]\s*=\s*\{' % re.escape(name), text)
    if head is None:
        raise SystemExit('Feld %s steht nicht in %s' % (name, TABLE))
    body = text[head.end():text.index('};', head.end())]
    out = []
    for line in body.splitlines():
        line = line.strip()
        if not line:
            continue
        if not (line.startswith('"') and line.endswith('",')):
            raise SystemExit('unerwartete Zeile in %s: %r' % (name, line))
        # Rueckwaerts durch c_string() aus generate.py: nur Anfuehrungszeichen
        # und Rueckstrich sind dort geschuetzt worden.
        inner = line[1:-2]
        inner = inner.replace('\\"', '"')
        inner = inner.replace('\\\\', '\\')
        out.append(inner)
    return out


def main():
    with io.open(TABLE, encoding='utf-8') as handle:
        text = handle.read()
    count = int(re.search(r'#define OSK_LANG_KEY_COUNT (\d+)', text).group(1))
    keys = read_array(text, 'osk_lang_keys')
    if len(keys) != count:
        raise SystemExit('OSK_LANG_KEY_COUNT ist %d, es stehen %d Schluessel da'
                         % (count, len(keys)))

    bad = []
    for lang in LANGUAGES:
        with io.open(os.path.join(LANG_DIR, lang + '.json'), encoding='utf-8') as handle:
            flat = flatten(json.load(handle))
        texts = read_array(text, 'osk_lang_' + lang)
        if len(texts) != count:
            bad.append('%s: %d Texte statt %d' % (lang, len(texts), count))
            continue
        for index, key in enumerate(keys):
            if key not in flat:
                bad.append('%s: Schluessel %s fehlt in der Sprachdatei' % (lang, key))
            elif flat[key] != texts[index]:
                bad.append('%s: Platz %d heisst %s, da steht %r, erwartet %r'
                           % (lang, index, key, texts[index], flat[key]))

    if bad:
        sys.stdout.write('INDIZES VERSCHOBEN:\n')
        for line in bad:
            sys.stdout.write('  ' + line + '\n')
        sys.stdout.write('  neu erzeugen: python apps/firmware/src/classes/ui/generate.py\n')
        return 1
    sys.stdout.write('%d Plaetze, %d Sprachen: jeder Platz meint ueberall denselben Begriff\n'
                     % (count, len(LANGUAGES)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
