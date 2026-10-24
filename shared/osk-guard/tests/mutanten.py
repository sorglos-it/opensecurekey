# -*- coding: utf-8 -*-
"""Mutanten fuer die Dongle-Pruefungen - mit Kontrollen in beide Richtungen.

    python shared/osk-guard/tests/mutanten.py

Gehoert NICHT zu tools/check.py: das hier ist eine Messung der Pruefungen
selbst und dauert ein paar Minuten. Sie beantwortet die Frage, ob die
Faelle in apps/desktop/classes/guard*_test.go ueberhaupt etwas halten.

Drei Mutanten muessen STERBEN (ein Fall wird rot), einer muss LEBEN (nur eine
oertliche Variable umbenannt). Die Dateien im Arbeitsbaum werden nicht
angefasst: `go test -overlay` legt die veraenderte Fassung darueber.

Das Skript prueft seine eigene Vorbereitung: findet es den zu ersetzenden
Text nicht, bricht es ab statt eine Zahl zu nennen. Und ein Rot, das aus
einem Bauabbruch kommt, wird als BAUABBRUCH gemeldet, nicht als Befund.
"""
import io
import json
import os
import subprocess
import sys
import tempfile

SRC = r'D:\claude\opensecurekey-dongle\apps\desktop'
WORK = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mut')

MUTANTS = [
    # (Name, muss_sterben, Datei, alt, neu, Begruendung)
    ('tot1-frage-fehlt', True, 'guard.go',
     '\tout = append(out, nonce...)\n\tout = append(out, binHash...)\n',
     '\tout = append(out, binHash...)\n',
     'Der Zufallswert geht nicht mehr in die Unterschrift ein - '
     'eine aufgezeichnete Antwort wuerde wieder passen.'),
    ('tot2-bezeichnung-fehlt', True, 'guard_seal.go',
     '\tout = append(out, 0)\n\treturn append(out, label...)\n}',
     '\tout = append(out, 0)\n\treturn out\n}',
     'Die Bezeichnung geht nicht in die Ableitung im Chip ein - jede Datei '
     'bekaeme denselben Schluessel und ginge unter jedem Namen auf.'),
    ('tot2b-zusatz-fehlt', True, 'guard_seal.go',
     'func sealAAD(extra []byte) []byte {\n\treturn extra\n}',
     'func sealAAD(extra []byte) []byte {\n\t_ = extra\n\treturn nil\n}',
     'Der Zusatz des Aufrufers wird nicht mitgerechnet - eine Datei ginge '
     'mit jeder Programmversion auf.'),
    ('tot4-fremder-stick', True, 'guard.go',
     '\tif len(answer.PublicKey) != GuardKeyLen ||\n'
     '\t\t!constantEqual(devicePublicKey, answer.PublicKey) {\n'
     '\t\treturn Fail("error.guard.wrongdevice")\n\t}\n',
     '\tif len(answer.PublicKey) != GuardKeyLen {\n'
     '\t\treturn Fail("error.guard.wrongdevice")\n\t}\n'
     '\tdevicePublicKey = answer.PublicKey\n',
     'Geprueft wird gegen den Schluessel, den das Geraet selbst mitschickt '
     '- dann gilt jeder Stick.'),
    ('tot3-freischaltung-fuer-alle', True, 'guard_device.go',
     '\tuntil, found := d.session[label]\n\tif !found {\n\t\treturn false\n\t}\n',
     '\tuntil, found := d.session[label]\n\tif !found {\n'
     '\t\tfor _, bis := range d.session {\n\t\t\tuntil, found = bis, true\n'
     '\t\t\tbreak\n\t\t}\n\t}\n\tif !found {\n\t\treturn false\n\t}\n',
     'Die Freischaltung eines Schluessels gilt fuer alle - genau die '
     'Begrenzung, die zugesagt ist.'),
    ('tot5-bindung-vergessen', True, 'guard.go',
     '\tif cfg.NoBinaryBinding {',
     '\tif !cfg.NoBinaryBinding {',
     'Der leere Wert schaltet die Bindung an die Programmdatei aus - wer '
     'das Feld vergisst, laeuft ungebunden.'),
    ('tot6-kaputte-lizenz-blockt', True, 'guard_license.go',
     '\t\t\t\tif round {\n',
     '\t\t\t\tif true {\n',
     'Eine kaputte Lizenz eines fremden Produkts beendet die Suche - wer '
     'zwei Produkte auf einem Stick hat, kommt an keines.'),
    ('lebt1-variable-umbenannt', False, 'guard.go',
     '\tdigest := sha256.New()\n'
     '\tif _, err := io.Copy(digest, io.LimitReader(file, guardMaxBinary)); err != nil {\n'
     '\t\treturn nil, Fail("error.guard.binary")\n\t}\n'
     '\treturn digest.Sum(nil), nil\n',
     '\tsumme := sha256.New()\n'
     '\tif _, err := io.Copy(summe, io.LimitReader(file, guardMaxBinary)); err != nil {\n'
     '\t\treturn nil, Fail("error.guard.binary")\n\t}\n'
     '\treturn summe.Sum(nil), nil\n',
     'Nur eine oertliche Variable heisst anders - das darf nichts aendern.'),
]


def prepare(name, source, old, new):
    """Legt die veraenderte Fassung ab und prueft, dass die Ersetzung wirkte."""
    path = os.path.join(SRC, 'classes', source)
    with io.open(path, encoding='utf-8') as handle:
        text = handle.read()
    if text.count(old) != 1:
        raise SystemExit('ABBRUCH %s: der zu ersetzende Text kommt %d mal vor'
                         % (name, text.count(old)))
    bent = text.replace(old, new, 1)
    if bent == text:
        raise SystemExit('ABBRUCH %s: die Ersetzung hat nichts geaendert' % name)
    target = os.path.join(WORK, name + '-' + source)
    with io.open(target, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(bent)
    config = os.path.join(WORK, name + '.json')
    with io.open(config, 'w', encoding='utf-8', newline='\n') as handle:
        json.dump({'Replace': {path: target}}, handle)
    return config


def run(config):
    """Laeuft ohne Pipe und gibt (Ausstiegscode, Ausgabe) zurueck."""
    process = subprocess.run(
        ['go', 'test', '-count=1', '-overlay', config, './classes/'],
        cwd=SRC, capture_output=True, text=True, errors='replace')
    return process.returncode, (process.stdout or '') + (process.stderr or '')


def main():
    os.makedirs(WORK, exist_ok=True)
    code, out = run_plain()
    if code != 0:
        print('ABBRUCH: der unveraenderte Stand ist schon rot')
        print(out[-2000:])
        return 1
    print('Grundstand: gruen')

    bad = 0
    for name, must_die, source, old, new, why in MUTANTS:
        config = prepare(name, source, old, new)
        code, out = run(config)
        build_broken = 'build failed' in out or '[build failed]' in out
        failed_cases = [line for line in out.splitlines()
                        if line.startswith('--- FAIL')]
        if build_broken:
            print('%-26s BAUABBRUCH - keine Kontrolle, umbauen' % name)
            bad += 1
            continue
        died = code != 0 and bool(failed_cases)
        mark = 'ok ' if died == must_die else 'FEHLER'
        print('%-26s %s  %s (%d rote Faelle)  %s'
              % (name, mark, 'gestorben' if died else 'lebt',
                 len(failed_cases), why))
        for line in failed_cases[:3]:
            print('        ' + line.strip())
        if died != must_die:
            bad += 1
    print('')
    if bad:
        print('%d von %d Kontrollen verhalten sich falsch - der Lauf ist wertlos'
              % (bad, len(MUTANTS)))
        return 1
    print('Alle %d Kontrollen verhalten sich wie verlangt.' % len(MUTANTS))
    return 0


def run_plain():
    process = subprocess.run(['go', 'test', '-count=1', './classes/'],
                             cwd=SRC, capture_output=True, text=True,
                             errors='replace')
    return process.returncode, (process.stdout or '') + (process.stderr or '')


if __name__ == '__main__':
    sys.exit(main())
