# -*- coding: utf-8 -*-
"""Doppelte Namen zwischen den Teilen der Firmware finden - ohne Uebersetzer.

Der Pruefsatz baut und bindet jeden Teil (ctap, se050, applets, ...) einzeln.
Zwei Teile duerfen deshalb monatelang denselben Namen vergeben, ohne dass es
auffaellt: erst der Binder des Gesamtbaus sagt "multiple definition". Genau so
sind osk_wipe (zweimal definiert) und osk_apdu_parse (zweimal definiert, mit
verschiedenen Rueckgabetypen) bis in ein gebautes Abbild gekommen.

Hier wird nicht gebunden, sondern gelesen: aus jeder .c-Datei die Namen, die
der Binder sehen wuerde (nicht-statische Funktionen und globale Variablen auf
Dateiebene), und aus jeder .h-Datei die angekuendigten Rueckgabetypen. Das
braucht keine Werkzeugkette und laeuft darum ueberall - auf Windows wie auf
Linux, in der CI wie am Arbeitsplatz. Ein Binder waere genauer; er ist hier
aber nur an einem von zwei Rechnern vorhanden, und eine Pruefung, die meistens
uebersprungen wird, findet nichts.

Grenzen, ehrlich gesagt: gelesen wird der Text, nicht der uebersetzte Code.
Namen, die erst ein Makro zusammensetzt, und Dateien, die #if ausknipst,
sieht diese Pruefung nicht. Was sie meldet, steht aber wirklich so im Quelltext.

Nur Standardbibliothek.
"""
import io
import os
import re

# Woerter, die am Anfang eines Stuecks stehen koennen, ohne dass es eine
# Definition ist (Steuerwoerter und alles, was der Binder nie sieht).
NO_SYMBOL = frozenset((
    'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'default', 'return',
    'goto', 'break', 'continue', 'sizeof', 'typedef', 'struct', 'union',
    'enum', 'extern', 'static', '_Static_assert', 'K_THREAD_STACK_DEFINE',
))
# Vorsaetze, die vor dem Rueckgabetyp stehen duerfen.
QUALIFIERS = frozenset((
    'const', 'volatile', 'inline', '__inline', 'register', 'signed',
    'unsigned', 'short', 'long', 'struct', 'union', 'enum', '_Noreturn',
))
NAME_RE = re.compile(r'[A-Za-z_]\w*')
# Zephyr-Makros erzeugen Namen, die hier nicht abzulesen sind - solche
# Stuecke werden uebergangen statt falsch geraten.
MACRO_DEFINITION_RE = re.compile(r'^[A-Z][A-Z0-9_]*\s*\(')


def strip_c(text):
    """Kommentare und Zeichenketten leeren, Vorverarbeiter-Zeilen entfernen.

    Die Zeilenzahl bleibt erhalten, damit gemeldete Stellen stimmen.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == '/*':
            end = text.find('*/', i + 2)
            end = n if end < 0 else end + 2
            out.append('\n' * text.count('\n', i, end))
            i = end
        elif two == '//':
            end = text.find('\n', i)
            i = n if end < 0 else end
        elif c in '"\'':
            i += 1
            while i < n and text[i] != c:
                i += 2 if text[i] == '\\' else 1
            out.append('""' if c == '"' else "''")
            i += 1
        else:
            out.append(c)
            i += 1
    text = ''.join(out)
    # Vorverarbeiter-Zeilen (mit Fortsetzung) leeren, Zeilen aber zaehlen.
    lines = text.split('\n')
    keep, in_macro = [], False
    for line in lines:
        if in_macro or line.lstrip().startswith('#'):
            in_macro = line.rstrip().endswith('\\')
            keep.append('')
        else:
            keep.append(line)
    return '\n'.join(keep)


IF_RE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|endif)\b')
# Der Doppelaufnahme-Schutz eines Kopfes ('#ifndef X_H' direkt gefolgt von
# '#define X_H') ist keine Bedingung, sondern ein Riegel - sonst stuende jede
# Ankuendigung in jedem Kopf als "bedingt" da und waere nie zu pruefen.
GUARD_IF_RE = re.compile(r'^\s*#\s*ifndef\s+(\w+)\s*$')
GUARD_DEF_RE = re.compile(r'^\s*#\s*define\s+(\w+)\s*$')


def conditional_depths(text):
    """Je Zeile (1-basiert): wie viele #if umschliessen sie.

    Drei Unterbauten desselben Kopfes (se050_crypto_psa/cng/openssl.c) tragen
    dieselben Namen und schliessen sich per #if gegenseitig aus - fuer den
    Binder sind sie nie zugleich da. Ob zwei Bedingungen sich wirklich
    ausschliessen, ist am Text nicht zu entscheiden; darum ist nur eine
    Doppelung ohne jede Bedingung ein Fehler, der Rest ein Hinweis.
    """
    lines = text.split('\n')
    guard = None
    for i, line in enumerate(lines):
        hit = GUARD_IF_RE.match(line)
        if hit:
            rest = [l for l in lines[i + 1:i + 3] if l.strip()]
            if rest and GUARD_DEF_RE.match(rest[0]) and GUARD_DEF_RE.match(rest[0]).group(1) == hit.group(1):
                guard = i
        if line.strip() and not line.lstrip().startswith(('#', '/*', '*', '//')):
            break

    depths, depth, in_macro = [0], 0, False
    for number, line in enumerate(lines):
        if number == guard:
            depths.append(depth)
            continue
        match = None if in_macro else IF_RE.match(line)
        if match and match.group(1) == 'endif':
            depth = max(0, depth - 1)
            depths.append(depth)
        elif match:
            depths.append(depth)
            depth += 1
        else:
            depths.append(depth)
        in_macro = line.rstrip().endswith('\\') and (in_macro or line.lstrip().startswith('#'))
    return depths


def depth_at(depths, line):
    return depths[line] if 0 <= line < len(depths) else 0


def chunks(text):
    """Stuecke auf Dateiebene: (Text, Zeile, naechstes Zeichen).

    Das naechste Zeichen ist '{' bei einem Rumpf und ';' bei einer Ankuendigung;
    nur so sind Definition und Prototyp auseinanderzuhalten.
    """
    found, depth, start, line, start_line = [], 0, 0, 1, 1
    for i, c in enumerate(text):
        if c == '\n':
            line += 1
        elif depth == 0 and c in '{;':
            piece = text[start:i]
            if piece.strip():
                found.append((piece, start_line, c))
            if c == '{':
                depth = 1
            start, start_line = i + 1, line
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth <= 0:
                depth, start, start_line = 0, i + 1, line
        if not text[start:i + 1].strip():
            start_line = line
    return found


def top_level_call(piece):
    """(Name, Rueckgabeteil) des Deklarators, oder None.

    Gesucht ist der Name direkt vor der ersten Klammer auf Ebene 0 - bei
    'osk_result_t osk_apdu_parse(const uint8_t *buf, ...)' also osk_apdu_parse.
    """
    depth = 0
    for i, c in enumerate(piece):
        if c == '(' and depth == 0:
            before = piece[:i]
            names = list(NAME_RE.finditer(before))
            if not names:
                return None
            return names[-1].group(0), before[:names[-1].start()]
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
    return None


def first_word(piece):
    match = NAME_RE.search(piece)
    return match.group(0) if match else ''


def variable_name(piece):
    """Name einer globalen Variablen, oder None (nur ohne Klammern auf Ebene 0)."""
    head = piece.split('=')[0]
    if '(' in head or '[' in head.split('[')[0]:
        pass
    head = head.split('[')[0]
    names = list(NAME_RE.finditer(head))
    if len(names) < 2:          # ein Wort allein ist ein Typ, keine Variable
        return None
    return names[-1].group(0)


def definitions(text):
    """Namen, die der Binder in dieser Uebersetzungseinheit sehen wuerde.

    Gibt [(Name, Zeile, Art, bedingt)] - Art ist 'funktion' oder 'variable',
    'bedingt' heisst: steht in einem #if-Block.
    """
    found, depths = [], conditional_depths(text)
    for piece, line, closer in chunks(strip_c(text)):
        words = set(NAME_RE.findall(piece))
        head = first_word(piece)
        if head in NO_SYMBOL or 'static' in words or 'typedef' in words:
            continue
        if MACRO_DEFINITION_RE.match(piece.strip()):
            continue
        call = top_level_call(piece)
        if closer == '{':
            if call and call[0] not in NO_SYMBOL and call[1].strip():
                found.append((call[0], line, 'funktion', depth_at(depths, line) > 0))
            continue
        # ';' - Prototyp (hat Klammern) zaehlt nicht, eine Variable schon.
        if call:
            continue
        name = variable_name(piece)
        if name and name not in NO_SYMBOL:
            found.append((name, line, 'variable', depth_at(depths, line) > 0))
    return found


def prototypes(text):
    """Angekuendigte Funktionen: [(Name, Zeile, Rueckgabetyp, bedingt)]."""
    found, depths = [], conditional_depths(text)
    for piece, line, closer in chunks(strip_c(text)):
        words = set(NAME_RE.findall(piece))
        if first_word(piece) in NO_SYMBOL or 'typedef' in words:
            continue
        call = top_level_call(piece)
        if not call:
            continue
        name, before = call
        kind = ' '.join(before.split())
        if not kind or name in NO_SYMBOL:
            continue
        if closer == '{' and 'static' not in words:
            continue            # Rumpf: die Definition zaehlt woanders
        found.append((name, line, kind, depth_at(depths, line) > 0))
    return found


def read(path):
    with io.open(path, encoding='utf-8', errors='replace') as handle:
        return handle.read()


def sources(folder, suffix, skip=('test', 'build', 'tests')):
    found = []
    for base, dirs, files in os.walk(folder):
        dirs[:] = [d for d in dirs if d not in skip and not d.startswith('.')]
        found += [os.path.join(base, n) for n in files if n.endswith(suffix)]
    return sorted(found)


def part_of(path, folder):
    """Der Teil, zu dem eine Datei gehoert - fuer die Meldung."""
    rel = os.path.relpath(path, folder).replace(os.sep, '/')
    bits = rel.split('/')
    if bits[0] == 'src' and len(bits) > 2 and bits[1] == 'classes':
        return bits[2]
    return bits[0]


def scan(folder):
    """Doppelte Definitionen und widerspruechliche Rueckgabetypen.

    Gibt (Doppelte, Widersprueche, Hinweise, Zahl der Dateien, Zahl der Namen):
      Doppelte      [(Name, Art, [Stelle, ...])] - zwei Rumpfe ohne Bedingung,
                    der Binder bricht hier ab: "multiple definition"
      Widersprueche [(Name, [(Stelle, Typ), ...])] - gleicher Name, anderer
                    Rueckgabetyp; bei OSK_OK == 0 kehrt das die Bedeutung um
      Hinweise      [(Name, [Stelle, ...])] - doppelt, aber mindestens eine
                    Stelle steht in einem #if; ob die sich ausschliessen, ist
                    am Text nicht zu entscheiden, darum kein Fehler
    Eine Stelle ist 'teil/datei.c:zeile'.
    """
    seen = {}
    files = sources(folder, '.c')
    for path in files:
        where = '%s/%s' % (part_of(path, folder), os.path.basename(path))
        for name, line, kind, bedingt in definitions(read(path)):
            # Zweimal dieselbe Datei kann der Binder nicht doppelt sehen; nur
            # verschiedene Uebersetzungseinheiten zaehlen.
            seen.setdefault(name, []).append(
                ('%s:%d' % (where, line), kind, os.path.abspath(path), bedingt))

    doubles, hints = [], []
    for name in sorted(seen):
        places = seen[name]
        if len({path for _, _, path, _ in places}) < 2:
            continue
        hard = [p for p in places if not p[3]]
        if len({path for _, _, path, _ in hard}) >= 2:
            kinds = sorted({k for _, k, _, _ in hard})
            doubles.append((name, '/'.join(kinds), [w for w, _, _, _ in hard]))
        else:
            hints.append((name, [w for w, _, _, _ in places]))

    types = {}
    for path in sources(folder, '.h') + files:
        where = '%s/%s' % (part_of(path, folder), os.path.basename(path))
        for name, line, kind, bedingt in prototypes(read(path)):
            if bedingt:
                continue
            types.setdefault(name, []).append(('%s:%d' % (where, line), kind))
    clashes = []
    for name in sorted(types):
        places = types[name]
        if len({kind for _, kind in places}) > 1:
            clashes.append((name, sorted(places)))
    return doubles, clashes, hints, len(files), len(seen)
