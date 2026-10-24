# -*- coding: utf-8 -*-
"""Erzeugt die beiden Schriften und die Texttabelle der Anzeige.

    python apps/firmware/src/classes/ui/generate.py            # schreibt die Dateien
    python apps/firmware/src/classes/ui/generate.py --check     # nur vergleichen

Erzeugt:
    osk_font_data.c   beide Schriften als Bitmuster, Zeile fuer Zeile
    osk_lang_table.h  die Texte aus apps/firmware/lang/*.json

Warum erzeugt und nicht von Hand: die Buchstaben mit Zeichen (ä, é, ñ, ç ...)
entstehen aus dem Grundbuchstaben plus einem Strich darueber - 54 Stueck, die
sonst alle einzeln gezeichnet und gepflegt werden muessten. Die erzeugten
Dateien liegen im Repo; der Pruefsatz unter test/ui vergleicht sie gegen diesen
Lauf und wird rot, wenn jemand die erzeugte Datei von Hand aendert.

Nur Standardbibliothek.
"""
import argparse
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
LANG_DIR = os.path.join(FIRMWARE, 'lang')
LANGUAGES = ('de', 'en', 'fr', 'it', 'es')

# --- Schriftmasse -------------------------------------------------------------
#
# Beide Schriften sind zeilenweise gespeichert: ein Byte je Bildzeile, das
# niedrigste Bit ist die linke Spalte. Zeile 0 ist frei und traegt das Zeichen
# ueber dem Buchstaben; die letzte Zeile traegt Unterlaengen (g, j, p, q, y)
# und die Cedille unter dem c.
#
#   normal: 5 Punkte breit, 9 Zeilen hoch  -> 6 x 10 Punkte je Platz
#   klein:  3 Punkte breit, 7 Zeilen hoch  -> 4 x 8  Punkte je Platz

BIG_W, BIG_ROWS = 5, 9
SMALL_W, SMALL_ROWS = 3, 7

# --- Grundbuchstaben, 5 breit, Zeilen 1..8 ------------------------------------
#
# Gezeichnet fuer dieses Projekt. '#' ist ein Punkt, '.' ist leer. Die achte
# Zeile ist die Unterlaenge und bei den meisten Zeichen leer.

BIG = {
    ' ': '...../...../...../...../...../...../...../.....',
    '!': '..#../..#../..#../..#../..#../...../..#../.....',
    '"': '.#.#./.#.#./...../...../...../...../...../.....',
    '#': '.#.#./#####/.#.#./#####/.#.#./...../...../.....',
    '$': '..#../.####/##.../.###./...##/####./..#../.....',
    '%': '##.../##..#/...#./..#../.#.../#..##/...##/.....',
    '&': '.##../#..#./#.#../.#.../#.#.#/#..#./.##.#/.....',
    "'": '..#../..#../...../...../...../...../...../.....',
    '(': '...#./..#../.#.../.#.../.#.../..#../...#./.....',
    ')': '.#.../..#../...#./...#./...#./..#../.#.../.....',
    '*': '...../#.#.#/.###./#####/.###./#.#.#/...../.....',
    '+': '...../..#../..#../#####/..#../..#../...../.....',
    ',': '...../...../...../...../...../..##./..#../.#...',
    '-': '...../...../...../#####/...../...../...../.....',
    '.': '...../...../...../...../...../.##../.##../.....',
    '/': '....#/...#./...#./..#../.#.../.#.../#..../.....',
    '0': '.###./#...#/#..##/#.#.#/##..#/#...#/.###./.....',
    '1': '..#../.##../..#../..#../..#../..#../.###./.....',
    '2': '.###./#...#/....#/...#./..#../.#.../#####/.....',
    '3': '#####/...#./..##./....#/....#/#...#/.###./.....',
    '4': '...#./..##./.#.#./#..#./#####/...#./...#./.....',
    '5': '#####/#..../####./....#/....#/#...#/.###./.....',
    '6': '..##./.#.../#..../####./#...#/#...#/.###./.....',
    '7': '#####/....#/...#./..#../..#../..#../..#../.....',
    '8': '.###./#...#/#...#/.###./#...#/#...#/.###./.....',
    '9': '.###./#...#/#...#/.####/....#/...#./.##../.....',
    ':': '...../.##../.##../...../.##../.##../...../.....',
    ';': '...../.##../.##../...../..##./..#../.#.../.....',
    '<': '....#/...#./..#../.#.../..#../...#./....#/.....',
    '=': '...../...../#####/...../#####/...../...../.....',
    '>': '#..../.#.../..#../...#./..#../.#.../#..../.....',
    '?': '.###./#...#/....#/...#./..#../...../..#../.....',
    '@': '.###./#...#/....#/.##.#/#.#.#/#.#.#/.####/.....',
    'A': '.###./#...#/#...#/#####/#...#/#...#/#...#/.....',
    'B': '####./#...#/#...#/####./#...#/#...#/####./.....',
    'C': '.###./#...#/#..../#..../#..../#...#/.###./.....',
    'D': '####./#...#/#...#/#...#/#...#/#...#/####./.....',
    'E': '#####/#..../#..../####./#..../#..../#####/.....',
    'F': '#####/#..../#..../####./#..../#..../#..../.....',
    'G': '.###./#...#/#..../#..##/#...#/#...#/.###./.....',
    'H': '#...#/#...#/#...#/#####/#...#/#...#/#...#/.....',
    'I': '.###./..#../..#../..#../..#../..#../.###./.....',
    'J': '..###/...#./...#./...#./...#./#..#./.##../.....',
    'K': '#...#/#..#./#.#../##.../#.#../#..#./#...#/.....',
    'L': '#..../#..../#..../#..../#..../#..../#####/.....',
    'M': '#...#/##.##/#.#.#/#.#.#/#...#/#...#/#...#/.....',
    'N': '#...#/##..#/##..#/#.#.#/#..##/#..##/#...#/.....',
    'O': '.###./#...#/#...#/#...#/#...#/#...#/.###./.....',
    'P': '####./#...#/#...#/####./#..../#..../#..../.....',
    'Q': '.###./#...#/#...#/#...#/#.#.#/#..##/.####/.....',
    'R': '####./#...#/#...#/####./#.#../#..#./#...#/.....',
    'S': '.###./#...#/#..../.###./....#/#...#/.###./.....',
    'T': '#####/..#../..#../..#../..#../..#../..#../.....',
    'U': '#...#/#...#/#...#/#...#/#...#/#...#/.###./.....',
    'V': '#...#/#...#/#...#/#...#/#...#/.#.#./..#../.....',
    'W': '#...#/#...#/#...#/#.#.#/#.#.#/##.##/#...#/.....',
    'X': '#...#/#...#/.#.#./..#../.#.#./#...#/#...#/.....',
    'Y': '#...#/#...#/.#.#./..#../..#../..#../..#../.....',
    'Z': '#####/....#/...#./..#../.#.../#..../#####/.....',
    '[': '..###/..#../..#../..#../..#../..#../..###/.....',
    '\\': '#..../#..../.#.../..#../..#../...#./....#/.....',
    ']': '###../..#../..#../..#../..#../..#../###../.....',
    '^': '..#../.#.#./#...#/...../...../...../...../.....',
    '_': '...../...../...../...../...../...../#####/.....',
    '`': '.#.../..#../...../...../...../...../...../.....',
    'a': '...../...../.###./....#/.####/#...#/.####/.....',
    'b': '#..../#..../#.##./##..#/#...#/#...#/####./.....',
    'c': '...../...../.###./#...#/#..../#...#/.###./.....',
    'd': '....#/....#/.##.#/#..##/#...#/#...#/.####/.....',
    'e': '...../...../.###./#...#/#####/#..../.###./.....',
    'f': '..##./.#..#/.#.../####./.#.../.#.../.#.../.....',
    'g': '...../...../.####/#...#/#...#/.####/....#/.###.',
    'h': '#..../#..../#.##./##..#/#...#/#...#/#...#/.....',
    'i': '..#../...../..#../..#../..#../..#../..#../.....',
    'j': '...#./...../...#./...#./...#./...#./...#./.##..',
    'k': '#..../#..../#..#./#.#../##.../#.#../#..#./.....',
    'l': '.##../..#../..#../..#../..#../..#../..###/.....',
    'm': '...../...../##.#./#.#.#/#.#.#/#.#.#/#...#/.....',
    'n': '...../...../#.##./##..#/#...#/#...#/#...#/.....',
    'o': '...../...../.###./#...#/#...#/#...#/.###./.....',
    'p': '...../...../####./#...#/#...#/####./#..../#....',
    'q': '...../...../.####/#...#/#...#/.####/....#/....#',
    'r': '...../...../#.##./##..#/#..../#..../#..../.....',
    's': '...../...../.####/#..../.###./....#/####./.....',
    't': '.#.../.#.../####./.#.../.#.../.#..#/..##./.....',
    'u': '...../...../#...#/#...#/#...#/#..##/.##.#/.....',
    'v': '...../...../#...#/#...#/#...#/.#.#./..#../.....',
    'w': '...../...../#...#/#.#.#/#.#.#/#.#.#/.#.#./.....',
    'x': '...../...../#...#/.#.#./..#../.#.#./#...#/.....',
    'y': '...../...../#...#/#...#/#...#/.####/....#/.###.',
    'z': '...../...../#####/...#./..#../.#.../#####/.....',
    '{': '...##/..#../..#../.##../..#../..#../...##/.....',
    '|': '..#../..#../..#../..#../..#../..#../..#../.....',
    '}': '##.../..#../..#../..##./..#../..#../##.../.....',
    '~': '...../..#.#/.#.#./...../...../...../...../.....',
    # Zeichen, die nicht aus einem Grundbuchstaben entstehen
    'ß': '...../.##../#..#./#.##./#..#./#..#./##.#./.....',
    'œ': '...../...../.#.#./#.#.#/#.###/#.#../.#.#./.....',
    'Œ': '.####/#.#../#.#../#.###/#.#../#.#../.####/.....',
    '¡': '..#../...../..#../..#../..#../..#../..#../.....',
    '¿': '..#../...../..#../.#.../#..../#...#/.###./.....',
    '€': '..###/.#.../#####/.#.../#####/.#.../..###/.....',
    '«': '...../#.#../#.#../#.#../#.#../...../...../.....',
    '»': '...../..#.#/..#.#/..#.#/..#.#/...../...../.....',
}

# --- Grundbuchstaben, 3 breit, Zeilen 1..6 ------------------------------------
#
# Fuer die woertlichen Angaben: 12 Zeichen je Zeile statt 8. Gross und Ziffern
# stehen in den Zeilen 1..5, kleine in 2..5, Unterlaengen in Zeile 6.

SMALL = {
    ' ': '.../.../.../.../.../...',
    '!': '.#./.#./.#./.../.#./...',
    '"': '#.#/#.#/.../.../.../...',
    '#': '#.#/###/#.#/###/#.#/...',
    '$': '.##/##./.#./.##/##./...',
    '%': '#.#/..#/.#./#../#.#/...',
    '&': '##./##./###/#.#/.##/...',
    "'": '.#./.#./.../.../.../...',
    '(': '.##/.#./#../.#./.##/...',
    ')': '##./.#./..#/.#./##./...',
    '*': '#.#/.#./###/.#./#.#/...',
    '+': '.../.#./###/.#./.../...',
    ',': '.../.../.../.##/.#./#..',
    '-': '.../.../###/.../.../...',
    '.': '.../.../.../.##/.##/...',
    '/': '..#/..#/.#./#../#../...',
    '0': '###/#.#/#.#/#.#/###/...',
    '1': '.#./##./.#./.#./###/...',
    '2': '##./..#/.#./#../###/...',
    '3': '##./..#/.##/..#/##./...',
    '4': '#.#/#.#/###/..#/..#/...',
    '5': '###/#../##./..#/##./...',
    '6': '.##/#../###/#.#/###/...',
    '7': '###/..#/.#./.#./.#./...',
    '8': '###/#.#/###/#.#/###/...',
    '9': '###/#.#/###/..#/##./...',
    ':': '.../.#./.../.#./.../...',
    ';': '.../.#./.../.##/.#./...',
    '<': '..#/.#./#../.#./..#/...',
    '=': '.../###/.../###/.../...',
    '>': '#../.#./..#/.#./#../...',
    '?': '##./..#/.#./.../.#./...',
    '@': '###/#.#/###/#../.##/...',
    'A': '###/#.#/###/#.#/#.#/...',
    'B': '##./#.#/##./#.#/##./...',
    'C': '###/#../#../#../###/...',
    'D': '##./#.#/#.#/#.#/##./...',
    'E': '###/#../###/#../###/...',
    'F': '###/#../###/#../#../...',
    'G': '###/#../#.#/#.#/###/...',
    'H': '#.#/#.#/###/#.#/#.#/...',
    'I': '###/.#./.#./.#./###/...',
    'J': '..#/..#/..#/#.#/###/...',
    'K': '#.#/#.#/##./#.#/#.#/...',
    'L': '#../#../#../#../###/...',
    'M': '#.#/###/###/#.#/#.#/...',
    'N': '##./#.#/#.#/#.#/..#/...',
    'O': '###/#.#/#.#/#.#/###/...',
    'P': '###/#.#/###/#../#../...',
    'Q': '###/#.#/#.#/###/..#/...',
    'R': '###/#.#/##./#.#/#.#/...',
    'S': '.##/#../.#./..#/##./...',
    'T': '###/.#./.#./.#./.#./...',
    'U': '#.#/#.#/#.#/#.#/###/...',
    'V': '#.#/#.#/#.#/#.#/.#./...',
    'W': '#.#/#.#/###/###/#.#/...',
    'X': '#.#/#.#/.#./#.#/#.#/...',
    'Y': '#.#/#.#/.#./.#./.#./...',
    'Z': '###/..#/.#./#../###/...',
    '[': '.##/.#./.#./.#./.##/...',
    '\\': '#../#../.#./..#/..#/...',
    ']': '##./.#./.#./.#./##./...',
    '^': '.#./#.#/.../.../.../...',
    '_': '.../.../.../.../###/...',
    '`': '#../.#./.../.../.../...',
    'a': '.../##./#.#/#.#/.##/...',
    'b': '#../##./#.#/#.#/##./...',
    'c': '.../.##/#../#../.##/...',
    'd': '..#/.##/#.#/#.#/.##/...',
    'e': '.../.##/###/#../.##/...',
    'f': '.##/#../##./#../#../...',
    'g': '.../.##/#.#/.##/..#/##.',
    'h': '#../##./#.#/#.#/#.#/...',
    'i': '.#./.../.#./.#./.#./...',
    'j': '..#/.../..#/..#/..#/##.',
    'k': '#../#.#/##./##./#.#/...',
    'l': '##./.#./.#./.#./.##/...',
    'm': '.../##./###/###/#.#/...',
    'n': '.../##./#.#/#.#/#.#/...',
    'o': '.../.#./#.#/#.#/.#./...',
    'p': '.../##./#.#/##./#../#..',
    'q': '.../.##/#.#/.##/..#/..#',
    'r': '.../.##/#../#../#../...',
    's': '.../.##/##./..#/##./...',
    't': '.#./###/.#./.#./.##/...',
    'u': '.../#.#/#.#/#.#/.##/...',
    'v': '.../#.#/#.#/#.#/.#./...',
    'w': '.../#.#/###/###/.#./...',
    'x': '.../#.#/.#./.#./#.#/...',
    'y': '.../#.#/#.#/.##/..#/##.',
    'z': '.../###/.#./#../###/...',
    '{': '.##/.#./##./.#./.##/...',
    '|': '.#./.#./.#./.#./.#./...',
    '}': '##./.#./.##/.#./##./...',
    '~': '.../.##/##./.../.../...',
    'ß': '.##/#.#/##./#.#/##./...',
    'œ': '.../###/###/#.#/###/...',
    'Œ': '###/#.#/###/#../###/...',
    '¡': '.#./.../.#./.#./.#./...',
    '¿': '.#./.../.#./#../.##/...',
    '€': '.##/###/#../###/.##/...',
    '«': '.../#.#/##./#.#/.../...',
    '»': '.../#.#/.##/#.#/.../...',
}

# --- Buchstaben mit Zeichen darueber ------------------------------------------
#
# Die Striche sind nur eine Bildzeile hoch - mehr gibt der Platz nicht her.
# Deshalb sehen Dach und Strich sich aehnlich; das steht so in docs/display.md.

MARKS = {
    'grave': ('.#...', '#..'),
    'acute': ('...#.', '..#'),
    'circ': ('..#..', '.#.'),
    'uml': ('.#.#.', '#.#'),
    'tilde': ('.###.', '###'),
}

# Zeichen unter dem Buchstaben (Cedille) - steht in der letzten Zeile.
CEDILLA = ('..#..', '.#.')

# Buchstabe mit Zeichen: Ziel, Grundbuchstabe, Zeichen.
ACCENTED = [
    ('à', 'a', 'grave'), ('á', 'a', 'acute'), ('â', 'a', 'circ'), ('ä', 'a', 'uml'),
    ('è', 'e', 'grave'), ('é', 'e', 'acute'), ('ê', 'e', 'circ'), ('ë', 'e', 'uml'),
    ('ì', 'i', 'grave'), ('í', 'i', 'acute'), ('î', 'i', 'circ'), ('ï', 'i', 'uml'),
    ('ò', 'o', 'grave'), ('ó', 'o', 'acute'), ('ô', 'o', 'circ'), ('ö', 'o', 'uml'),
    ('ù', 'u', 'grave'), ('ú', 'u', 'acute'), ('û', 'u', 'circ'), ('ü', 'u', 'uml'),
    ('ñ', 'n', 'tilde'), ('ÿ', 'y', 'uml'),
    ('À', 'A', 'grave'), ('Á', 'A', 'acute'), ('Â', 'A', 'circ'), ('Ä', 'A', 'uml'),
    ('È', 'E', 'grave'), ('É', 'E', 'acute'), ('Ê', 'E', 'circ'), ('Ë', 'E', 'uml'),
    ('Ì', 'I', 'grave'), ('Í', 'I', 'acute'), ('Î', 'I', 'circ'), ('Ï', 'I', 'uml'),
    ('Ò', 'O', 'grave'), ('Ó', 'O', 'acute'), ('Ô', 'O', 'circ'), ('Ö', 'O', 'uml'),
    ('Ù', 'U', 'grave'), ('Ú', 'U', 'acute'), ('Û', 'U', 'circ'), ('Ü', 'U', 'uml'),
    ('Ñ', 'N', 'tilde'), ('Ÿ', 'Y', 'uml'),
]

CEDILLA_CHARS = [('ç', 'c'), ('Ç', 'C')]


def row_byte(line, width):
    """Eine Bildzeile als Byte, Bit 0 ist links."""
    if len(line) != width:
        raise ValueError('Zeile muss %d breit sein: %r' % (width, line))
    value = 0
    for column, dot in enumerate(line):
        if dot == '#':
            value |= 1 << column
        elif dot != '.':
            raise ValueError('nur # und . erlaubt: %r' % line)
    return value


def rows_of(art, width, rows):
    """Bildzeile -> Byte. Zeile 0 bleibt frei, danach kommt das Gezeichnete."""
    parts = art.split('/')
    if len(parts) != rows - 1:
        raise ValueError('%d Zeilen erwartet, %d bekommen: %r' % (rows - 1, len(parts), art))
    out = [0]
    for line in parts:
        out.append(row_byte(line, width))
    return out


def with_mark(base, mark, width, rows, lowercase):
    """Setzt den Strich ueber den Buchstaben: Zeile 1, wo sie frei ist, sonst Zeile 0."""
    glyph = list(base)
    row = 1 if lowercase and glyph[1] == 0 else 0
    glyph[row] |= row_byte(mark, width)
    return glyph


def with_cedilla(base, mark, width, rows):
    glyph = list(base)
    glyph[rows - 1] |= row_byte(mark, width)
    return glyph


def build_font(art, width, rows, mark_index):
    """Liefert {Zeichen: [Bytes]} - Grundzeichen und abgeleitete zusammen."""
    glyphs = {}
    for char, drawing in art.items():
        glyphs[char] = rows_of(drawing, width, rows)
    for char, base, mark in ACCENTED:
        glyphs[char] = with_mark(glyphs[base], MARKS[mark][mark_index], width, rows,
                                 char.islower())
    for char, base in CEDILLA_CHARS:
        glyphs[char] = with_cedilla(glyphs[base], CEDILLA[mark_index], width, rows)
    return glyphs


def charset():
    """Alle Zeichen in fester Reihenfolge: erst ASCII, dann der Rest."""
    ascii_part = [chr(c) for c in range(0x20, 0x7F)]
    extra = sorted({c for c in BIG if ord(c) > 0x7F} |
                   {c for c, _, _ in ACCENTED} | {c for c, _ in CEDILLA_CHARS},
                   key=ord)
    return ascii_part, extra


# --- Sprachdateien ------------------------------------------------------------

def flatten(data, prefix=''):
    flat = {}
    if isinstance(data, dict):
        for key, value in sorted(data.items()):
            flat.update(flatten(value, '%s%s.' % (prefix, key)))
    else:
        flat[prefix.rstrip('.')] = data
    return flat


def read_languages():
    texts = {}
    for lang in LANGUAGES:
        path = os.path.join(LANG_DIR, lang + '.json')
        with io.open(path, encoding='utf-8') as handle:
            texts[lang] = flatten(json.load(handle))
    keys = sorted(texts['en'])
    for lang, flat in texts.items():
        missing = sorted(set(keys) - set(flat))
        extra = sorted(set(flat) - set(keys))
        if missing or extra:
            raise SystemExit('%s.json: fehlt %s, zu viel %s' % (lang, missing, extra))
    return keys, texts


def c_string(text):
    out = []
    for char in text:
        if char == '"':
            out.append('\\"')
        elif char == '\\':
            out.append('\\\\')
        elif char == '\n':
            out.append('\\n')
        elif ord(char) < 0x20:
            raise ValueError('Steuerzeichen im Text: %r' % text)
        else:
            out.append(char)
    return '"%s"' % ''.join(out)


# --- Ausgabe ------------------------------------------------------------------

# Die Marke haelt clang-format von den Tabellen fern: sonst wuerde es sie
# umbrechen, und danach stimmten sie nicht mehr mit diesem Lauf ueberein.
HEAD = ('/* ERZEUGT von src/classes/ui/generate.py - nicht von Hand aendern.\n'
        ' * Der Pruefsatz test/ui vergleicht diese Datei gegen einen neuen Lauf. */\n'
        '/* clang-format off */')


def font_table(name, glyphs, ascii_part, extra, rows):
    lines = ['static const uint8_t %s[][%d] = {' % (name, rows)]
    for char in ascii_part + extra:
        data = glyphs[char]
        shown = char if 0x20 < ord(char) < 0x7F and char not in '*/' else 'U+%04X' % ord(char)
        lines.append('\t{%s}, /* %s */' % (', '.join('0x%02X' % b for b in data), shown))
    lines.append('};')
    return '\n'.join(lines)


def write_font(path):
    ascii_part, extra = charset()
    big = build_font(BIG, BIG_W, BIG_ROWS, 0)
    small = build_font(SMALL, SMALL_W, SMALL_ROWS, 1)
    for char in ascii_part + extra:
        for name, table in (('normal', big), ('klein', small)):
            if char not in table:
                raise SystemExit('Schrift %s kennt U+%04X nicht' % (name, ord(char)))
    body = [HEAD, '', '#include "osk_font.h"', '',
            '/* Ein Byte je Bildzeile, Bit 0 ist links. Zeile 0 traegt das Zeichen',
            ' * ueber dem Buchstaben, die letzte Zeile Unterlaengen und die Cedille. */',
            font_table('font_big_rows', big, ascii_part, extra, BIG_ROWS), '',
            font_table('font_small_rows', small, ascii_part, extra, SMALL_ROWS), '',
            '/* Zeichen jenseits von ASCII: Codepunkt -> Platz in den Tabellen oben. */',
            'static const uint16_t font_extra_cp[] = {']
    for index, char in enumerate(extra):
        body.append('\t0x%04X, /* %s = %d */' % (ord(char), 'U+%04X' % ord(char),
                                                 len(ascii_part) + index))
    body += ['};', '',
             '#define FONT_ASCII_COUNT %d' % len(ascii_part),
             '#define FONT_EXTRA_COUNT %d' % len(extra), '',
             'const osk_font_t osk_font_big = {',
             '\t.width = %d,' % BIG_W,
             '\t.rows = %d,' % BIG_ROWS,
             '\t.advance = %d,' % (BIG_W + 1),
             '\t.line = %d,' % (BIG_ROWS + 1),
             '\t.glyphs = &font_big_rows[0][0],',
             '};', '',
             'const osk_font_t osk_font_small = {',
             '\t.width = %d,' % SMALL_W,
             '\t.rows = %d,' % SMALL_ROWS,
             '\t.advance = %d,' % (SMALL_W + 1),
             '\t.line = %d,' % (SMALL_ROWS + 1),
             '\t.glyphs = &font_small_rows[0][0],',
             '};', '',
             'size_t osk_font_glyph_count(void)',
             '{',
             '\treturn (size_t)(FONT_ASCII_COUNT + FONT_EXTRA_COUNT);',
             '}', '',
             '/* Codepunkt -> Platz. Unbekanntes bekommt das Fragezeichen, nie eine Luecke. */',
             'size_t osk_font_index(uint32_t codepoint)',
             '{',
             '\tif (codepoint >= 0x20u && codepoint < 0x7Fu) {',
             '\t\treturn (size_t)(codepoint - 0x20u);',
             '\t}',
             '\tfor (size_t i = 0; i < FONT_EXTRA_COUNT; i++) {',
             '\t\tif (font_extra_cp[i] == codepoint) {',
             '\t\t\treturn (size_t)FONT_ASCII_COUNT + i;',
             '\t\t}',
             '\t}',
             '\treturn (size_t)(0x3Fu - 0x20u); /* ? */',
             '}', '',
             '/* Zeile einer Schriftstelle holen - die Tabellen liegen flach. */',
             'uint8_t osk_font_row(const osk_font_t *font, size_t index, uint8_t row)',
             '{',
             '\tif (font == NULL || row >= font->rows || index >= osk_font_glyph_count()) {',
             '\t\treturn 0;',
             '\t}',
             '\treturn font->glyphs[index * font->rows + row];',
             '}', '']
    text = '\n'.join(body)
    cost = (len(ascii_part) + len(extra)) * (BIG_ROWS + SMALL_ROWS) + len(extra) * 2
    return text, cost


def write_lang(path):
    keys, texts = read_languages()
    lines = [HEAD, '', '#ifndef OSK_LANG_TABLE_H', '#define OSK_LANG_TABLE_H', '',
             '/* %d Schluessel in %d Sprachen. Quelle: apps/firmware/lang/<sprache>.json */'
             % (len(keys), len(LANGUAGES)), '',
             '#define OSK_LANG_KEY_COUNT %d' % len(keys), '',
             'static const char *const osk_lang_keys[OSK_LANG_KEY_COUNT] = {']
    for key in keys:
        lines.append('\t%s,' % c_string(key))
    lines += ['};', '']
    for lang in LANGUAGES:
        lines.append('static const char *const osk_lang_%s[OSK_LANG_KEY_COUNT] = {' % lang)
        for key in keys:
            lines.append('\t%s,' % c_string(texts[lang][key]))
        lines += ['};', '']
    lines += ['static const struct {',
              '\tconst char *name;',
              '\tconst char *const *texts;',
              '} osk_lang_all[] = {']
    for lang in LANGUAGES:
        lines.append('\t{"%s", osk_lang_%s},' % (lang, lang))
    lines += ['};', '', '#endif', '', '/* clang-format on */', '']
    bytes_needed = sum(len(texts[l][k].encode('utf-8')) + 1 for l in LANGUAGES for k in keys)
    bytes_needed += sum(len(k) + 1 for k in keys)
    return '\n'.join(lines), bytes_needed


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--check', action='store_true', help='nur vergleichen, nichts schreiben')
    parser.add_argument('--out', default=HERE, help='Zielordner (Vorgabe: dieser)')
    args = parser.parse_args(argv)

    font_path = os.path.join(args.out, 'osk_font_data.c')
    lang_path = os.path.join(args.out, 'osk_lang_table.h')
    font_text, font_cost = write_font(font_path)
    lang_text, lang_cost = write_lang(lang_path)

    bad = []
    for path, text in ((font_path, font_text), (lang_path, lang_text)):
        if args.check:
            try:
                with io.open(path, encoding='utf-8') as handle:
                    if handle.read() != text:
                        bad.append(os.path.basename(path))
            except IOError:
                bad.append(os.path.basename(path) + ' fehlt')
        else:
            with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
                handle.write(text)
    if args.check:
        if bad:
            sys.stdout.write('ERZEUGTE DATEI VERALTET: %s\n'
                             '  neu erzeugen: python %s\n'
                             % (', '.join(bad), os.path.relpath(__file__)))
            return 1
        sys.stdout.write('erzeugte Dateien stimmen mit generate.py ueberein\n')
        return 0
    sys.stdout.write('Schriften: %d Byte Bitmuster und Tabelle\n' % font_cost)
    sys.stdout.write('Texte: %d Byte in 5 Sprachen\n' % lang_cost)
    return 0


if __name__ == '__main__':
    sys.exit(main())
