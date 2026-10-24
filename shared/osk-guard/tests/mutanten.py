# -*- coding: utf-8 -*-
"""Mutanten fuer den Dongle-Schutz - mit Kontrollen in beide Richtungen.

    python shared/osk-guard/tests/mutanten.py

Gehoert NICHT zu tools/check.py: das hier ist eine Messung der Pruefungen
selbst und dauert ein paar Minuten. Sie beantwortet die Frage, ob die Faelle
in apps/desktop/classes/guard*_test.go und in shared/osk-guard/go/*_test.go
ueberhaupt etwas halten.

Mehrere Mutanten muessen STERBEN (ein Fall wird rot), einer muss LEBEN (nur
eine oertliche Variable umbenannt). Die Dateien im Arbeitsbaum werden nicht
angefasst: `go test -overlay` legt die veraenderte Fassung darueber.

Das Skript prueft seine eigene Vorbereitung: findet es den zu ersetzenden
Text nicht genau einmal, bricht es ab statt eine Zahl zu nennen. Und ein
Rot, das aus einem Bauabbruch kommt, wird als BAUABBRUCH gemeldet, nicht als
Befund - ein Mutant, der nicht uebersetzt, ist keine Kontrolle.
"""
import io
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
# tests -> osk-guard -> shared -> Projektstamm
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SRC = os.path.join(ROOT, 'apps', 'desktop')
GO = os.path.join(ROOT, 'shared', 'osk-guard', 'go')
WORK = tempfile.mkdtemp(prefix='osk-mutanten-')

# Ein berechneter Pfad wird gegen etwas gehalten, das nur der richtige
# Ordner hat - "drei Ebenen hoeher" ist keine Pruefung.
if not os.path.isfile(os.path.join(SRC, 'classes', 'guard.go')):
    raise SystemExit('ABBRUCH: %s ist nicht apps/desktop' % SRC)
if not os.path.isfile(os.path.join(GO, 'cbor.go')):
    raise SystemExit('ABBRUCH: %s ist nicht shared/osk-guard/go' % GO)

# (Name, muss_sterben, Datei, alt, neu, Begruendung)
# Dateien ohne Vorsatz liegen in apps/desktop/classes/, solche mit "go:" in
# shared/osk-guard/go/.
MUTANTS = [
    ('tot1-frage-fehlt', True, 'guard.go',
     '\tout = append(out, nonce...)\n\tout = append(out, binHash...)\n',
     '\tout = append(out, binHash...)\n',
     'Der Zufallswert geht nicht mehr in die Unterschrift ein - eine '
     'aufgezeichnete Antwort wuerde wieder passen.'),
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
    ('tot3-freischaltung-fuer-alle', True, 'guard_device.go',
     '\tuntil, found := d.session[label]\n\tif !found {\n\t\treturn false\n\t}\n',
     '\tuntil, found := d.session[label]\n\tif !found {\n'
     '\t\tfor _, bis := range d.session {\n\t\t\tuntil, found = bis, true\n'
     '\t\t\tbreak\n\t\t}\n\t}\n\tif !found {\n\t\treturn false\n\t}\n',
     'Die Freischaltung eines Schluessels gilt fuer alle - genau die '
     'Begrenzung, die zugesagt ist.'),
    ('tot4-fremder-stick', True, 'guard.go',
     '\tif len(answer.PublicKey) != GuardKeyLen ||\n'
     '\t\t!constantEqual(devicePublicKey, answer.PublicKey) {\n'
     '\t\treturn Fail("error.guard.wrongdevice")\n\t}\n',
     '\tif len(answer.PublicKey) != GuardKeyLen {\n'
     '\t\treturn Fail("error.guard.wrongdevice")\n\t}\n'
     '\tdevicePublicKey = answer.PublicKey\n',
     'Geprueft wird gegen den Schluessel, den das Geraet selbst mitschickt - '
     'dann gilt jeder Stick.'),
    ('tot5-bindung-vergessen', True, 'guard.go',
     '\tif cfg.NoBinaryBinding {',
     '\tif !cfg.NoBinaryBinding {',
     'Der leere Wert schaltet die Bindung an die Programmdatei aus - wer das '
     'Feld vergisst, laeuft ungebunden.'),
    ('tot6-kaputte-lizenz-blockt', True, 'guard_license.go',
     '\t\t\t\tif round {\n',
     '\t\t\t\tif true {\n',
     'Eine kaputte Lizenz eines fremden Produkts beendet die Suche - wer '
     'zwei Produkte auf einem Stick hat, kommt an keines.'),

    # --- Der CBOR-Leser der Go-Bindung -----------------------------------
    # Diese vier messen, was ohne die unterschriebenen Lizenz-Pruefwerte in
    # tests/license-vectors.json gar nicht messbar waere: geprueft wird die
    # Unterschrift VOR dem Lesen, also braucht eine falsch geschriebene
    # Nutzlast eine gueltige Unterschrift, um bis zum Leser zu kommen.
    ('tot7-vier-byte-form', True, 'go:cbor.go',
     '\t\tvalue := uint64(binary.BigEndian.Uint32(data[1:5]))\n'
     '\t\tif value <= 0xffff {\n'
     '\t\t\treturn 0, nil, fail("error.license.format")\n\t\t}\n',
     '\t\tvalue := uint64(binary.BigEndian.Uint32(data[1:5]))\n',
     'Eine kleine Zahl darf in der 4-Byte-Form stehen - dieselbe Zahl waere '
     'dann mehrfach schreibbar, in unterschriebenen Daten.'),
    ('tot8-acht-byte-form', True, 'go:cbor.go',
     '\t\tvalue := binary.BigEndian.Uint64(data[1:9])\n'
     '\t\tif value <= 0xffffffff {\n'
     '\t\t\treturn 0, nil, fail("error.license.format")\n\t\t}\n',
     '\t\tvalue := binary.BigEndian.Uint64(data[1:9])\n',
     'Dasselbe fuer die 8-Byte-Form.'),
    ('tot9-reste-uebergangen', True, 'go:cbor.go',
     '\tif len(rest) != 0 {\n',
     '\tif len(rest) != 0 && false {\n',
     'Bytes hinter der Nutzlast werden stillschweigend uebergangen.'),
    ('tot10-schluessel-zweimal', True, 'go:cbor.go',
     '\t\t\tif _, twice := out[name]; twice {',
     '\t\t\tif _, twice := out[name]; false && twice {',
     'Derselbe Schluessel darf zweimal vorkommen - dann ist nicht '
     'entschieden, welches Produkt die Lizenz nennt.'),

    # Dieser Mutant aendert nicht das ERGEBNIS, sondern den Satz, den der
    # Anwender liest - abgewiesen wird ein fremder Stick so oder so. Die
    # Lehre kam von der Python-Fassung, die ihren eigenen Leser mutiert
    # hat: ein falscher Satz schickt jemanden an die falsche Stelle, und
    # genau das merkt kein Fall, der nur auf "abgewiesen" prueft.
    ('tot11-falscher-satz-bei-fremdem-stick', True, 'go:osk_guard.go',
     'if !equal(public, g.devicePub) {',
     'if false && !equal(public, g.devicePub) {',
     'Ein fremder Stick wird mit "die Unterschrift stimmt nicht" '
     'abgewiesen statt mit "es steckt ein anderer Schluessel".'),

    ('lebt1-variable-umbenannt', False, 'guard.go',
     '\tdigest := sha256.New()\n'
     '\tif _, err := io.Copy(digest, io.LimitReader(file, guardMaxBinary)); '
     'err != nil {\n'
     '\t\treturn nil, Fail("error.guard.binary")\n\t}\n'
     '\treturn digest.Sum(nil), nil\n',
     '\tsumme := sha256.New()\n'
     '\tif _, err := io.Copy(summe, io.LimitReader(file, guardMaxBinary)); '
     'err != nil {\n'
     '\t\treturn nil, Fail("error.guard.binary")\n\t}\n'
     '\treturn summe.Sum(nil), nil\n',
     'Nur eine oertliche Variable heisst anders - das darf nichts aendern.'),
]


def where(source):
    """Loest 'guard.go' und 'go:cbor.go' in Datei, Ordner und Pruefziel auf."""
    if source.startswith('go:'):
        return os.path.join(GO, source[3:]), GO, '.'
    return os.path.join(SRC, 'classes', source), SRC, './classes/'


def prepare(name, source, old, new):
    """Legt die veraenderte Fassung ab und prueft, dass die Ersetzung wirkte."""
    path = where(source)[0]
    with io.open(path, encoding='utf-8') as handle:
        text = handle.read()
    if text.count(old) != 1:
        raise SystemExit('ABBRUCH %s: der zu ersetzende Text kommt %d mal vor'
                         % (name, text.count(old)))
    bent = text.replace(old, new, 1)
    if bent == text:
        raise SystemExit('ABBRUCH %s: die Ersetzung hat nichts geaendert' % name)
    target = os.path.join(WORK, name + '-' + os.path.basename(source))
    with io.open(target, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(bent)
    config = os.path.join(WORK, name + '.json')
    with io.open(config, 'w', encoding='utf-8', newline='\n') as handle:
        json.dump({'Replace': {path: target}}, handle)
    return config


def run(config, source):
    """Laeuft OHNE Pipe und gibt (Ausstiegscode, Ausgabe) zurueck - der Code
    einer Pipe gehoert dem letzten Glied, nicht dem Programm."""
    cwd, target = where(source)[1], where(source)[2]
    process = subprocess.run(
        ['go', 'test', '-count=1', '-overlay', config, target],
        cwd=cwd, capture_output=True, text=True, errors='replace')
    return process.returncode, (process.stdout or '') + (process.stderr or '')


def run_plain():
    """Beide Baeume im unveraenderten Stand. Ist einer schon rot, hat eine
    Mutantenmessung keinen Sinn."""
    out = ''
    for cwd, target in ((SRC, './classes/'), (GO, '.')):
        process = subprocess.run(['go', 'test', '-count=1', target], cwd=cwd,
                                 capture_output=True, text=True,
                                 errors='replace')
        out += (process.stdout or '') + (process.stderr or '')
        if process.returncode != 0:
            return process.returncode, out
    return 0, out


def main():
    code, out = run_plain()
    if code != 0:
        print('ABBRUCH: der unveraenderte Stand ist schon rot')
        print(out[-2000:])
        return 1
    print('Grundstand: gruen (beide Baeume)')

    bad = 0
    for name, must_die, source, old, new, why in MUTANTS:
        config = prepare(name, source, old, new)
        code, out = run(config, source)
        if 'build failed' in out:
            print('%-28s BAUABBRUCH - keine Kontrolle, umbauen' % name)
            bad += 1
            continue
        failed = [line for line in out.splitlines()
                  if line.strip().startswith('--- FAIL')]
        died = code != 0 and bool(failed)
        mark = 'ok    ' if died == must_die else 'FEHLER'
        print('%-28s %s %-10s (%d rote Faelle)  %s'
              % (name, mark, 'gestorben' if died else 'lebt', len(failed), why))
        for line in failed[:3]:
            print('        ' + line.strip())
        if died != must_die:
            bad += 1
    print('')
    if bad:
        print('%d von %d Kontrollen verhalten sich falsch - der Lauf ist '
              'wertlos' % (bad, len(MUTANTS)))
        return 1
    print('Alle %d Kontrollen verhalten sich wie verlangt.' % len(MUTANTS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
