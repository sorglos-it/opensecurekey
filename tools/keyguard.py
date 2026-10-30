# -*- coding: utf-8 -*-
"""Nachsehen, ob ein privater Schluessel im Projektordner liegt.

    python tools/keyguard.py
    python tools/keyguard.py --selftest

WARUM ES DAS GIBT: Ein Schluessel, der einmal in einem Commit steckt, ist aus
der Geschichte nicht mehr herauszuholen. Wer ihn hat, baut Firmware, die JEDES
Geraet annimmt - und dann ist die ganze Absicherung des Projekts nichts mehr
wert. Darum wird hier nicht gewarnt, sondern abgebrochen.

DREI SCHICHTEN, und jede faengt, was die vorige durchlaesst:

  1. .gitignore          haelt die bekannten Namen heraus (*.key, *.pem, ...)
  2. tools/sign.py       benutzt keinen Schluessel aus dem Projektordner, auch
                         keinen ausdruecklich genannten (refuse_inside_project)
  3. dieses Werkzeug     sieht nach, was WIRKLICH da liegt - nach Inhalt, nicht
                         nur nach Namen, und auch bei Dateien, die .gitignore
                         nicht kennt

Die dritte Schicht ist die, die zaehlt. .gitignore kennt nur Muster, die jemand
vorher aufgeschrieben hat; eine Datei namens "firmware-schluessel.txt" faellt
durch jedes davon. Und ein "git add -f" ueberfaehrt .gitignore sowieso.

WAS ALS SCHLUESSEL GILT:

  - der Inhalt ist GENAU ein 32-Byte-Seed in Hex (64 Zeichen 0-9a-f und sonst
    nichts) - das ist das Format, das tools/sign.py --gen-key schreibt
  - der Inhalt traegt einen PEM-Kopf eines privaten Schluessels
  - der Inhalt traegt einen OpenSSH-Kopf eines privaten Schluessels
  - der Name sieht nach Schluessel aus (*.key, *.pem, id_ed25519, ...)

WAS NICHT: etwas, das INNERHALB einer groesseren Datei steht.

Das ist nicht Feinschliff, sondern gemessen. Der erste Entwurf dieser Datei
schlug am 30.10.2026 auf NEUN eigene Quelldateien an: auf drei Go-Pruefungen
und eine C-Pruefung, weil dort ein PEM-Kopf als Textbaustein in einer
Zeichenkette steht, auf vier Quelldateien wie osk_ctap_key.c, weil ein
Namensmuster zu weit gefasst war - und auf diese Datei hier selbst. Daraus
sind zwei Regeln geworden:

  - Ein PEM- oder OpenSSH-Kopf gilt nur am ANFANG der Datei. Eine echte
    Schluesseldatei beginnt damit; eine Quelldatei, die ihn erwaehnt, hat ihn
    mitten im Text.
  - Namen von Quelldateien (.c, .h, .go, .py, ...) werden gar nicht nach
    Schluesselmustern beurteilt. osk_ctap_key.c ist Quelltext, kein Schluessel.

Dasselbe gilt fuer eine Hexzeichenkette in einer groesseren Datei: in
tools/sign.py stehen die Testvektoren aus RFC 8032, und die sind 64 Zeichen
lang. Wer darauf anschlaegt, hat eine Pruefung, die immer rot ist - und eine
Pruefung, die immer rot ist, sieht nach einer Woche niemand mehr an.

Nur Standardbibliothek.
"""
import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common

# Ordner, in denen nichts gesucht wird: erzeugt, fremd oder nicht unser.
SKIP_DIRS = ('.git', 'build', 'dist', 'node_modules', 'vendor', '__pycache__',
             'graphify-out', '.pio')

# Nur Dateien bis hierher werden gelesen. Ein Schluessel ist klein; ein
# Gerberpaket oder ein Abbild ist gross, und beides Byte fuer Byte zu lesen
# machte die Pruefung so langsam, dass sie abgeschaltet wird.
MAX_READ = 64 * 1024

# Namen, die nach Schluessel aussehen. Dieselbe Liste gehoert nach .gitignore -
# nur steht sie dort als Muster und hier als Muster fuer dieselben Namen.
NAME_PATTERNS = (
    re.compile(r'\.(key|pem|p8|p12|pfx|jks|der|priv|gpg|asc)$', re.I),
    re.compile(r'^id_(rsa|dsa|ecdsa|ed25519)$', re.I),
    re.compile(r'(^|[-_.])(privkey|private[-_.]?key|secret[-_.]?key|signing[-_.]?key)', re.I),
)

# Quelltext wird nach dem NAMEN gar nicht beurteilt. osk_ctap_key.c heisst so,
# weil es um Schluessel GEHT, und ist keiner. Der Inhalt wird trotzdem gelesen -
# ein Schluessel, der in eine .c-Datei kopiert wird, faellt dort auf.
SOURCE_EXT = ('.c', '.h', '.cpp', '.hpp', '.go', '.py', '.js', '.mjs', '.cjs',
              '.md', '.json', '.yml', '.yaml', '.cmake', '.conf', '.overlay',
              '.ld', '.s', '.rs', '.java', '.cs', '.html', '.css', '.svg')

# Ein Name darf trotz Muster durchgehen, wenn er ausdruecklich ein Beispiel
# oder ein OEFFENTLICHER Teil ist. Der oeffentliche Teil gehoert ins Repo -
# der Bootloader braucht ihn (tools/sign.py --pubkey-c).
NAME_ALLOWED = (
    re.compile(r'\.example$', re.I),
    re.compile(r'(^|[-_.])(pub|public)([-_.]|$)', re.I),
    re.compile(r'\.pub$', re.I),
)

# Inhalt: genau ein Seed, nichts sonst. Das schreibt tools/sign.py --gen-key.
SEED_ONLY = re.compile(r'\A[0-9a-fA-F]{64}\Z')

# Alle mit \A: der Kopf gilt nur am ANFANG der Datei, nicht irgendwo darin.
CONTENT_PATTERNS = (
    (re.compile(r'\A\s*-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----'),
     'die Datei BEGINNT mit einem PEM-Kopf eines privaten Schluessels'),
    (re.compile(r'\A\s*-----BEGIN OPENSSH PRIVATE KEY-----'),
     'die Datei BEGINNT mit einem OpenSSH-Schluessel'),
    (re.compile(r'\A\s*-----BEGIN PGP PRIVATE KEY BLOCK-----'),
     'die Datei BEGINNT mit einem PGP-Schluessel'),
)


def name_looks_like_key(name):
    """Sieht dieser Dateiname nach einem privaten Schluessel aus?"""
    base = os.path.basename(name)
    if base.lower().endswith(SOURCE_EXT):
        return None
    if any(pattern.search(base) for pattern in NAME_ALLOWED):
        return None
    for pattern in NAME_PATTERNS:
        if pattern.search(base):
            return 'der Name passt auf %s' % pattern.pattern
    return None


def content_looks_like_key(text):
    """Sieht dieser Inhalt nach einem privaten Schluessel aus?"""
    if SEED_ONLY.match((text or '').strip()):
        return ('der Inhalt ist genau ein 32-Byte-Seed in Hex - das Format von '
                'tools/sign.py --gen-key')
    for pattern, why in CONTENT_PATTERNS:
        if pattern.search(text or ''):
            return why
    return None


def scan(root=common.ROOT):
    """Sucht den ganzen Projektordner ab. Gibt eine Liste (Pfad, Grund).

    Gelesen wird JEDE Datei, die klein genug ist - auch eine, die .gitignore
    kennt. Eine ignorierte Datei kommt zwar nicht von selbst in einen Commit,
    liegt aber im Projektordner, und genau dort darf sie nicht liegen: sie
    wandert in jede Sicherung, in jedes ZIP und in jeden "git add -f".
    """
    hits = []
    for base, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
        for name in sorted(files):
            path = os.path.join(base, name)
            where = os.path.relpath(path, root).replace(os.sep, '/')
            reason = name_looks_like_key(name)
            if reason:
                hits.append((where, reason))
                continue
            try:
                if os.path.getsize(path) > MAX_READ:
                    continue
                with io.open(path, encoding='utf-8', errors='strict') as handle:
                    text = handle.read()
            except (OSError, UnicodeDecodeError):
                continue    # kein Text - ein Schluessel in unseren Formaten ist Text
            reason = content_looks_like_key(text)
            if reason:
                hits.append((where, reason))
    return hits


def selftest():
    """(Zahl der Fehler, Zeilen). Prueft, dass der Schutz anschlaegt UND dass er
    nicht auf alles anschlaegt - eine Pruefung ohne Gegenprobe misst nichts."""
    wrong, lines = 0, []

    def case(name, got, want):
        same = bool(got) == bool(want)
        lines.append('%-62s %s' % (name, 'richtig' if same else 'FALSCH (%r)' % (got,)))
        return 0 if same else 1

    # Muss anschlagen.
    seed = 'a' * 64
    wrong += case('Seed allein in der Datei faellt auf', content_looks_like_key(seed), True)
    wrong += case('Seed mit Zeilenumbruch faellt auf', content_looks_like_key(seed + '\n'), True)
    wrong += case('PEM-Schluessel faellt auf',
                  content_looks_like_key('-----BEGIN PRIVATE KEY-----\nMIIB\n'), True)
    wrong += case('PEM ed25519 faellt auf',
                  content_looks_like_key('-----BEGIN ED25519 PRIVATE KEY-----\n'), True)
    wrong += case('OpenSSH-Schluessel faellt auf',
                  content_looks_like_key('-----BEGIN OPENSSH PRIVATE KEY-----\n'), True)
    for name in ('firmware-ed25519.key', 'mcuboot.pem', 'id_ed25519', 'signing-key.txt',
                 'my-private-key.bin'):
        wrong += case('Name faellt auf: %s' % name, name_looks_like_key(name), True)

    # Darf NICHT anschlagen. Die vier Faelle unten sind nicht erfunden: so hat
    # der erste Entwurf am 30.10.2026 auf eigene Quelldateien angeschlagen.
    wrong += case('oeffentlicher Teil geht durch: firmware-ed25519.pub',
                  name_looks_like_key('firmware-ed25519.pub'), False)
    wrong += case('Beispieldatei geht durch: config.json.example',
                  name_looks_like_key('config.json.example'), False)
    wrong += case('Quelltext geht durch: osk_boot_pubkey.h',
                  name_looks_like_key('osk_boot_pubkey.h'), False)
    wrong += case('Quelltext geht durch: osk_ctap_key.c',
                  name_looks_like_key('osk_ctap_key.c'), False)
    wrong += case('Quelltext geht durch: osk_guard_keys.h',
                  name_looks_like_key('osk_guard_keys.h'), False)
    wrong += case('Quelltext geht durch: agent_key_test.go',
                  name_looks_like_key('agent_key_test.go'), False)
    wrong += case('PEM MITTEN in einer Pruefung geht durch',
                  content_looks_like_key('const pem = "-----BEGIN PRIVATE KEY-----"\n'), False)
    wrong += case('Hex INNERHALB einer Datei geht durch',
                  content_looks_like_key("RFC_VECTORS = ['%s', 'x']\n" % seed), False)
    wrong += case('63 Zeichen sind kein Seed', content_looks_like_key('a' * 63), False)
    wrong += case('65 Zeichen sind kein Seed', content_looks_like_key('a' * 65), False)
    wrong += case('Hex mit einem Buchstaben zu viel ist kein Seed',
                  content_looks_like_key('z' + 'a' * 63), False)
    wrong += case('leere Datei ist kein Schluessel', content_looks_like_key(''), False)

    # Und der eigene Projektordner muss sauber sein - sonst waere diese
    # Pruefung von Anfang an rot und niemand sieht sie mehr an.
    hits = scan()
    wrong += case('der Projektordner selbst ist sauber', hits, False)
    if hits:
        lines += ['   gefunden: %s (%s)' % pair for pair in hits]
    return wrong, lines


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args(argv)
    if args.selftest:
        wrong, lines = selftest()
        common.say('\n'.join(lines))
        common.say('%d falsch.' % wrong)
        return 1 if wrong else 0
    hits = scan(root)
    if not hits:
        common.say('Kein privater Schluessel im Projektordner.')
        return 0
    common.say('FEHLER: Es liegt ein privater Schluessel im Projektordner.')
    for where, why in hits:
        common.say('  %s  -  %s' % (where, why))
    common.say('')
    common.say('Der Schluessel gehoert AUSSERHALB des Projektordners, etwa nach')
    common.say('%%USERPROFILE%%\\.osk-keys\\ - siehe docs/signatur.md. Datei verschieben,')
    common.say('nicht nur in .gitignore eintragen: ignoriert ist nicht weg.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
