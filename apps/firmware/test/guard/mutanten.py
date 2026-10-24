# -*- coding: utf-8 -*-
"""Mutationskontrollen fuer die Zwecktrennung der Befehle 14 bis 18.

    python3 apps/firmware/test/guard/mutanten.py

Die Zusicherung aus docs/dongle.md und shared/protocol.md lautet: eine
Antwort auf `attest` darf nie als `sign`-Ergebnis durchgehen, und die
64-Byte-Aufgabe des alten attest-Aufrufs nie als Dongle-Antwort. Diese
Zusicherung haengt an genau drei Zeichenketten in osk_guard_msg.c.

Ein gruener Pruefsatz beweist davon nichts: er beweist nur, dass die heutige
Fassung sich so verhaelt. Darum legt dieses Skript die Zwecke absichtlich
zusammen und verlangt, dass test_forgery dabei ROT wird. Wird es gruen, ist
der Fall ein hohler Riegel.

Zwei Kontrollen sind Pflicht (Z:\\GitHub\\claude-md\\messen.md):
  - Mutanten, die STERBEN muessen (zusammengelegte Zwecke)
  - ein Mutant, der LEBEN muss (eine umbenannte lokale Variable)
Verhaelt sich einer falsch, ist der ganze Lauf wertlos.

Das Skript prueft seine eigene Vorbereitung: eine Ersetzung, die nichts
geaendert hat, bricht ab, statt eine Zahl zu nennen. Es arbeitet immer in
einer Kopie - nie im Arbeitsbaum.

Gebraucht werden `make` und ein C-Uebersetzer. Fehlen sie, sagt das Skript
das laut und endet mit Code 2 - es ueberspringt sich nicht still.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.dirname(os.path.dirname(HERE))
GUARD_SRC = os.path.join(FIRMWARE, 'src', 'classes', 'applets', 'guard')
CTAP_SRC = os.path.join(FIRMWARE, 'src', 'classes', 'ctap')
CTAP_TEST = os.path.join(FIRMWARE, 'test', 'ctap')
INCLUDE = os.path.join(FIRMWARE, 'include')

MSG_FILE = os.path.join('src', 'classes', 'applets', 'guard', 'osk_guard_msg.c')

# Jeder Mutant: Name, Datei, Suchen, Ersetzen, und welcher Pruefling
# darueber stolpern muss ('' = keiner, der Mutant muss LEBEN).
MUTANTS = [
    ('attest-raw bekommt den attest-Zweck', MSG_FILE,
     'static const char attest_raw_domain[] = "osk-guard/attest-raw/1";',
     'static const char attest_raw_domain[] = "osk-guard/attest/1";',
     'test_forgery'),
    ('sign bekommt den attest-Zweck', MSG_FILE,
     'static const char sign_domain[] = "osk-guard/sign/1";',
     'static const char sign_domain[] = "osk-guard/attest/1";',
     'test_forgery'),
    ('attest bekommt den sign-Zweck', MSG_FILE,
     'static const char attest_domain[] = "osk-guard/attest/1";',
     'static const char attest_domain[] = "osk-guard/sign/1";',
     'test_msg'),
    ('das Trennbyte hinter dem Namen faellt weg', MSG_FILE,
     '\tput(&b, label, label_len);\n\tput_byte(&b, 0u);',
     '\tput(&b, label, label_len);',
     'test_msg'),
    # Kontrolle: muss LEBEN. Zwei unabhaengige Laengenpruefungen tauschen
    # die Reihenfolge - beide melden OSK_ERR_ARG, von aussen ist kein
    # Unterschied zu sehen. Stirbt dieser Mutant, misst der Pruefsatz die
    # Reihenfolge der Zeilen statt das Verhalten.
    ('Kontrolle: zwei Laengenpruefungen getauscht', MSG_FILE,
     '\tif (label_len == 0u || label_len > OSK_GUARD_LABEL_MAX) {\n'
     '\t\treturn OSK_ERR_ARG;\n\t}\n'
     '\tif (hash_len != OSK_GUARD_HASH_LEN) {\n\t\treturn OSK_ERR_ARG;\n\t}',
     '\tif (hash_len != OSK_GUARD_HASH_LEN) {\n\t\treturn OSK_ERR_ARG;\n\t}\n'
     '\tif (label_len == 0u || label_len > OSK_GUARD_LABEL_MAX) {\n'
     '\t\treturn OSK_ERR_ARG;\n\t}',
     ''),
]


def say(text):
    sys.stdout.write(text + '\n')
    sys.stdout.flush()


def prepare(workdir):
    """Eine Kopie des Baums anlegen - nur das, was der Pruefsatz braucht."""
    for src, rel in ((GUARD_SRC, os.path.join('src', 'classes', 'applets', 'guard')),
                     (CTAP_SRC, os.path.join('src', 'classes', 'ctap')),
                     (CTAP_TEST, os.path.join('test', 'ctap')),
                     (INCLUDE, 'include'),
                     (HERE, os.path.join('test', 'guard'))):
        shutil.copytree(src, os.path.join(workdir, rel))
    build = os.path.join(workdir, 'test', 'guard', 'build')
    if os.path.isdir(build):
        shutil.rmtree(build)


def run_make(workdir, target):
    folder = os.path.join(workdir, 'test', 'guard')
    proc = subprocess.run(['make', target], cwd=folder, capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def build_one(workdir, which):
    """Einen Pruefling bauen und laufen lassen. Rueckgabe (code, ausgabe)."""
    folder = os.path.join(workdir, 'test', 'guard')
    code, out = run_make(workdir, os.path.join('build', which))
    if code != 0:
        return 'BAUABBRUCH', out
    proc = subprocess.run([os.path.join('.', 'build', which)], cwd=folder,
                          capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def apply_mutation(workdir, rel, find, replace):
    path = os.path.join(workdir, rel)
    with open(path, 'r', encoding='utf-8') as handle:
        text = handle.read()
    if text.count(find) != 1:
        return False, 'die Stelle kommt %d mal vor, erwartet genau einmal' % text.count(find)
    changed = text.replace(find, replace)
    with open(path, 'w', encoding='utf-8') as handle:
        handle.write(changed)
    # Die eigene Vorbereitung pruefen: eine Ersetzung, die nicht gewirkt
    # hat, macht jede folgende Zahl wertlos.
    with open(path, 'r', encoding='utf-8') as handle:
        again = handle.read()
    if again == text or find in again:
        return False, 'die Ersetzung hat nicht gewirkt'
    return True, ''


def check_tools():
    for tool in ('make', 'cc'):
        if shutil.which(tool) is None:
            say('ABBRUCH: %s fehlt. Ohne Uebersetzer ist nichts gemessen.' % tool)
            return False
    return True


def main():
    if not check_tools():
        return 2

    tests = sorted({name for _, _, _, _, name in MUTANTS if name})
    failures = 0
    runs = 0

    # 1. Der unveraenderte Stand muss GRUEN sein. Ohne das misst jedes Rot
    #    darunter nur einen kaputten Baum.
    with tempfile.TemporaryDirectory(prefix='osk-guard-mut-') as base:
        clean = os.path.join(base, 'clean')
        os.makedirs(clean)
        prepare(clean)
        for which in tests:
            code, out = build_one(clean, which)
            runs += 1
            if code != 0:
                say('ABBRUCH: %s ist schon ohne Mutation nicht gruen (%s)' % (which, code))
                say(out[-2000:])
                return 2
            say('Ausgangslage %-12s gruen' % which)

    # 2. Je Mutant eine eigene Kopie - ein Mutant darf den naechsten nicht
    #    sehen.
    for label, rel, find, replace, must_die in MUTANTS:
        with tempfile.TemporaryDirectory(prefix='osk-guard-mut-') as base:
            workdir = os.path.join(base, 'work')
            os.makedirs(workdir)
            prepare(workdir)
            ok, why = apply_mutation(workdir, rel, find, replace)
            if not ok:
                say('ABBRUCH: Mutant "%s" liess sich nicht setzen: %s' % (label, why))
                return 2

            if must_die:
                code, out = build_one(workdir, must_die)
                runs += 1
                if code == 'BAUABBRUCH':
                    # Ein Rot aus einem Bauabbruch beweist nichts ueber den
                    # Fall - der Mutant ist dann untauglich.
                    say('FEHLER %-55s uebersetzt nicht - keine Kontrolle' % label)
                    say(out[-1500:])
                    failures += 1
                elif code == 0:
                    say('FEHLER %-55s LEBT - %s merkt es nicht' % (label, must_die))
                    failures += 1
                else:
                    say('ok     %-55s stirbt an %s' % (label, must_die))
            else:
                # Diese Kontrolle muss durch ALLE Pruefsaetze kommen.
                alive = True
                for which in tests:
                    code, out = build_one(workdir, which)
                    runs += 1
                    if code == 'BAUABBRUCH':
                        say('FEHLER %-55s uebersetzt nicht - keine Kontrolle' % label)
                        say(out[-1500:])
                        alive = False
                        break
                    if code != 0:
                        say('FEHLER %-55s stirbt an %s, darf aber nicht'
                            % (label, which))
                        alive = False
                        break
                if alive:
                    say('ok     %-55s lebt, wie verlangt' % label)
                else:
                    failures += 1

    say('')
    say('mutanten: %d Laeufe, %d Mutanten, %d Fehler' % (runs, len(MUTANTS), failures))
    if failures == 0:
        say('mutationskontrollen: ok')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
