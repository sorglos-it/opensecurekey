# -*- coding: utf-8 -*-
"""Alles pruefen, was ohne Hardware geht - und ehrlich sagen, was gelaufen ist.

    python tools/check.py
    python tools/check.py --report dist/check-report.json

Schritte: Versionsdateien, Werkzeug-Tests, Uebersetzen/Syntax und Tests je Teilprogramm
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

TEST_LINE = re.compile(r'^(\S+): (\d+) Faelle, (\d+) Fehler$', re.M)

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


def check_firmware_tests(root):
    """Die Pruefungen der Firmware laufen am PC mit gcc und make - sie brauchen
    das nRF Connect SDK NICHT. Frueher haengten sie am SDK und wurden deshalb
    immer uebersprungen: der groesste Pruefsatz des Projekts lief nie in der CI.
    Je Ordner unter apps/firmware/test/ ein Makefile, Zusammenfassung je Datei
    'name: N Faelle, M Fehler'."""
    part, base = 'firmware', os.path.join(common.part_dir('firmware', root), 'test')
    if not os.path.isdir(base):
        return [Result(part, 'tests', SKIPPED, 'apps/firmware/test fehlt')]
    suites = sorted(d for d in os.listdir(base)
                    if os.path.isfile(os.path.join(base, d, 'Makefile')))
    if not suites:
        return [Result(part, 'tests', SKIPPED, 'kein Makefile unter apps/firmware/test/')]
    if common.run(['make', '--version'], capture=True)[0] != 0:
        return [Result(part, 'tests', SKIPPED, 'make fehlt - unter Windows etwa ueber WSL')]

    # Die Sanitizer finden Ueberlaeufe, die kein Test sieht - aber MinGW kann sie
    # nicht binden. Probe statt Annahme: scheitert sie, laufen die Pruefungen ohne,
    # und das steht dann auch in der Meldung. Lieber flacher pruefen als gar nicht.
    extra, suffix = [], ''
    if not sanitizers_work():
        extra, suffix = ['SAN='], ' ohne Sanitizer'

    results, total = [], 0
    for suite in suites:
        step = 'tests/' + suite
        code, out = common.run(['make'] + extra, cwd=os.path.join(base, suite), capture=True)
        count = sum(int(m.group(2)) for m in TEST_LINE.finditer(out or ''))
        if code != 0:
            results.append(fail(part, step, 'Pruefungen rot', out))
        elif count == 0:
            # Gruener Code ohne eine einzige Pruefung ist kein Erfolg.
            results.append(fail(part, step, 'keine Pruefung ausgefuehrt', out))
        else:
            results.append(Result(part, step, OK, 'make (%d Faelle)%s' % (count, suffix), count))
            total += count
    return results


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
    return results + check_firmware_tests(root)


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
        code, out = common.run(['clang-format', '--dry-run', '-Werror'] + c_files, capture=True)
        results.append(Result('firmware', 'format', OK, 'clang-format') if code == 0
                       else fail('firmware', 'format', 'clang-format meldet Abweichungen', out))
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


# --- Gesamt -----------------------------------------------------------------------

def run_all(root=common.ROOT):
    results = [check_versions(root), check_tools(root), check_python_syntax(root)]
    results += check_firmware(root) + check_desktop(root) + check_web(root)
    results += check_lang(root) + check_format(root)
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
