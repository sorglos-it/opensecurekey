# -*- coding: utf-8 -*-
"""Laesst jedes Beispiel von osk-guard gegen die Attrappe laufen.

    python shared/osk-guard/tests/run.py
    python shared/osk-guard/tests/run.py --only go,php

Je Sprache wird das Beispiel zweimal gefahren: einmal gegen einen Chip, der
alles kann, und einmal gegen einen, der **keinen Schluesselaustausch** kann
(ein geliefertes SE051 im FIPS-Modus). Der zweite Lauf prueft, dass das
Beispiel sagt, welcher Anwendungsfall wegfaellt, statt abzubrechen.

Ausstiegscode 0 nur, wenn kein Lauf scheiterte UND mindestens einer lief.
Eine uebersprungene Sprache ist kein Bestanden - sie steht mit Grund dabei.

Kein echtes Geraet ist dabei: die Attrappe liegt im Arbeitsspeicher, der
Tastendruck ist behauptet. Nur Standardbibliothek.
"""
import argparse
import io
import json
import os
import shutil
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
GUARD = os.path.dirname(HERE)
SIM = os.path.join(HERE, 'guard-sim')

OK, SKIPPED, FAILED = 'ok', 'uebersprungen', 'FEHLER'


class Result(object):
    def __init__(self, language, mode, status, detail, lines=0):
        self.language = language
        self.mode = mode
        self.status = status
        self.detail = detail
        self.lines = lines


def say(text):
    sys.stdout.write(text + '\n')
    sys.stdout.flush()


def php_command():
    """PHP braucht die Erweiterung sodium - sie gehoert zu PHP selbst, ist
    aber nicht in jeder Installation eingeschaltet. Erst ohne Zutun
    probieren, dann mit dem Pfad der Erweiterung."""
    php = shutil.which('php')
    if php is None:
        return None, 'PHP ist nicht installiert'
    probe = [php, '-r', 'exit(extension_loaded("sodium") ? 0 : 3);']
    if subprocess.run(probe, capture_output=True).returncode == 0:
        return [php], None
    ext = os.path.join(os.path.dirname(php), 'ext')
    if os.path.isdir(ext):
        with_ext = [php, '-d', 'extension_dir=' + ext, '-d', 'extension=sodium']
        if subprocess.run(with_ext + ['-r',
                          'exit(extension_loaded("sodium") ? 0 : 3);'],
                          capture_output=True).returncode == 0:
            return with_ext, None
    return None, ('die PHP-Erweiterung sodium ist aus - einschalten mit '
                  'extension=sodium in der php.ini')


def dotnet_ready():
    if shutil.which('dotnet') is None:
        return None, 'dotnet ist nicht installiert'
    done = subprocess.run(['dotnet', '--list-sdks'], capture_output=True,
                          text=True, errors='replace')
    if done.returncode != 0 or not (done.stdout or '').strip():
        return None, 'dotnet ist da, aber kein SDK (dotnet --list-sdks ist leer)'
    return True, None


def languages():
    """Je Sprache: wie das Beispiel gestartet wird, und warum nicht."""
    out = []

    # Go
    if shutil.which('go') is None:
        out.append(('go', None, 'Go ist nicht installiert'))
    elif not os.path.isdir(os.path.join(GUARD, 'go', 'example')):
        out.append(('go', None, 'shared/osk-guard/go/example fehlt'))
    else:
        out.append(('go', {'cmd': ['go', 'run', './example'],
                           'cwd': os.path.join(GUARD, 'go'),
                           'flag': '-stand'}, None))

    # PHP
    php, why = php_command()
    script = os.path.join(GUARD, 'php', 'example.php')
    if php is None:
        out.append(('php', None, why))
    elif not os.path.isfile(script):
        out.append(('php', None, 'shared/osk-guard/php/example.php fehlt'))
    else:
        out.append(('php', {'cmd': php + [script],
                            'cwd': os.path.join(GUARD, 'php'),
                            'flag': '--stand'}, None))

    # Python
    script = os.path.join(GUARD, 'python', 'example.py')
    if not os.path.isfile(script):
        out.append(('python', None, 'shared/osk-guard/python/example.py fehlt'))
    else:
        out.append(('python', {'cmd': [sys.executable, script],
                               'cwd': os.path.join(GUARD, 'python'),
                               'flag': '--stand'}, None))

    # C
    folder = os.path.join(GUARD, 'c')
    if not os.path.isfile(os.path.join(folder, 'Makefile')):
        out.append(('c', None, 'shared/osk-guard/c/Makefile fehlt'))
    elif shutil.which('make') is None:
        out.append(('c', None, 'make ist nicht installiert'))
    elif shutil.which('gcc') is None and shutil.which('cc') is None:
        out.append(('c', None, 'kein C-Uebersetzer gefunden (gcc, cc)'))
    else:
        out.append(('c', {'cmd': None, 'cwd': folder, 'flag': '-stand',
                          'build': ['make'], 'binary': 'example'}, None))

    # .NET
    folder = os.path.join(GUARD, 'dotnet', 'example')
    ready, why = dotnet_ready()
    if not os.path.isdir(folder):
        out.append(('dotnet', None, 'shared/osk-guard/dotnet/example fehlt'))
    elif ready is None:
        out.append(('dotnet', None, why))
    else:
        out.append(('dotnet', {'cmd': ['dotnet', 'run', '--nologo', '--'],
                               'cwd': folder, 'flag': '--stand'}, None))
    return out


def free_port():
    probe = socket.socket()
    probe.bind(('127.0.0.1', 0))
    port = probe.getsockname()[1]
    probe.close()
    return port


class Simulator(object):
    """Die Attrappe samt lokalem Dienst. Beendet wird sie ueber ihre PID -
    pkill greift auf Windows nicht."""

    def __init__(self, stand_path, no_ecdh):
        self.stand_path = stand_path
        self.no_ecdh = no_ecdh
        self.process = None

    def __enter__(self):
        if os.path.exists(self.stand_path):
            os.remove(self.stand_path)
        command = ['go', 'run', '.', '-out', self.stand_path,
                   '-port', str(free_port())]
        if self.no_ecdh:
            command.append('-no-ecdh')
        self.process = subprocess.Popen(command, cwd=SIM,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True,
                                        errors='replace')
        # Warten, bis die Standdatei da ist - nicht blind schlafen.
        for _ in range(600):
            if os.path.isfile(self.stand_path):
                time.sleep(0.2)   # die Datei wird umbenannt, nicht gestueckelt
                return self
            if self.process.poll() is not None:
                raise SystemExit('ABBRUCH: die Attrappe startete nicht:\n'
                                 + (self.process.stdout.read() or ''))
            time.sleep(0.1)
        raise SystemExit('ABBRUCH: die Attrappe schrieb keinen Stand')

    def __exit__(self, *ignored):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
        if os.path.exists(self.stand_path):
            os.remove(self.stand_path)
        return False


def run_example(language, how, stand_path, no_ecdh):
    mode = 'ohne-ecdh' if no_ecdh else 'voll'
    if how.get('build'):
        built = subprocess.run(how['build'], cwd=how['cwd'],
                               capture_output=True, text=True, errors='replace')
        if built.returncode != 0:
            tail = ((built.stdout or '') + (built.stderr or '')).strip()
            say(tail[-1500:])
            return Result(language, mode, FAILED, 'Bau scheitert')
    command = how['cmd']
    if command is None:
        binary = os.path.join(how['cwd'], how['binary'])
        if not os.path.isfile(binary) and os.path.isfile(binary + '.exe'):
            binary += '.exe'
        if not os.path.isfile(binary):
            return Result(language, mode, FAILED,
                          'nach dem Bau fehlt %s' % how['binary'])
        command = [binary]
    done = subprocess.run(command + [how['flag'], stand_path], cwd=how['cwd'],
                          capture_output=True, text=True, errors='replace')
    out = (done.stdout or '') + (done.stderr or '')
    lines = [line for line in out.splitlines()
             if line.strip().startswith('ergebnis:')]
    if done.returncode != 0:
        say(out.strip()[-1500:])
        return Result(language, mode, FAILED,
                      'Ausstiegscode %d' % done.returncode, len(lines))
    # Ein Lauf ohne eine einzige Ergebniszeile hat nichts geprueft.
    if len(lines) < 4:
        say(out.strip()[-1500:])
        return Result(language, mode, FAILED,
                      'nur %d Ergebniszeilen - das Beispiel hat kaum etwas '
                      'geprueft' % len(lines), len(lines))
    # Im Lauf ohne ECDH muss das Beispiel den Fall benennen.
    if no_ecdh:
        named = any('schluesselaustausch' in line.lower() for line in lines)
        if not named:
            say('\n'.join(lines))
            return Result(language, mode, FAILED,
                          'der fehlende Schluesselaustausch wird nicht '
                          'benannt', len(lines))
    return Result(language, mode, OK, '%d Ergebniszeilen' % len(lines),
                  len(lines))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--only', help='nur diese Sprachen, mit Komma getrennt')
    parser.add_argument('--report', help='JSON-Bericht schreiben')
    args = parser.parse_args(argv)

    if shutil.which('go') is None:
        say('ABBRUCH: ohne Go laeuft die Attrappe nicht - dann ist keine '
            'Sprache pruefbar.')
        return 1

    wanted = None
    if args.only:
        wanted = set(part.strip() for part in args.only.split(',') if part.strip())

    plan = languages()
    results = []
    for language, how, why in plan:
        if wanted is not None and language not in wanted:
            continue
        if how is None:
            results.append(Result(language, '-', SKIPPED, why))
            say('%-8s %s: %s' % (language, SKIPPED, why))
            continue
        for no_ecdh in (False, True):
            stand = os.path.join(how['cwd'], 'stand-pruefung.json')
            with Simulator(stand, no_ecdh) as sim:
                result = run_example(language, how, stand, no_ecdh)
            results.append(result)
            say('%-8s %-9s %s: %s' % (language, result.mode, result.status,
                                      result.detail))

    ran = [r for r in results if r.status == OK]
    failed = [r for r in results if r.status == FAILED]
    skipped = [r for r in results if r.status == SKIPPED]
    say('')
    say('Gelaufen: %d ok, %d uebersprungen, %d Fehler.' %
        (len(ran), len(skipped), len(failed)))
    if skipped:
        say('Uebersprungen (NICHT geprueft): ' +
            ', '.join('%s (%s)' % (r.language, r.detail) for r in skipped))
    if args.report:
        folder = os.path.dirname(os.path.abspath(args.report))
        if folder:
            os.makedirs(folder, exist_ok=True)
        with io.open(args.report, 'w', encoding='utf-8', newline='\n') as handle:
            json.dump({'results': [vars(r) for r in results]}, handle, indent=2)
    if failed:
        say('FEHLER in: ' + ', '.join('%s/%s' % (r.language, r.mode)
                                      for r in failed))
        return 1
    if not ran:
        say('FEHLER: es lief kein einziges Beispiel - das gilt nicht als '
            'bestanden.')
        return 1
    say('Ergebnis: bestanden (nur fuer das, was oben als ok steht). '
        'Kein echtes Geraet war beteiligt.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
