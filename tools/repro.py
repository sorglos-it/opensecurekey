# -*- coding: utf-8 -*-
"""Zweimal bauen und die Abbilder vergleichen - reproduzierbar oder nicht.

    python tools/repro.py --pack              # das Paket: laeuft ueberall
    python tools/repro.py --firmware          # das Abbild: braucht das SDK
    python tools/repro.py --firmware --variant rsa

WOZU: Wer nicht nachbauen kann, muss glauben. Ein reproduzierbarer Bau ist
die einzige Moeglichkeit, ein veroeffentlichtes Abbild gegen den Quelltext
zu pruefen, ohne dem zu vertrauen, der es gebaut hat - und bei einem
Sicherheitsschluessel ist genau das der Punkt.

WIE GEMESSEN WIRD: zweimal bauen, und die Hashwerte vergleichen. Nicht
"sieht gleich aus", nicht "sollte gleich sein". Gerechnet wird mit
`git hash-object`, nicht mit einer eigenen Hashrechnung: das ist dieselbe
Zahl, die git in jedem Commit fuehrt, und sie laesst sich von Hand
nachrechnen.

ZWEI DINGE, DIE SCHON EINMAL SCHIEFGINGEN - darum wird darauf geachtet:

  - Ein Modul-ZIP trug die BAUZEIT statt eines festen Zeitstempels. Zwei
    Baeufe im Abstand einer Sekunde waren verschieden, und niemand konnte
    sagen, ob sich der Inhalt geaendert hatte.
  - Der BAUORDNER steht im Abbild. Uebersetzer schreiben __FILE__ und
    Pfade in Fehlermeldungen hinein; liegt der zweite Bau in einem anderen
    Ordner, unterscheiden sich die Abbilder, obwohl der Quelltext derselbe
    ist. Darum wird hier ZWEIMAL IN VERSCHIEDENEN ORDNERN gebaut: ein Bau,
    der nur im selben Ordner reproduzierbar ist, ist es nicht.

Scheitert der Vergleich, nennt dieses Werkzeug die ERSTE abweichende Stelle
und was dort steht. Das ist meistens der ganze Befund: ein Zeitstempel sieht
wie ein Zeitstempel aus, ein Pfad wie ein Pfad.

Nur Standardbibliothek.
"""
import argparse
import io
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import build as build_mod
import common
import variant as variant_mod


def git_hash(path):
    """Der Hashwert einer Datei, gerechnet von git. (Hash, None) oder (None, Grund).

    `git hash-object` statt einer eigenen Hashrechnung: dieselbe Zahl, die git
    in jedem Commit fuehrt, von Hand nachrechenbar und von niemandem hier
    erfunden.
    """
    if not os.path.isfile(path):
        return None, 'gibt es nicht: %s' % path
    code, out = common.run(['git', 'hash-object', '--', path], capture=True)
    if code != 0:
        return None, 'git hash-object endete mit Code %d: %s' % (code, (out or '').strip())
    value = (out or '').strip().splitlines()
    if not value or len(value[0]) != 40:
        return None, 'git hash-object lieferte keinen Hashwert: %r' % (out,)
    return value[0], None


def first_difference(left, right):
    """Die erste Stelle, an der sich zwei Dateien unterscheiden. Gibt Text.

    Der Befund ohne Begruendung: WO es abweicht und WAS dort steht. Was das
    bedeutet, entscheidet der Mensch - ein Werkzeug, das hier raet, schickt
    die Suche in die falsche Richtung.
    """
    with io.open(left, 'rb') as a, io.open(right, 'rb') as b:
        first, second = a.read(), b.read()
    if len(first) != len(second):
        head = 'verschieden lang: %d und %d Byte' % (len(first), len(second))
    else:
        head = 'gleich lang: %d Byte' % len(first)
    shortest = min(len(first), len(second))
    for at in range(shortest):
        if first[at] != second[at]:
            start = max(0, at - 24)
            return ('%s; erste Abweichung bei Byte %d (%#x)\n'
                    '  Bau 1: %r\n  Bau 2: %r'
                    % (head, at, at, first[start:at + 40], second[start:at + 40]))
    return '%s; die ersten %d Byte sind gleich, der Rest ist laenger' % (head, shortest)


def compare(label, left, right):
    """Vergleicht zwei Dateien. (gleich, Zeilen)."""
    lines = []
    hash_left, why = git_hash(left)
    if hash_left is None:
        return False, ['%s: Bau 1 nicht messbar - %s' % (label, why)]
    hash_right, why = git_hash(right)
    if hash_right is None:
        return False, ['%s: Bau 2 nicht messbar - %s' % (label, why)]
    lines.append('%s  Bau 1: %s' % (label, hash_left))
    lines.append('%s  Bau 2: %s' % (label, hash_right))
    if hash_left == hash_right:
        lines.append('%s  GLEICH - reproduzierbar' % label)
        return True, lines
    lines.append('%s  VERSCHIEDEN - nicht reproduzierbar' % label)
    lines.append('%s  %s' % (label, first_difference(left, right)))
    return False, lines


# --- Das Paket: laeuft ohne SDK ----------------------------------------------

def check_pack(root=common.ROOT):
    """Ergibt zweimaliges Verpacken dasselbe Paket? (ok, Zeilen).

    Das ist der Teil des Weges, der OHNE nRF Connect SDK messbar ist: aus
    demselben Abbild und demselben Schluessel muss zweimal dasselbe Paket
    entstehen. Haengt ein Zeitstempel im Kopf - so wie er einmal in einem
    Modul-ZIP hing -, faellt es hier auf und nicht beim Nutzer.

    Der Schluessel dafuer ist ein fester Pruefwert, KEIN echter: er wird hier
    im Programm erzeugt, nie in eine Datei geschrieben und nie ins Repo
    gelegt. Ein echter Schluessel liegt ausserhalb des Projektordners, und
    diese Messung braucht keinen.
    """
    import sign
    lines, ok = [], True
    folder = tempfile.mkdtemp(prefix='osk-repro-pack-')
    try:
        # Fester Pruefwert statt os.urandom: zwei Laeufe muessen vergleichbar
        # sein, und ein zufaelliger Schluessel machte jede Messung wertlos.
        seed = bytes(bytearray(range(32)))
        image = bytes(bytearray((i * 7) % 256 for i in range(4096)))
        paths = []
        for run in (1, 2):
            data = sign.pack(seed, sign.BOARDS['osk_v1'], '0.0.1.0', 1, image)
            path = os.path.join(folder, 'paket-%d.oskfw' % run)
            with io.open(path, 'wb') as handle:
                handle.write(data)
            paths.append(path)
        same, said = compare('Paket (tools/sign.py)', paths[0], paths[1])
        lines += said
        ok = ok and same

        # Und dasselbe fuer den Abbildkopf mit dem AAGUID - beide Fassungen.
        for which in variant_mod.VARIANTS:
            paths = []
            for run in (1, 2):
                data = variant_mod.build_image(image, which, root=root)
                path = os.path.join(folder, 'abbild-%s-%d.bin' % (which, run))
                with io.open(path, 'wb') as handle:
                    handle.write(data)
                paths.append(path)
            same, said = compare('Abbildkopf %-5s' % which, paths[0], paths[1])
            lines += said
            ok = ok and same
    finally:
        shutil.rmtree(folder, ignore_errors=True)
    return ok, lines


# --- Das Abbild: braucht das SDK ---------------------------------------------

def check_firmware(root=common.ROOT, board=build_mod.DEFAULT_BOARD,
                   osk_variant=build_mod.DEFAULT_VARIANT):
    """Ergibt zweimaliges Bauen dasselbe Abbild? (ok oder None, Zeilen).

    None heisst: nicht gemessen, weil das SDK fehlt - nicht "in Ordnung".

    Gebaut wird in ZWEI VERSCHIEDENEN Ordnern mit verschieden LANGEN Namen.
    Gleich lange Pfade wuerden einen Unterschied verdecken, den der
    Uebersetzer sonst hineinschreibt: viele Werkzeuge fuellen Pfade auf
    gleiche Laenge auf, und dann sieht ein Bau reproduzierbar aus, der es
    nur im selben Ordner ist.
    """
    prefix, info = build_mod.find_sdk()
    if prefix is None:
        return None, ['Firmware-Abbild: NICHT GEMESSEN - ' + info,
                      'Das ist kein Ergebnis. Zwei Abbilder, von denen keines gebaut wurde, '
                      'sind nicht gleich - sie sind nicht da.']
    key, why = build_mod.mcuboot_key(root=root)
    if key is None:
        return None, ['Firmware-Abbild: NICHT GEMESSEN - ' + why]
    version = common.read_version('firmware', root)
    lines, built = [], []
    base = tempfile.mkdtemp(prefix='osk-repro-')
    try:
        for run, tail in ((1, 'a'), (2, 'zweiter-bau-mit-laengerem-pfad')):
            folder = os.path.join(base, tail)
            result = build_mod.build_firmware(version, root, board=board,
                                              osk_variant=osk_variant, build_dir=folder)
            lines.append('Bau %d (%s): %s %s' % (run, tail, result.status, result.detail))
            if result.status != common.OK:
                return False, lines + ['Der Vergleich entfaellt: ein Bau ist nicht '
                                       'durchgelaufen.']
            outputs = build_mod.firmware_outputs(folder)
            name = 'zephyr.bin' if 'zephyr.bin' in outputs else sorted(outputs)[0]
            keep = os.path.join(base, 'bau-%d-%s' % (run, name))
            shutil.copyfile(outputs[name], keep)
            built.append((name, keep))
        same, said = compare('Abbild %s' % built[0][0], built[0][1], built[1][1])
        return same, lines + said
    finally:
        shutil.rmtree(base, ignore_errors=True)


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--pack', action='store_true', help='Paket und Abbildkopf (ohne SDK)')
    parser.add_argument('--firmware', action='store_true', help='das ARM-Abbild (braucht das SDK)')
    parser.add_argument('--board', default=build_mod.DEFAULT_BOARD)
    parser.add_argument('--variant', default=build_mod.DEFAULT_VARIANT,
                        choices=variant_mod.VARIANTS)
    args = parser.parse_args(argv)
    if not args.pack and not args.firmware:
        args.pack = args.firmware = True

    results = []
    if args.pack:
        ok, lines = check_pack(root)
        common.say('\n'.join(lines))
        common.say('')
        results.append(ok)
    if args.firmware:
        ok, lines = check_firmware(root, args.board, args.variant)
        common.say('\n'.join(lines))
        common.say('')
        results.append(ok)

    if False in results:
        common.say('Ergebnis: NICHT reproduzierbar - siehe die Abweichung oben.')
        return 1
    if None in results:
        common.say('Ergebnis: zum Teil nicht gemessen. Was nicht gemessen ist, gilt nicht '
                   'als reproduzierbar.')
        return 2
    common.say('Ergebnis: reproduzierbar, fuer das Gemessene.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
