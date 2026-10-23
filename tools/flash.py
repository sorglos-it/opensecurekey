# -*- coding: utf-8 -*-
"""Firmware aufs Geraet schreiben.

    python tools/flash.py dk                  # nRF52840-DK ueber den Debugger (J-Link)
    python tools/flash.py dongle              # Dongle: UF2-Datei auf das Laufwerk des Bootloaders
    python tools/flash.py dongle --drive E:\\ # Laufwerk selbst nennen

Nimmt die neueste Datei aus dist/ (osk-firmware-<version>-<board>.hex/.uf2),
oder --file. Vorher: python tools/build.py firmware [--board ...].

UNGEPRUEFT: Es gab bei der Entstehung keine Hardware. Die Befehle folgen der
Dokumentation von Nordic (nrfutil device program, west flash) und der UF2-
Konvention (Laufwerk mit INFO_UF2.TXT), sind aber nie gegen ein Geraet gelaufen.
Das Skript sagt es bei jedem Aufruf. Standardbibliothek.
"""
import argparse
import glob
import os
import shutil
import string
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import build
import common

UNTESTED = ('HINWEIS: flash.py ist ungeprueft - es war noch keine Hardware da. '
            'Danach die Version am Geraet kontrollieren.')


def newest(root, pattern):
    hits = glob.glob(os.path.join(root, 'dist', pattern))
    return max(hits, key=os.path.getmtime) if hits else None


def uf2_drives():
    """Laufwerke, die wie ein UF2-Bootloader aussehen (Datei INFO_UF2.TXT im Stamm)."""
    if os.name == 'nt':
        roots = ['%s:\\' % letter for letter in string.ascii_uppercase]
    else:
        roots = glob.glob('/media/*/*') + glob.glob('/run/media/*/*') + glob.glob('/Volumes/*')
    return [r for r in roots if os.path.isfile(os.path.join(r, 'INFO_UF2.TXT'))]


def flash_dk(path, root, runner=common.run):
    if not path.endswith('.hex'):
        return 'Der DK braucht eine .hex-Datei, gefunden: ' + path
    if common.which('nrfutil'):
        args = ['nrfutil', 'device', 'program', '--firmware', path,
                '--options', 'chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,verify=VERIFY_READ']
        common.say(' '.join(args))
        code, _ = runner(args, cwd=root)
        return None if code == 0 else 'nrfutil device program endete mit Code %d' % code
    sdk, _ = build.find_sdk()
    build_dirs = sorted(glob.glob(os.path.join(root, 'build', 'firmware-*dk*')))
    if sdk and build_dirs:
        args = sdk + ['flash', '-d', build_dirs[-1]]
        common.say(' '.join(args))
        code, _ = runner(args, cwd=root)
        return None if code == 0 else 'west flash endete mit Code %d' % code
    return ('Weder "nrfutil" (mit dem Modul "device") noch das nRF Connect SDK gefunden - '
            'nichts geschrieben. Siehe docs/build.md.')


def flash_dongle(path, drive):
    if not path.endswith('.uf2'):
        return 'Der Dongle braucht eine .uf2-Datei, gefunden: ' + path
    if not drive:
        drives = uf2_drives()
        if len(drives) != 1:
            return ('%s UF2-Laufwerk(e) gefunden - Bootloader starten (Reset-Taste) '
                    'oder --drive angeben.' % (len(drives) or 'Kein'))
        drive = drives[0]
    if not os.path.isdir(drive):
        return 'Laufwerk nicht gefunden: ' + drive
    shutil.copyfile(path, os.path.join(drive, os.path.basename(path)))
    return None     # Der Bootloader startet nach dem Kopieren selbst neu; das Laufwerk verschwindet.


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('target', choices=('dk', 'dongle'))
    parser.add_argument('--file', help='Abbild statt der neuesten Datei aus dist/')
    parser.add_argument('--drive', help='Dongle: Laufwerk des UF2-Bootloaders')
    args = parser.parse_args(argv)
    common.say(UNTESTED)
    kind = 'hex' if args.target == 'dk' else 'uf2'
    path = args.file or newest(root, 'osk-firmware-*.' + kind)
    if not path or not os.path.isfile(path):
        common.say('FEHLER: Keine .%s-Datei in dist/ - zuerst "python tools/build.py firmware" '
                   '(fuer den Dongle: --board nrf52840dongle/nrf52840).' % kind)
        return 1
    error = flash_dk(path, root) if args.target == 'dk' else flash_dongle(path, args.drive)
    if error:
        common.say('FEHLER: ' + error)
        return 1
    common.say('Geschrieben: ' + os.path.basename(path) + ' (ungeprueft, bitte am Geraet gegenpruefen)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
