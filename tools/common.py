# -*- coding: utf-8 -*-
"""Gemeinsame Helfer der Werkzeuge: Versionen, Teilprogramme, Aufruf externer Programme.

Nur Standardbibliothek. Kein Werkzeug schreibt etwas anderes als apps/<typ>/VERSION,
dist/ und (auf ausdruecklichen Wunsch) CHANGELOG.md.
"""
import io
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARTS = ('firmware', 'desktop', 'web')
VERSION_RE = re.compile(r'^(\d+)\.(\d+)\.(\d+)\.(\d+)$')
# Zephyrs eigene Schreibweise derselben Nummer - siehe parse_version_file().
ZEPHYR_KEYS = ('VERSION_MAJOR', 'VERSION_MINOR', 'PATCHLEVEL', 'VERSION_TWEAK')
ZEPHYR_RE = re.compile(r'^[ 	]*(%s)[ 	]*=[ 	]*(\d+)[ 	]*$' % '|'.join(ZEPHYR_KEYS),
                       re.MULTILINE)

OK, SKIPPED, FAILED = 'ok', 'uebersprungen', 'FEHLER'


class Result:
    """Ergebnis eines Schritts. 'ok' heisst: wirklich gelaufen und gut."""

    def __init__(self, part, step, status, detail='', tests=0):
        self.part, self.step, self.status, self.detail, self.tests = part, step, status, detail, tests

    def as_dict(self):
        return {'part': self.part, 'step': self.step, 'status': self.status,
                'detail': self.detail, 'tests': self.tests}


def print_results(results):
    for r in results:
        extra = ' (%d Tests)' % r.tests if r.tests else ''
        say('%-9s %-14s %-14s %s%s' % (r.part, r.step, r.status, r.detail, extra))


# --- Versionen --------------------------------------------------------------

def parse_version(text):
    match = VERSION_RE.match(text.strip())
    if not match:
        raise ValueError('Version braucht vier Zahlen (v.x.y.z), gefunden: %r' % text.strip())
    return tuple(int(n) for n in match.groups())


def parse_version_file(text):
    """Liest beide Schreibweisen derselben vierstelligen Nummer.

    Eine Zeile '0.0.1.0' ist der Hausstandard (version.md). Zephyr liest die
    Datei apps/firmware/VERSION aber selbst, und zwar als Kconfig-Datei
    (Kconfig.zephyr, fest verdrahtet) - dort muessen vier Zuweisungen stehen.
    Darum traegt die Firmware ihre Nummer in Zephyrs Schreibweise; es ist
    dieselbe Nummer, nur anders geschrieben.
    """
    felder = dict((m.group(1), int(m.group(2))) for m in ZEPHYR_RE.finditer(text))
    if felder:
        fehlend = [k for k in ZEPHYR_KEYS if k not in felder]
        if fehlend:
            raise ValueError('in der Zephyr-Schreibweise fehlt: %s' % ', '.join(fehlend))
        return tuple(felder[k] for k in ZEPHYR_KEYS)
    return parse_version(text)


def format_version_file(numbers, zephyr):
    """Erzeugt den Dateiinhalt in der Schreibweise, die die Datei schon hatte."""
    if not zephyr:
        return format_version(numbers) + '\n'
    return ''.join('%s = %d\n' % (key, n) for key, n in zip(ZEPHYR_KEYS, numbers))


def is_zephyr_version(part, root=ROOT):
    try:
        with io.open(version_file(part, root), encoding='utf-8') as handle:
            return bool(ZEPHYR_RE.search(handle.read()))
    except OSError:
        return False


def format_version(numbers):
    return '.'.join(str(n) for n in numbers)


def bump(version, kind):
    """Zaehlt nach version.md hoch: build = z, feature = y, release = x, version = v."""
    v, x, y, z = parse_version(version)
    if kind == 'build':
        z += 1
    elif kind == 'feature':
        y, z = y + 1, 0
    elif kind == 'release':
        x, y, z = x + 1, 0, 0
    elif kind == 'version':
        v, x, y, z = v + 1, 0, 0, 0
    else:
        raise ValueError('unbekannte Art: %s' % kind)
    return format_version((v, x, y, z))


def part_dir(part, root=ROOT):
    return os.path.join(root, 'apps', part)


def version_file(part, root=ROOT):
    return os.path.join(part_dir(part, root), 'VERSION')


def read_version(part, root=ROOT):
    """Gibt die Nummer immer als 'v.x.y.z' zurueck, egal wie sie in der Datei steht."""
    with io.open(version_file(part, root), encoding='utf-8') as handle:
        return format_version(parse_version_file(handle.read()))


# --- Teilprogramme ------------------------------------------------------------

SKIP_DIRS = ('build', 'node_modules', '.git', 'vendor', '__pycache__')


def has_sources(part, root=ROOT):
    """Falsch, solange in apps/<typ> nur VERSION und Platzhalter liegen."""
    for folder, dirs, files in os.walk(part_dir(part, root)):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in files:
            if name not in ('VERSION', '.gitkeep', 'README.md'):
                return True
    return False


def files_with(part, suffixes, root=ROOT):
    found = []
    for folder, dirs, files in os.walk(part_dir(part, root)):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        found += [os.path.join(folder, n) for n in files if n.endswith(suffixes)]
    return sorted(found)


# --- Externe Programme ----------------------------------------------------------

def which(name):
    return shutil.which(name)


def command_line(args):
    """Eine Zeile fuer cmd.exe."""
    return subprocess.list2cmdline([str(a) for a in args])


def run(args, cwd=None, env=None, capture=False, windows=None):
    """Startet ein Programm; unter Windows ueber cmd (SDK-Werkzeuge wie west und
    nrfutil scheitern sonst aus PowerShell). Gibt (Code, Ausgabe) zurueck.
    Fehlt das Programm, ist der Code 127 - nie ein Python-Traceback."""
    on_windows = (os.name == 'nt') if windows is None else windows
    options = {'cwd': cwd, 'env': env}
    if capture:
        options.update(stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       universal_newlines=True, encoding='utf-8', errors='replace')
    try:
        if on_windows:
            done = subprocess.run(command_line(args), shell=True, **options)
        else:
            done = subprocess.run([str(a) for a in args], **options)
    except (FileNotFoundError, NotADirectoryError) as error:
        return 127, str(error)
    return done.returncode, (done.stdout or '') if capture else ''


def say(text=''):
    sys.stdout.write(text + '\n')
    sys.stdout.flush()
