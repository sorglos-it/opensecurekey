# -*- coding: utf-8 -*-
"""Teilprogramme bauen, Version hineinstempeln, Ergebnis nach dist/ legen.

    python tools/build.py firmware            # nRF52840-DK (Standard-Board)
    python tools/build.py firmware --board nrf52840dongle/nrf52840
    python tools/build.py desktop             # fuer diesen Rechner
    python tools/build.py web
    python tools/build.py all

Die Version kommt aus apps/<typ>/VERSION und wird beim Bauen eingestempelt:
  firmware  cmake-Variable OSK_VERSION  (CMakeLists.txt der Firmware muss sie auswerten)
  desktop   Go-Variable main.version    (-ldflags -X, wird still ignoriert, wenn sie fehlt)
  web       "version" in der manifest.json der Kopie im ZIP, nie im Quellbaum

Ehrlichkeit: Ein Schritt ist nur 'ok', wenn wirklich gebaut wurde und eine Datei in
dist/ liegt. Fehlt ein Teilprogramm-Inhalt oder ein Werkzeug (z. B. das nRF Connect
SDK), steht 'uebersprungen' mit Grund da, der Ausstiegscode ist dann 2 (nichts
gebaut) - nie 0 fuer einen Bau, der nicht stattgefunden hat. Standardbibliothek.
"""
import argparse
import glob
import io
import json
import os
import shutil
import sys
import time
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common
from common import OK, SKIPPED, FAILED, Result

DEFAULT_BOARD = 'nrf52840dk/nrf52840'
WEB_EXCLUDE = ('tests', 'node_modules', 'data', 'config', '__pycache__')


def dist_dir(root):
    path = os.path.join(root, 'dist')
    os.makedirs(path, exist_ok=True)
    return path


# --- Firmware -----------------------------------------------------------------

def find_sdk(environ=None, which=common.which):
    """Wie startet man 'west'? Gibt (Vorsatz, Beschreibung) oder (None, Grund).

    1. Eine nRF-Connect-Shell: 'west' im PATH und ZEPHYR_BASE gesetzt.
    2. nrfutil toolchain-manager, wenn OSK_NCS (z. B. v3.0.0) die Version nennt.
    """
    env = os.environ if environ is None else environ
    if which('west') and env.get('ZEPHYR_BASE'):
        return ['west'], 'west mit ZEPHYR_BASE=%s' % env['ZEPHYR_BASE']
    if which('nrfutil') and env.get('OSK_NCS'):
        return (['nrfutil', 'toolchain-manager', 'launch', '--ncs-version', env['OSK_NCS'], '--',
                 'west'], 'nrfutil toolchain-manager, NCS %s' % env['OSK_NCS'])
    return None, ('nRF Connect SDK nicht gefunden: weder "west" im PATH mit gesetztem ZEPHYR_BASE '
                  '(Shell des SDK) noch "nrfutil" mit OSK_NCS=<Version>. Siehe docs/build.md.')


def firmware_outputs(build_dir):
    """Fertige Abbilder im Bauordner: zusammengefuehrt zuerst, sonst die der Anwendung."""
    names = ('merged.hex', 'zephyr.hex', 'zephyr.uf2', 'zephyr.bin', 'zephyr.elf')
    found = {}
    for name in names:
        hits = sorted(glob.glob(os.path.join(build_dir, '**', name), recursive=True),
                      key=lambda p: (p.count(os.sep), p))
        if hits:
            found[name] = hits[0]
    return found


def build_firmware(version, root=common.ROOT, board=DEFAULT_BOARD, environ=None, runner=common.run):
    part = 'firmware'
    if not common.has_sources(part, root):
        return Result(part, 'build', SKIPPED, 'apps/firmware ist noch leer')
    source = common.part_dir(part, root)
    if not os.path.isfile(os.path.join(source, 'CMakeLists.txt')):
        return Result(part, 'build', SKIPPED, 'apps/firmware/CMakeLists.txt fehlt - noch nicht baubar')
    prefix, info = find_sdk(environ)
    if prefix is None:
        return Result(part, 'build', SKIPPED, info)
    build_dir = os.path.join(root, 'build', 'firmware-' + board.replace('/', '-'))
    args = prefix + ['build', '-b', board, '-d', build_dir, source, '--pristine=auto',
                     '--', '-DOSK_VERSION=' + version]
    common.say('Firmware: ' + ' '.join(args))
    code, _ = runner(args, cwd=root)
    if code != 0:
        return Result(part, 'build', FAILED, 'west build endete mit Code %d (%s)' % (code, info))
    outputs = firmware_outputs(build_dir)
    wanted = [n for n in ('merged.hex', 'zephyr.hex') if n in outputs]
    if not wanted:
        return Result(part, 'build', FAILED, 'west meldet Erfolg, aber kein .hex in ' + build_dir)
    tag = board.split('/')[0]
    copied = []
    for name, path in outputs.items():
        if name == 'zephyr.hex' and 'merged.hex' in outputs:
            continue
        target = os.path.join(dist_dir(root), 'osk-firmware-%s-%s%s' % (version, tag, os.path.splitext(name)[1]))
        shutil.copyfile(path, target)
        copied.append(os.path.basename(target))
    return Result(part, 'build', OK, 'Board %s -> dist/%s' % (board, ', '.join(copied)))


# --- Desktop ------------------------------------------------------------------

def go_target():
    code, out = common.run(['go', 'env', 'GOOS', 'GOARCH'], capture=True)
    lines = out.split()
    return (lines[0], lines[1]) if code == 0 and len(lines) >= 2 else (None, None)


def build_desktop(version, root=common.ROOT, targets=None, runner=common.run):
    part = 'desktop'
    if not common.has_sources(part, root):
        return Result(part, 'build', SKIPPED, 'apps/desktop ist noch leer')
    source = common.part_dir(part, root)
    if not os.path.isfile(os.path.join(source, 'go.mod')):
        return Result(part, 'build', SKIPPED, 'apps/desktop/go.mod fehlt - noch nicht baubar')
    if not common.which('go'):
        return Result(part, 'build', SKIPPED, 'Go ist nicht installiert (https://go.dev/dl/)')
    if not targets:
        goos, goarch = go_target()
        if not goos:
            return Result(part, 'build', FAILED, '"go env" lieferte kein GOOS/GOARCH')
        targets = [(goos, goarch)]
    built = []
    for goos, goarch in targets:
        name = 'osk-%s-%s-%s%s' % (version, goos, goarch, '.exe' if goos == 'windows' else '')
        out = os.path.join(dist_dir(root), name)
        env = dict(os.environ, GOOS=goos, GOARCH=goarch, CGO_ENABLED='0')
        args = ['go', 'build', '-trimpath', '-ldflags', '-s -w -X main.version=' + version, '-o', out, '.']
        common.say('Desktop: ' + ' '.join(args))
        code, _ = runner(args, cwd=source, env=env)
        if code != 0 or not os.path.isfile(out):
            return Result(part, 'build', FAILED, 'go build (%s/%s) endete mit Code %d' % (goos, goarch, code))
        built.append(name)
    return Result(part, 'build', OK, 'dist/' + ', '.join(built))


# --- Browser-Erweiterung --------------------------------------------------------

def build_web(version, root=common.ROOT):
    part = 'web'
    if not common.has_sources(part, root):
        return Result(part, 'build', SKIPPED, 'apps/web ist noch leer')
    source = common.part_dir(part, root)
    manifest_path = os.path.join(source, 'manifest.json')
    if not os.path.isfile(manifest_path):
        return Result(part, 'build', SKIPPED, 'apps/web/manifest.json fehlt - noch nicht baubar')
    try:
        with io.open(manifest_path, encoding='utf-8') as handle:
            manifest = json.load(handle)
    except ValueError as error:
        return Result(part, 'build', FAILED, 'manifest.json ist kein gueltiges JSON: %s' % error)
    manifest['version'] = version
    target = os.path.join(dist_dir(root), 'osk-web-%s.zip' % version)
    stamp = time.localtime()[:6]
    count = 0
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for folder, dirs, files in os.walk(source):
            dirs[:] = sorted(d for d in dirs if d not in WEB_EXCLUDE)
            for name in sorted(files):
                path = os.path.join(folder, name)
                rel = os.path.relpath(path, source).replace(os.sep, '/')
                if rel in ('VERSION', 'manifest.json') or name.endswith(('.pyc', '.DS_Store')):
                    continue
                info = zipfile.ZipInfo(rel, stamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                with io.open(path, 'rb') as handle:
                    archive.writestr(info, handle.read())
                count += 1
        info = zipfile.ZipInfo('manifest.json', stamp)
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
    return Result(part, 'build', OK, 'dist/%s (%d Dateien + manifest.json)' % (os.path.basename(target), count))


# --- Aufruf ---------------------------------------------------------------------

def build(part, root=common.ROOT, **options):
    try:
        version = common.read_version(part, root)
    except (OSError, ValueError) as error:
        return Result(part, 'build', FAILED, 'VERSION unbrauchbar: %s' % error)
    if part == 'firmware':
        return build_firmware(version, root, board=options.get('board') or DEFAULT_BOARD)
    if part == 'desktop':
        return build_desktop(version, root, targets=options.get('targets'))
    return build_web(version, root)


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('part', choices=common.PARTS + ('all',))
    parser.add_argument('--board', default=DEFAULT_BOARD, help='nur Firmware (Standard: %(default)s)')
    args = parser.parse_args(argv)
    parts = common.PARTS if args.part == 'all' else (args.part,)
    results = [build(p, root, board=args.board) for p in parts]
    common.print_results(results)
    if any(r.status == FAILED for r in results):
        return 1
    if not any(r.status == OK for r in results):
        common.say('Nichts gebaut.')
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
