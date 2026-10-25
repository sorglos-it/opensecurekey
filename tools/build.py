# -*- coding: utf-8 -*-
"""Teilprogramme bauen, Version hineinstempeln, Ergebnis nach dist/ legen.

    python tools/build.py firmware            # nRF52840-DK (Standard-Board)
    python tools/build.py firmware --board nrf52840dongle/nrf52840
    python tools/build.py desktop             # fuer diesen Rechner
    python tools/build.py web
    python tools/build.py all

Die Version kommt aus apps/<typ>/VERSION und wird beim Bauen eingestempelt:
  firmware  OSK_VERSION - als Umgebungsvariable UND als cmake-Variable (die
            CMakeLists.txt der Firmware muss sie auswerten). Warum beides: bei
            einem Sysbuild laeuft cmake zweimal, und alles hinter '--' landet
            nur im aeusseren Lauf, nicht im Abbild. Nach dem Bau wird darum
            nachgesehen, ob die Version wirklich im Abbild steht; steht sie
            nicht drin, ist der Schritt FEHLER - ein Abbild ohne Versions-
            nummer gilt nicht als gebaut.
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

def find_sdk(environ=None, which=None):
    """Wie startet man 'west'? Gibt (Vorsatz, Beschreibung) oder (None, Grund).

    1. Eine nRF-Connect-Shell: 'west' im PATH und ZEPHYR_BASE gesetzt.
    2. nrfutil toolchain-manager, wenn OSK_NCS (z. B. v3.0.0) die Version nennt.
    """
    env = os.environ if environ is None else environ
    which = common.which if which is None else which
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


def cmake_caches(build_dir):
    """Alle CMakeCache.txt im Bauordner, der aeussere zuerst."""
    found = []
    for base, dirs, files in os.walk(build_dir):
        dirs[:] = [d for d in dirs if d != 'CMakeFiles']
        if 'CMakeCache.txt' in files:
            found.append(os.path.join(base, 'CMakeCache.txt'))
    return sorted(found, key=lambda p: (p.count(os.sep), p))


def cache_value(path, name):
    """Wert eines Eintrags im Zwischenspeicher, oder None."""
    try:
        with io.open(path, encoding='utf-8', errors='replace') as handle:
            for line in handle:
                left, _, value = line.partition('=')
                if left.split(':')[0].strip() == name:
                    return value.strip()
    except OSError:
        return None
    return None


def version_arrived(build_dir, version, name='OSK_VERSION'):
    """Steht die Version im Abbild - oder nur im aeusseren Bau? (ok, Text)

    Bei einem Sysbuild laeuft CMake zweimal: einmal aussen, einmal je Abbild.
    Alles hinter '--' landet nur im aeusseren Lauf; das Abbild sah die Version
    deshalb nie, und niemand merkte es, weil west trotzdem Erfolg meldete.
    Darum wird hier nachgesehen statt geglaubt. Gemessen am 25.10.2026 mit
    zwei geschachtelten CMake-Laeufen: '-DOSK_VERSION=' und
    '-Dfirmware_OSK_VERSION=' stehen nur im aeusseren Zwischenspeicher, die
    Umgebungsvariable OSK_VERSION kommt im inneren Lauf an.
    """
    caches = cmake_caches(build_dir)
    if not caches:
        return False, 'kein CMakeCache.txt in ' + build_dir
    inner = caches[1:]
    if not inner:
        # Ein einziger CMake-Lauf (kein Sysbuild): der aeussere IST das Abbild.
        got = cache_value(caches[0], name)
        where = os.path.relpath(caches[0], build_dir)
        return (got == version), '%s=%s in %s' % (name, got, where)
    good = [os.path.relpath(p, build_dir) for p in inner if cache_value(p, name) == version]
    if good:
        return True, '%s=%s in %s' % (name, version, ', '.join(good))
    return False, ('%s steht nicht im Abbild: %s hat sie%s, %s nicht'
                   % (name, os.path.relpath(caches[0], build_dir),
                      '' if cache_value(caches[0], name) else ' auch nicht',
                      ', '.join(os.path.relpath(p, build_dir) for p in inner)))


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
    # Die Version wird auf drei Wegen mitgegeben, weil nur einer gemessen
    # ankommt: '-DOSK_VERSION' erreicht den aeusseren Lauf (und bei
    # --no-sysbuild ist der aeussere der einzige), '-Dfirmware_OSK_VERSION'
    # ist die Schreibweise fuer ein einzelnes Abbild, und die
    # Umgebungsvariable erbt jeder geschachtelte CMake-Lauf - das ist der
    # Weg, der im inneren Lauf gemessen ankommt. Die CMakeLists.txt der
    # Firmware liest sie so:
    #   if(NOT OSK_VERSION AND DEFINED ENV{OSK_VERSION})
    #     set(OSK_VERSION "$ENV{OSK_VERSION}" CACHE STRING "")
    #   endif()
    args = prefix + ['build', '-b', board, '-d', build_dir, source, '--pristine=auto',
                     '--', '-DOSK_VERSION=' + version, '-Dfirmware_OSK_VERSION=' + version]
    env = dict(os.environ if environ is None else environ, OSK_VERSION=version)
    common.say('Firmware: OSK_VERSION=%s ' % version + ' '.join(args))
    code, _ = runner(args, cwd=root, env=env)
    if code != 0:
        return Result(part, 'build', FAILED, 'west build endete mit Code %d (%s)' % (code, info))
    outputs = firmware_outputs(build_dir)
    wanted = [n for n in ('merged.hex', 'zephyr.hex') if n in outputs]
    if not wanted:
        return Result(part, 'build', FAILED, 'west meldet Erfolg, aber kein .hex in ' + build_dir)
    arrived, how = version_arrived(build_dir, version)
    if not arrived:
        return Result(part, 'build', FAILED,
                      'west meldet Erfolg, aber die Version kam im Abbild nicht an: ' + how)
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
