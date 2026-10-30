# -*- coding: utf-8 -*-
"""Alles pruefen, was ohne Hardware geht - und ehrlich sagen, was gelaufen ist.

    python tools/check.py
    python tools/check.py --report dist/check-report.json

Schritte: Versionsdateien, Werkzeug-Tests, Uebersetzen/Syntax und Tests je Teilprogramm,
Namenskollisionen zwischen den Teilen der Firmware (firmware/symbole),
(Firmware: Bau nur mit nRF Connect SDK, die Pruefungen laufen am PC mit gcc und make),
Sprachdateien (gleiche Schluessel in de, en, fr, it, es),
Formatierung. Jeder Schritt ist 'ok', 'uebersprungen' (mit Grund) oder 'FEHLER'.

Ausstiegscode: 0 nur, wenn kein Schritt scheiterte UND mindestens ein Test lief.
Nichts auszufuehren gilt nie als Erfolg. Standardbibliothek.
"""
import argparse
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import build
import common
import set_version
import symbols
from common import OK, SKIPPED, FAILED, Result

LANGUAGES = ('de', 'en', 'fr', 'it', 'es')


def tail(text, lines=15):
    return '\n'.join(text.strip().splitlines()[-lines:])


def fail(part, step, detail, output=''):
    if output:
        common.say(tail(output))
    return Result(part, step, FAILED, detail)


# --- Werkzeuge ------------------------------------------------------------------

def check_versions(root):
    wrong, lines = set_version.check(root)
    return Result('tools', 'versionen', FAILED if wrong else OK, '; '.join(l.split()[0] + ' ' + ' '.join(l.split()[1:]) for l in lines))


def check_tools(root):
    code, out = common.run([sys.executable, '-m', 'unittest', 'discover', '-s',
                            os.path.join(root, 'tools', 'tests')], cwd=root, capture=True, windows=False)
    match = re.search(r'^Ran (\d+) tests?', out, re.M)
    count = int(match.group(1)) if match else 0
    if code != 0:
        return fail('tools', 'tests', 'Werkzeug-Tests scheitern', out)
    if count == 0:
        return fail('tools', 'tests', 'keine Werkzeug-Tests gefunden')
    return Result('tools', 'tests', OK, 'unittest', count)


def check_python_syntax(root):
    bad = []
    folder = os.path.join(root, 'tools')
    for base, dirs, files in os.walk(folder):
        dirs[:] = [d for d in dirs if d != '__pycache__']
        for name in files:
            if name.endswith('.py'):
                path = os.path.join(base, name)
                with io.open(path, encoding='utf-8') as handle:
                    try:
                        compile(handle.read(), path, 'exec')
                    except SyntaxError as error:
                        bad.append('%s:%s' % (name, error.lineno))
    return Result('tools', 'syntax', FAILED if bad else OK, ', '.join(bad) or 'alle .py uebersetzbar')


# --- Desktop (Go) -----------------------------------------------------------------

def go_ready(root):
    part = 'desktop'
    if not common.has_sources(part, root):
        return 'apps/desktop ist noch leer'
    if not os.path.isfile(os.path.join(common.part_dir(part, root), 'go.mod')):
        return 'apps/desktop/go.mod fehlt'
    if not common.which('go'):
        return 'Go ist nicht installiert'
    return None


def check_desktop(root):
    reason = go_ready(root)
    if reason:
        return [Result('desktop', 'build', SKIPPED, reason), Result('desktop', 'tests', SKIPPED, reason)]
    cwd = common.part_dir('desktop', root)
    results = []
    code, out = common.run(['go', 'vet', './...'], cwd=cwd, capture=True)
    results.append(Result('desktop', 'vet', OK, 'go vet') if code == 0 else fail('desktop', 'vet', 'go vet scheitert', out))
    code, out = common.run(['go', 'build', './...'], cwd=cwd, capture=True)
    results.append(Result('desktop', 'build', OK, 'go build ./...') if code == 0 else fail('desktop', 'build', 'go build scheitert', out))
    code, out = common.run(['go', 'test', '-json', './...'], cwd=cwd, capture=True)
    ran = failed = 0
    for line in out.splitlines():
        try:
            event = json.loads(line)
        except ValueError:
            continue
        if event.get('Test') and event.get('Action') in ('pass', 'fail'):
            ran += 1
            failed += event['Action'] == 'fail'
    if code != 0 or failed:
        results.append(fail('desktop', 'tests', 'go test scheitert (%d von %d Tests rot)' % (failed, ran),
                            '\n'.join(l for l in out.splitlines() if not l.startswith('{'))))
    else:
        results.append(Result('desktop', 'tests', OK, 'go test', ran))
    return results


# --- Web (Browser-Erweiterung) ----------------------------------------------------

def check_web(root):
    part = 'web'
    if not common.has_sources(part, root):
        return [Result(part, 'build', SKIPPED, 'apps/web ist noch leer'),
                Result(part, 'tests', SKIPPED, 'apps/web ist noch leer')]
    results = []
    sources = common.files_with(part, ('.js', '.mjs', '.cjs'), root)
    manifest = os.path.join(common.part_dir(part, root), 'manifest.json')
    if not common.which('node'):
        reason = 'Node.js ist nicht installiert'
        return [Result(part, 'build', SKIPPED, reason), Result(part, 'tests', SKIPPED, reason)]
    bad = []
    for path in sources:
        code, out = common.run(['node', '--check', path], capture=True)
        if code != 0:
            bad.append(os.path.relpath(path, root))
            common.say(tail(out, 6))
    if os.path.isfile(manifest):
        try:
            with io.open(manifest, encoding='utf-8') as handle:
                json.load(handle)
        except ValueError as error:
            bad.append('manifest.json: %s' % error)
    else:
        results.append(Result(part, 'manifest', SKIPPED, 'manifest.json fehlt - noch nicht baubar'))
    if bad:
        results.append(Result(part, 'syntax', FAILED, ', '.join(bad)))
    else:
        results.append(Result(part, 'syntax', OK, '%d JavaScript-Dateien geprueft' % len(sources)))
    test_files = common.files_with(part, ('.test.js', '.test.mjs', '.test.cjs'), root)
    if not test_files:
        results.append(Result(part, 'tests', SKIPPED, 'keine *.test.js in apps/web'))
        return results
    code, out = common.run(['node', '--test', '--test-reporter=tap'] + test_files, cwd=common.part_dir(part, root), capture=True)
    match = re.search(r'^# tests (\d+)', out, re.M)
    count = int(match.group(1)) if match else 0
    if code != 0:
        results.append(fail(part, 'tests', 'node --test scheitert', out))
    elif count == 0:
        results.append(fail(part, 'tests', 'node --test lief, zaehlte aber keinen Test', out))
    else:
        results.append(Result(part, 'tests', OK, 'node --test', count))
    return results


# --- Firmware ---------------------------------------------------------------------

# Die Pruefsaetze melden ihre Zahl unterschiedlich - beide Formen zaehlen.
# ctest rueckt die Ausgabe des Programms ein ("1: 5875 Pruefungen"), darum
# darf der Zeilenanfang nicht verlangt werden.
TEST_LINES = (re.compile(r'^\S+: (\d+) Faelle, \d+ Fehler$', re.M),
              re.compile(r'(?:^|:\s*)(\d+) Pruefungen, \d+ Fehler\s*$', re.M))


def count_cases(output):
    for pattern in TEST_LINES:
        found = [int(m.group(1)) for m in pattern.finditer(output or '')]
        if found:
            return sum(found)
    return 0

SAN_FLAGS = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']


def sanitizers_work():
    """Kann diese Werkzeugkette die Sanitizer wirklich binden? MinGW uebersetzt
    sie, scheitert aber beim Binden - das muss man probieren, nicht raten."""
    probe_c = os.path.join(HERE, '_san_probe.c')
    probe_x = os.path.join(HERE, '_san_probe.out')
    try:
        with io.open(probe_c, 'w', encoding='utf-8') as handle:
            handle.write('int main(void) { return 0; }\n')
        code, _ = common.run([os.environ.get('CC', 'cc')] + SAN_FLAGS +
                             [probe_c, '-o', probe_x], capture=True)
        return code == 0
    except OSError:
        return False
    finally:
        for path in (probe_c, probe_x):
            try:
                os.remove(path)
            except OSError:
                pass


def build_and_run_cmake(folder, extra):
    """Einen CMake-Pruefsatz uebersetzen und laufen lassen. Die Ausgabe des
    Programms wird durchgereicht, damit die Fallzahl gezaehlt werden kann -
    ctest allein meldet nur 'ein Test', nicht die Tausende darin."""
    out_dir = os.path.join(folder, 'build')
    flags = [] if extra else ['-DCMAKE_C_FLAGS=' + ' '.join(SAN_FLAGS),
                              '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(SAN_FLAGS)]
    # Visual Studio ist ein Mehrfach-Erzeuger: die EXE landet in build/Debug/,
    # und ctest findet sie ohne --config nicht ("Not Run"). Darum ueberall
    # dieselbe Bauart angeben, auch wo sie nichts aendert.
    config = ['--config', 'Debug']
    code, out = common.run(['cmake', '-S', folder, '-B', out_dir,
                            '-DCMAKE_BUILD_TYPE=Debug'] + flags, capture=True)
    if code != 0:
        return code, out
    code, build_out = common.run(['cmake', '--build', out_dir] + config, capture=True)
    if code != 0:
        return code, (out or '') + (build_out or '')
    code, test_out = common.run(['ctest', '--test-dir', out_dir, '-C', 'Debug',
                                 '--output-on-failure', '-V'], capture=True)
    return code, test_out


def check_firmware_tests(root):
    """Die Pruefungen der Firmware laufen am PC mit gcc und make - sie brauchen
    das nRF Connect SDK NICHT. Frueher haengten sie am SDK und wurden deshalb
    immer uebersprungen: der groesste Pruefsatz des Projekts lief nie in der CI.
    Je Ordner unter apps/firmware/test/ ein Makefile, Zusammenfassung je Datei
    'name: N Faelle, M Fehler'."""
    part, base = 'firmware', os.path.join(common.part_dir('firmware', root), 'test')
    if not os.path.isdir(base):
        return [Result(part, 'tests', SKIPPED, 'apps/firmware/test fehlt')]
    # Zwei Bauweisen sind im Umlauf: Makefile (CTAP) und CMake (Tresor).
    # Beide muessen laufen, sonst faellt ein Pruefsatz still unter den Tisch -
    # genau das ist dem CTAP-Satz schon einmal passiert.
    suites = []
    for name in sorted(os.listdir(base)):
        folder = os.path.join(base, name)
        if os.path.isfile(os.path.join(folder, 'Makefile')):
            suites.append((name, 'make'))
        elif os.path.isfile(os.path.join(folder, 'CMakeLists.txt')):
            suites.append((name, 'cmake'))
    if not suites:
        return [Result(part, 'tests', SKIPPED, 'kein Makefile und kein CMakeLists.txt unter apps/firmware/test/')]

    needed = sorted({'make' if kind == 'make' else 'cmake' for _, kind in suites})
    missing = [tool for tool in needed
               if common.run([tool, '--version'], capture=True)[0] != 0]
    if missing:
        return [Result(part, 'tests', SKIPPED,
                       '%s fehlt - unter Windows etwa ueber WSL' % ' und '.join(missing))]

    # Die Sanitizer finden Ueberlaeufe, die kein Test sieht - aber MinGW kann sie
    # nicht binden. Probe statt Annahme: scheitert sie, laufen die Pruefungen ohne,
    # und das steht dann auch in der Meldung. Lieber flacher pruefen als gar nicht.
    extra, suffix = [], ''
    if not sanitizers_work():
        extra, suffix = ['SAN='], ' ohne Sanitizer'

    results, total = [], 0
    for suite, kind in suites:
        step, folder = 'tests/' + suite, os.path.join(base, suite)
        if kind == 'make':
            code, out = common.run(['make'] + extra, cwd=folder, capture=True)
        else:
            code, out = build_and_run_cmake(folder, extra)
        count = count_cases(out)
        if code != 0:
            results.append(fail(part, step, 'Pruefungen rot', out))
        elif count == 0:
            # Gruener Code ohne eine einzige Pruefung ist kein Erfolg.
            results.append(fail(part, step, 'keine Pruefung ausgefuehrt', out))
        else:
            results.append(Result(part, step, OK, '%s (%d Faelle)%s' % (kind, count, suffix), count))
            total += count
    return results


def check_firmware_symbols(root):
    """Namenskollisionen zwischen den Teilen der Firmware.

    Der Pruefsatz baut und bindet jeden Teil einzeln (ctap, se050, applets, ...),
    darum sieht er nicht, wenn zwei Teile denselben Namen vergeben. Erst der
    Binder des Gesamtbaus sagt "multiple definition" - am 25.10.2026 beim
    ersten Gesamtbau gleich zweimal (osk_wipe, osk_apdu_parse). Diese Pruefung
    liest die Namen aus dem Quelltext statt zu binden: so laeuft sie auch auf
    einem Rechner ohne Werkzeugkette, und das ist hier die Mehrheit.

    Worauf sich niemand verlassen darf: Kollisionen mit fremdem Code (libc,
    Zephyr, PSA, OpenSSL), Namen aus Makros und aus Assembler findet sie
    nicht. Die ganze Liste der blinden Flecken steht im Kopf von
    tools/symbols.py - sie gehoert neben jeden gruenen Haken hier.
    """
    part, step = 'firmware', 'symbole'
    folder = common.part_dir(part, root)
    if not common.has_sources(part, root):
        return Result(part, step, SKIPPED, 'apps/firmware ist noch leer')
    if not symbols.sources(folder, '.c'):
        return Result(part, step, SKIPPED, 'keine .c-Dateien in apps/firmware')
    doubles, clashes, hints, files, names = symbols.scan(folder)
    problems = []
    for name, kind, places in doubles:
        problems.append('%s (%s) doppelt definiert: %s' % (name, kind, ', '.join(places)))
    for name, places in clashes:
        problems.append('%s mit verschiedenen Rueckgabetypen: %s'
                        % (name, ', '.join('%s %s' % (kind, where) for where, kind in places)))
    extra = ''
    if hints:
        # Kein Fehler: mindestens eine Stelle steht in einem #if, und ob sich
        # zwei Bedingungen ausschliessen, ist am Text nicht zu entscheiden.
        extra = '; %d doppelt, aber bedingt (#if): %s' % (
            len(hints), ', '.join(name for name, _ in hints))
    if problems:
        return Result(part, step, FAILED, '; '.join(problems) + extra)
    return Result(part, step, OK, '%d Dateien, %d globale Namen, keine Kollision%s'
                  % (files, names, extra))


def check_firmware(root):
    part = 'firmware'
    results = []
    prefix, info = build.find_sdk()
    if prefix is None:
        reason = info if common.has_sources(part, root) else 'apps/firmware ist noch leer'
        results.append(Result(part, 'build', SKIPPED, reason))
    else:
        built = build.build_firmware(common.read_version(part, root), root)
        built.step = 'build'
        results.append(built)
    return results + [check_firmware_symbols(root)] + check_firmware_tests(root)


# --- Sprachdateien -----------------------------------------------------------------

def flatten(data, prefix=''):
    keys = set()
    if isinstance(data, dict):
        for key, value in data.items():
            keys |= flatten(value, '%s%s.' % (prefix, key))
    else:
        keys.add(prefix.rstrip('.'))
    return keys


def check_lang(root):
    results = []
    for part in common.PARTS:
        folder = os.path.join(common.part_dir(part, root), 'lang')
        if not os.path.isdir(folder):
            continue
        files = sorted(f for f in os.listdir(folder) if f.endswith('.json'))
        problems, sets = [], {}
        for lang in LANGUAGES:
            if lang + '.json' not in files:
                problems.append('%s.json fehlt' % lang)
        for name in files:
            try:
                with io.open(os.path.join(folder, name), encoding='utf-8') as handle:
                    sets[name[:-5]] = flatten(json.load(handle))
            except ValueError as error:
                problems.append('%s: kein gueltiges JSON (%s)' % (name, error))
        if 'de' in sets:
            for lang, keys in sorted(sets.items()):
                missing, extra = sorted(sets['de'] - keys), sorted(keys - sets['de'])
                if missing:
                    problems.append('%s: fehlt %s' % (lang, ', '.join(missing[:5]) + (' ...' if len(missing) > 5 else '')))
                if extra:
                    problems.append('%s: zu viel %s' % (lang, ', '.join(extra[:5]) + (' ...' if len(extra) > 5 else '')))
        detail = '; '.join(problems) or '%d Sprachen, %d Schluessel' % (len(sets), len(sets.get('de', ())))
        results.append(Result(part, 'sprachen', FAILED if problems else OK, detail))
    if not results:
        results.append(Result('lang', 'sprachen', SKIPPED, 'noch kein apps/*/lang/ vorhanden'))
    return results


# --- Formatierung -----------------------------------------------------------------

def check_format(root):
    results = []
    if common.has_sources('desktop', root) and common.which('gofmt'):
        code, out = common.run(['gofmt', '-l', common.part_dir('desktop', root)], capture=True)
        listed = out.strip()
        results.append(Result('desktop', 'format', FAILED if listed or code else OK,
                              ('nicht formatiert: ' + ', '.join(listed.split())) if listed or code else 'gofmt'))
    else:
        results.append(Result('desktop', 'format', SKIPPED, 'kein Go-Code oder gofmt fehlt'))
    c_files = [f for f in common.files_with('firmware', ('.c', '.h'), root)]
    has_config = os.path.isfile(os.path.join(root, '.clang-format'))
    if c_files and has_config and common.which('clang-format'):
        # In Haeppchen, weil cmd.exe bei 8191 Zeichen abschneidet: bei ueber
        # hundert Dateien scheiterte der Aufruf mit "Die Befehlszeile ist zu
        # lang" - und das sah nach einem Formatfehler aus, obwohl jede Datei
        # sauber war. Gefunden vom Tresor-Strang, als seine Dateizahl wuchs.
        failed_out, ok = [], True
        for start in range(0, len(c_files), 40):
            code, out = common.run(['clang-format', '--dry-run', '-Werror'] +
                                   c_files[start:start + 40], capture=True)
            if code != 0:
                ok = False
                failed_out.append(out or '')
        results.append(Result('firmware', 'format', OK, 'clang-format (%d Dateien)' % len(c_files))
                       if ok else fail('firmware', 'format',
                                       'clang-format meldet Abweichungen',
                                       '\n'.join(failed_out)))
    else:
        why = 'keine C-Dateien' if not c_files else ('.clang-format fehlt' if not has_config else 'clang-format fehlt')
        results.append(Result('firmware', 'format', SKIPPED, why))
    bad = []
    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in ('.git', 'dist', 'build', 'node_modules', 'vendor', '__pycache__')]
        for name in files:
            if name.endswith('.json'):
                path = os.path.join(base, name)
                try:
                    with io.open(path, encoding='utf-8') as handle:
                        json.load(handle)
                except ValueError as error:
                    bad.append('%s (%s)' % (os.path.relpath(path, root), error))
    results.append(Result('repo', 'json', FAILED if bad else OK, '; '.join(bad) or 'alle .json gueltig'))
    return results


def check_guard(root=common.ROOT):
    """Der Vertrag der fuenf osk-guard-Fassungen gegen die Quellen.

    Braucht keinen Chip, keinen Uebersetzer und keine Attrappe - er liest
    die Dateien. Darum gehoert er hierher und nicht in run.py: er laeuft
    immer, auch auf einem Rechner ohne Werkzeugkette.

    Siehe docs/dongle-bau.md."""
    script = os.path.join(root, 'shared', 'osk-guard', 'tests', 'vertrag.py')
    if not os.path.isfile(script):
        return [Result('guard', 'vertrag', SKIPPED,
                       'shared/osk-guard/tests/vertrag.py fehlt')]
    code, out = common.run([sys.executable, script], cwd=root, capture=True)
    count = 0
    for line in (out or '').splitlines():
        if line.startswith('vertrag: '):
            parts = line.split()
            if len(parts) > 1 and parts[1].isdigit():
                count = int(parts[1])
    if code != 0:
        return [fail('guard', 'vertrag', 'der Vertrag ist verletzt', out)]
    if count == 0:
        return [fail('guard', 'vertrag', 'kein einziger Fall geprueft', out)]
    return [Result('guard', 'vertrag', OK,
                   'contract.json gegen fuenf Sprachen (%d Faelle)' % count,
                   count)]


# --- Gesamt -----------------------------------------------------------------------

def run_all(root=common.ROOT):
    results = [check_versions(root), check_tools(root), check_python_syntax(root)]
    results += check_firmware(root) + check_desktop(root) + check_web(root)
    results += check_lang(root) + check_format(root)
    results += check_guard(root)
    return results


def summarize(results):
    """(Ausstiegscode, Text). 0 nur ohne Fehler und mit mindestens einem gelaufenen Test."""
    tests = sum(r.tests for r in results)
    failed = [r for r in results if r.status == FAILED]
    skipped = [r for r in results if r.status == SKIPPED]
    ran = [r for r in results if r.status == OK]
    lines = ['', 'Gelaufen: %d Schritte ok, %d uebersprungen, %d Fehler; %d Tests ausgefuehrt.'
             % (len(ran), len(skipped), len(failed), tests)]
    if skipped:
        lines.append('Uebersprungen (NICHT geprueft): ' + ', '.join('%s/%s' % (r.part, r.step) for r in skipped))
    if failed:
        lines.append('FEHLER in: ' + ', '.join('%s/%s' % (r.part, r.step) for r in failed))
        return 1, '\n'.join(lines)
    if tests == 0:
        lines.append('FEHLER: Es lief kein einziger Test - das gilt nicht als bestanden.')
        return 1, '\n'.join(lines)
    lines.append('Ergebnis: bestanden (nur fuer das, was oben als ok steht).')
    return 0, '\n'.join(lines)


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--report', help='JSON-Bericht schreiben (die CI liest ihn gegen)')
    args = parser.parse_args(argv)
    try:
        results = run_all(root)
    except Exception as error:      # ein kaputtes Werkzeug darf nie als Erfolg enden
        common.say('FEHLER: check.py selbst ist abgestuerzt: %r' % (error,))
        return 1
    common.print_results(results)
    code, text = summarize(results)
    common.say(text)
    if args.report:
        os.makedirs(os.path.dirname(os.path.abspath(args.report)), exist_ok=True)
        with io.open(args.report, 'w', encoding='utf-8', newline='\n') as handle:
            json.dump({'exit_code': code, 'tests_run': sum(r.tests for r in results),
                       'results': [r.as_dict() for r in results]}, handle, indent=2)
    return code


if __name__ == '__main__':
    sys.exit(main())
