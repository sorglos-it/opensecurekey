# -*- coding: utf-8 -*-
"""Teilprogramme bauen, Version hineinstempeln, Ergebnis nach dist/ legen.

    python tools/build.py firmware --variant norsa    # ohne RSA
    python tools/build.py firmware --variant rsa      # mit RSA
    python tools/build.py firmware --variant rsa --board nrf52840dongle/nrf52840
    python tools/build.py desktop             # fuer diesen Rechner
    python tools/build.py web
    python tools/build.py all

ZWEI FASSUNGEN DER FIRMWARE, mit und ohne RSA (--variant). Welche gebaut
wurde, steht danach an drei Stellen, und sie muss ueberall dieselbe sein:
im Dateinamen, im Abbildkopf (AAGUID als geschuetztes TLV) und in der
.config des Baus. Geprueft wird das gleich nach dem Bau von
tools/variant.py - stimmt es nicht, ist der Schritt FEHLER und nicht
"ok mit Warnung". Ein Abbild ohne RSA mit dem AAGUID der Fassung MIT RSA
verspricht dem Browser Faehigkeiten, die das Geraet nicht hat.

DER SIGNATURSCHLUESSEL kommt von aussen (OSK_MCUBOOT_KEY) und liegt nie im
Projektordner. Fehlt er, bricht der Bau ab, BEVOR west startet: ohne ihn
nimmt MCUboot seinen eigenen, oeffentlich bekannten Beispielschluessel, und
der Bau waere gruen. Siehe docs/signatur.md.

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
import binascii
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
import variant as variant_mod
from common import OK, SKIPPED, FAILED, Result

DEFAULT_BOARD = 'nrf52840dk/nrf52840'
DEFAULT_VARIANT = 'norsa'
WEB_EXCLUDE = ('tests', 'node_modules', 'data', 'config', '__pycache__')


def dist_dir(root):
    path = os.path.join(root, 'dist')
    os.makedirs(path, exist_ok=True)
    return path


# --- Firmware -----------------------------------------------------------------

def mcuboot_key(environ=None, root=common.ROOT, isfile=None):
    """Der MCUboot-Signaturschluessel von aussen. (Pfad, None) oder (None, Grund).

    DIE WICHTIGSTE PRUEFUNG DIESER DATEI. Fehlt der Schluessel, nimmt MCUboot
    seinen EIGENEN Beispielschluessel - und der ist oeffentlich bekannt. Ein
    Geraet, das damit gebaut ist, nimmt Firmware von jedem an, der die Datei
    aus dem Netz laedt. Es gibt dafuer keine Warnung: der Bau wird gruen.

    Darum wird hier abgebrochen, BEVOR west startet. Dieselbe Regel steht
    noch einmal in apps/firmware/CMakeLists.txt - zwei Waende, weil ein Bau
    von Hand die hier nicht sieht, und ein Bau ohne Schluessel schlimmer ist
    als kein Bau.

    Drei Gruende fuer ein Nein, und alle drei werden unterschieden: nicht
    gesetzt, liegt nicht da, liegt im Projektordner. Der Pfad kommt in keine
    Meldung, die weitergegeben wird - er verraet, wo der Schluessel liegt.
    """
    env = os.environ if environ is None else environ
    isfile = os.path.isfile if isfile is None else isfile
    given = (env.get('OSK_MCUBOOT_KEY') or '').strip()
    if not given:
        return None, ('OSK_MCUBOOT_KEY ist nicht gesetzt. Ohne Schluessel baut MCUboot mit '
                      'seinem oeffentlich bekannten Beispielschluessel - dann nimmt das Geraet '
                      'Firmware von jedem an. Setzen (PowerShell): '
                      '$env:OSK_MCUBOOT_KEY = "$env:USERPROFILE\\.osk-keys\\mcuboot-ed25519.pem". '
                      'Siehe docs/signatur.md.')
    path = os.path.abspath(given)
    repo = os.path.abspath(root)
    try:
        inside = os.path.commonpath([path, repo]) == repo
    except ValueError:
        inside = False      # verschiedene Laufwerke
    if inside:
        return None, ('Der MCUboot-Schluessel liegt IM Projektordner. Er gehoert ausserhalb - '
                      'verschieben, nicht nur in .gitignore eintragen: ignoriert ist nicht weg '
                      '(tools/keyguard.py sieht beides).')
    if not isfile(path):
        return None, ('OSK_MCUBOOT_KEY zeigt auf eine Datei, die nicht da ist. Der Pfad steht '
                      'hier absichtlich nicht - er verraet, wo der Schluessel liegt.')
    return path, None


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


def build_firmware(version, root=common.ROOT, board=DEFAULT_BOARD, environ=None,
                   runner=common.run, osk_variant=DEFAULT_VARIANT, build_dir=None):
    part = 'firmware'
    if osk_variant not in variant_mod.VARIANTS:
        return Result(part, 'build', FAILED,
                      '--variant %s kennt niemand; es gibt %s'
                      % (osk_variant, ' und '.join(variant_mod.VARIANTS)))
    if not common.has_sources(part, root):
        return Result(part, 'build', SKIPPED, 'apps/firmware ist noch leer')
    source = common.part_dir(part, root)
    if not os.path.isfile(os.path.join(source, 'CMakeLists.txt')):
        return Result(part, 'build', SKIPPED, 'apps/firmware/CMakeLists.txt fehlt - noch nicht baubar')
    conf = os.path.join(source, 'variant-%s.conf' % osk_variant)
    if not os.path.isfile(conf):
        return Result(part, 'build', FAILED,
                      'Die Baudatei der Fassung fehlt: apps/firmware/variant-%s.conf. Ohne sie '
                      'entscheidet die Vorgabe von Kconfig, und niemand weiss, welche Fassung '
                      'herauskam.' % osk_variant)

    # Der Schluessel wird VOR der Suche nach dem SDK geprueft. Sonst heisst es
    # auf einem Rechner ohne SDK "uebersprungen", und dass der Schluessel
    # fehlt, erfaehrt man erst auf dem Rechner, der wirklich baut.
    key, why = mcuboot_key(environ, root)
    if key is None:
        return Result(part, 'build', FAILED, 'Kein Signaturschluessel: ' + why)

    try:
        aaguid = variant_mod.aaguid_for(osk_variant, root)
    except variant_mod.VariantError as error:
        return Result(part, 'build', FAILED, 'AAGUID der Fassung nicht lesbar: %s' % error)

    prefix, info = find_sdk(environ)
    if prefix is None:
        return Result(part, 'build', SKIPPED, info)
    # Jede Fassung hat ihren EIGENEN Bauordner. Zwei Fassungen in einem Ordner
    # heisst: der zweite Bau laeuft auf den Resten des ersten, und welche
    # Fassung im Abbild steckt, haengt daran, was --pristine gerade raeumt.
    if build_dir is None:
        build_dir = os.path.join(root, 'build', 'firmware-%s-%s'
                                 % (board.replace('/', '-'), osk_variant))
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
    #
    # Die Fassung geht auf drei Wegen mit, und jeder hat einen Grund:
    #   EXTRA_CONF_FILE  setzt CONFIG_OSK_RSA im Bau selbst
    #   OSK_AAGUID_TLV   gibt den AAGUID an imgtool weiter - in den Abbildkopf
    #   OSK_MCUBOOT_KEY  in der Umgebung, damit auch der innere CMake-Lauf ihn
    #                    sieht (derselbe Grund wie bei OSK_VERSION)
    args = prefix + ['build', '-b', board, '-d', build_dir, source, '--pristine=auto',
                     '--', '-DOSK_VERSION=' + version, '-Dfirmware_OSK_VERSION=' + version,
                     '-DEXTRA_CONF_FILE=' + conf,
                     '-Dfirmware_EXTRA_CONF_FILE=' + conf,
                     '-DOSK_AAGUID_TLV=0x' + binascii.hexlify(aaguid).decode('ascii'),
                     '-Dfirmware_OSK_AAGUID_TLV=0x' + binascii.hexlify(aaguid).decode('ascii')]
    env = dict(os.environ if environ is None else environ, OSK_VERSION=version,
               OSK_MCUBOOT_KEY=key)
    # Der Schluesselpfad steht ABSICHTLICH nicht in dieser Zeile: Bauprotokolle
    # wandern in Fehlerberichte, und der Pfad verraet, wo der Schluessel liegt.
    common.say('Firmware: OSK_VERSION=%s Fassung=%s (Schluessel von aussen) '
               % (version, osk_variant) + ' '.join(args))
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

    # Kam die FASSUNG an? Dieselbe Frage wie bei der Version, und aus demselben
    # Grund: west meldet Erfolg, auch wenn Kconfig die Zeile still verworfen
    # hat (siehe apps/firmware/prj.conf - 21 verworfene Zeilen, und der Bau
    # lief weiter). Gelesen wird die .config, die Kconfig WIRKLICH geschrieben
    # hat, nicht die .conf, die wir hineingegeben haben.
    got, why = variant_mod.variant_from_build(build_dir)
    if got != osk_variant:
        return Result(part, 'build', FAILED,
                      'west meldet Erfolg, aber gebaut wurde die Fassung "%s" statt "%s" (%s). '
                      'Ein Abbild, dessen Fassung nicht die gewuenschte ist, gehoert in kein '
                      'Geraet.' % (got or 'unbestimmt', osk_variant, why))

    tag = board.split('/')[0]
    copied = []
    for name, path in outputs.items():
        if name == 'zephyr.hex' and 'merged.hex' in outputs:
            continue
        # Der Dateiname wird an EINER Stelle gebildet (variant.name_for),
        # damit er nicht hier und in tools/release.py auseinanderlaeuft.
        target = os.path.join(dist_dir(root),
                              variant_mod.name_for(version, tag, osk_variant,
                                                   os.path.splitext(name)[1]))
        shutil.copyfile(path, target)
        copied.append(os.path.basename(target))

    # Und jetzt das Abbild gegen sich selbst: Dateiname, Abbildkopf und .config
    # muessen dieselbe Fassung nennen. Ohne diesen Schritt steht die Fassung
    # dreimal da und niemand hat sie je verglichen.
    #
    # Geprueft wird die .bin, falls es eine gibt: nur sie traegt den
    # MCUboot-Kopf mit dem AAGUID-TLV. Eine .hex ist Text und hat ihn nicht;
    # fehlt die .bin, sagt die Meldung das, statt den Schritt still
    # durchzulassen.
    checked = [n for n in copied if n.endswith('.bin')]
    if not checked:
        return Result(part, 'build', OK,
                      'Board %s, Fassung %s -> dist/%s (Fassung im Abbildkopf NICHT geprueft: '
                      'keine .bin im Bau, nur dort steht der MCUboot-Kopf)'
                      % (board, osk_variant, ', '.join(copied)))
    good, lines = variant_mod.check(os.path.join(dist_dir(root), checked[0]),
                                    expect=osk_variant, build_dir=build_dir, root=root)
    if not good:
        common.say('\n'.join(lines))
        return Result(part, 'build', FAILED,
                      'Das Abbild ist gebaut, aber die Fassung stimmt nicht ueberall: %s'
                      % lines[-1])
    return Result(part, 'build', OK,
                  'Board %s, Fassung %s (Dateiname, Abbildkopf und .config stimmen) -> dist/%s'
                  % (board, osk_variant, ', '.join(copied)))


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
        return build_firmware(version, root, board=options.get('board') or DEFAULT_BOARD,
                              osk_variant=options.get('osk_variant') or DEFAULT_VARIANT)
    if part == 'desktop':
        return build_desktop(version, root, targets=options.get('targets'))
    return build_web(version, root)


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('part', choices=common.PARTS + ('all',))
    parser.add_argument('--board', default=DEFAULT_BOARD, help='nur Firmware (Standard: %(default)s)')
    parser.add_argument('--variant', default=DEFAULT_VARIANT, choices=variant_mod.VARIANTS,
                        help='nur Firmware: Fassung mit oder ohne RSA (Standard: %(default)s)')
    args = parser.parse_args(argv)
    parts = common.PARTS if args.part == 'all' else (args.part,)
    results = [build(p, root, board=args.board, osk_variant=args.variant) for p in parts]
    common.print_results(results)
    if any(r.status == FAILED for r in results):
        return 1
    if not any(r.status == OK for r in results):
        common.say('Nichts gebaut.')
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
