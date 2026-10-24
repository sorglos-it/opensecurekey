# -*- coding: utf-8 -*-
"""Versionsnummer eines Teilprogramms hochzaehlen (Standard: version.md, v.x.y.z).

    python tools/set_version.py desktop --build      # z + 1
    python tools/set_version.py firmware --feature   # y + 1, z = 0
    python tools/set_version.py all --release        # x + 1, y = 0, z = 0
    python tools/set_version.py web --version        # v + 1 - nur auf Ansage von Thomas
    python tools/set_version.py --check              # alle VERSION-Dateien pruefen

Geschrieben wird ausschliesslich apps/<typ>/VERSION. Alle anderen Stellen
(Manifest, Programmanzeige, Changelog) holen die Nummer von dort - build.py
stempelt sie beim Bauen. Nur Standardbibliothek.
"""
import argparse
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common


def write_version(part, text, root=common.ROOT):
    """Schreibt die Nummer in der Schreibweise, die die Datei schon hatte.

    Die Firmware fuehrt sie in Zephyrs Schreibweise, weil Zephyr diese Datei
    selbst liest - common.parse_version_file() erklaert, warum.
    """
    inhalt = common.format_version_file(common.parse_version(text),
                                        common.is_zephyr_version(part, root))
    with io.open(common.version_file(part, root), 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(inhalt)


def check(root=common.ROOT):
    """Gibt (Zahl der Fehler, Zeilen) zurueck."""
    wrong, lines = 0, []
    for part in common.PARTS:
        try:
            lines.append('%-9s %s' % (part, common.read_version(part, root)))
        except (OSError, ValueError) as error:
            wrong += 1
            lines.append('%-9s FEHLER: %s' % (part, error))
    return wrong, lines


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('part', nargs='?', choices=common.PARTS + ('all',))
    kind = parser.add_mutually_exclusive_group()
    kind.add_argument('--build', action='store_const', dest='kind', const='build')
    kind.add_argument('--feature', action='store_const', dest='kind', const='feature')
    kind.add_argument('--release', action='store_const', dest='kind', const='release')
    kind.add_argument('--version', action='store_const', dest='kind', const='version',
                      help='nur auf Ansage von Thomas')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)

    if args.check:
        wrong, lines = check(root)
        print('\n'.join(lines))
        return 1 if wrong else 0
    if not args.part or not args.kind:
        parser.error('Teilprogramm und eine Art (--build, --feature, --release) angeben')

    parts = common.PARTS if args.part == 'all' else (args.part,)
    planned = []
    for part in parts:                      # erst alles lesen, dann schreiben: ganz oder gar nicht
        try:
            old = common.read_version(part, root)
        except (OSError, ValueError) as error:
            print('FEHLER %s: %s' % (part, error), file=sys.stderr)
            return 1
        planned.append((part, old, common.bump(old, args.kind)))
    for part, old, new in planned:
        write_version(part, new, root)
        print('%-9s %s -> %s' % (part, old, new))
    return 0


if __name__ == '__main__':
    sys.exit(main())
