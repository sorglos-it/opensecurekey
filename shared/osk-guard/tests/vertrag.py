# -*- coding: utf-8 -*-
"""Prueft die fuenf Umsetzungen gegen contract.json.

    python shared/osk-guard/tests/vertrag.py

Eine Entscheidung vermehrt sich in fuenf Umsetzungen und laeuft dann
auseinander. Darum steht sie an einer Stelle - und darum wird gemessen, dass
jede Sprache sie auch hat. Diese Pruefung braucht keinen Chip, keinen
Uebersetzer und keine Attrappe: sie liest die Quellen.

Gemessen wird dreierlei:
  1. jeder Pflichtaufruf aus dem Vertrag steht in jeder der fuenf Sprachen,
  2. kein verbotener Aufruf steht irgendwo (eine Ja/Nein-Abfrage erzieht zum
     herausschneidbaren Schutz, und der Dauerbetrieb gehoert nicht in eine
     eingebundene Bibliothek),
  3. jeder Fehlerschluessel, den eine Quelle nennt, steht in lang/de.json -
     ein Schluessel ohne Satz ist ein Fehler ohne Erklaerung.
"""
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GUARD = os.path.dirname(HERE)

# Welche Dateien die Schnittstelle einer Sprache ausmachen.
SOURCES = {
    'c': ['c/osk_guard.h', 'c/osk_guard.c'],
    'go': ['go/osk_guard.go', 'go/license.go'],
    'php': ['php/OskGuard.php', 'php/OskGuardLicenseReader.php'],
    'dotnet': ['dotnet/OskGuard.cs'],
    'python': ['python/osk_guard.py', 'python/license_reader.py'],
}

KEY_PATTERN = re.compile(r'error\.(?:guard|license)\.[a-z]+')


def read(name):
    path = os.path.join(GUARD, name)
    if not os.path.isfile(path):
        return None
    with io.open(path, 'r', encoding='utf-8', errors='replace') as handle:
        return handle.read()


def main():
    contract = json.loads(read('contract.json'))
    phrases = json.loads(read('lang/de.json'))

    text = {}
    problems = []
    for language, files in sorted(SOURCES.items()):
        parts = []
        for name in files:
            body = read(name)
            if body is None:
                problems.append('%s: %s fehlt' % (language, name))
                continue
            parts.append(body)
        text[language] = '\n'.join(parts)

    checked = 0

    # 1. Pflichtaufrufe
    for call in contract['calls']:
        if not call.get('pflicht'):
            continue
        for language in sorted(SOURCES):
            needle = call['namen'].get(language)
            if needle is None:
                problems.append('contract.json: %s nennt keinen Namen fuer %s'
                                % (call['name'], language))
                continue
            checked += 1
            if needle not in text.get(language, ''):
                problems.append('%s fehlt in %s (gesucht: %r)'
                                % (call['name'], language, needle))

    # 2. Verbotenes
    for ban in contract['verboten']:
        for pattern in ban['muster']:
            for language in sorted(SOURCES):
                checked += 1
                if pattern in text.get(language, ''):
                    problems.append('%s: %r ist verboten - %s'
                                    % (language, pattern, ban['warum']))

    # 3. Fehlerschluessel haben einen Satz
    for language in sorted(SOURCES):
        for key in sorted(set(KEY_PATTERN.findall(text.get(language, '')))):
            checked += 1
            if key not in phrases:
                problems.append('%s nennt %s, aber lang/de.json hat dazu '
                                'keinen Satz' % (language, key))

    # 4. Jede Sprache bringt einen eigenen Prueflauf mit.
    for name in ('c/test_guard.c', 'go/present_test.go',
                 'php/test_guard.php', 'dotnet/tests/TestGuard.cs',
                 'python/test_guard.py'):
        checked += 1
        if read(name) is None:
            problems.append('%s fehlt - eine Sprache ohne eigenen Prueflauf '
                            'ist nicht gemessen' % name)

    sys.stdout.write('vertrag: %d Faelle, %d Fehler\n'
                     % (checked, len(problems)))
    for line in problems:
        sys.stdout.write('  FEHLER ' + line + '\n')
    if problems:
        sys.stdout.write('VERTRAG ROT\n')
        return 1
    sys.stdout.write('pruefung: ok\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
