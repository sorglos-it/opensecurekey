# -*- coding: utf-8 -*-
"""Baut und laesst den Prueflauf der .NET-Fassung laufen.

    python shared/osk-guard/tests/dotnet-test.py

Ohne .NET-SDK wird mit dem Roslyn-Uebersetzer aus den Visual-Studio-
Build-Tools gebaut - dieselbe Suche wie in run.py, die hier nicht ein
zweites Mal geschrieben, sondern eingebunden wird. Das ist der Unterschied
zwischen "gemessen" und "uebersprungen": dass dotnet nicht im PATH steht,
heisst nicht, dass nichts da ist.

Ausstiegscode 0 nur, wenn der Prueflauf wirklich lief und gruen war. Eine
uebersprungene Sprache ist kein Bestanden - sie steht mit Grund da.
"""
import importlib.util
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GUARD = os.path.dirname(HERE)
DOTNET = os.path.join(GUARD, 'dotnet')


def load_run():
    """run.py einbinden - der Bindestrich im eigenen Namen verbietet den
    gewoehnlichen Import, run.py selbst hat keinen."""
    spec = importlib.util.spec_from_file_location(
        'osk_guard_run', os.path.join(HERE, 'run.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    run = load_run()
    ready, tools, why = run.dotnet_ready()
    if ready is None:
        sys.stdout.write('dotnet uebersprungen (NICHT geprueft): %s\n' % why)
        return 1
    sources = [os.path.join('tests', 'TestGuard.cs'), 'OskGuard.cs',
               'Ed25519Verify.cs']
    if ready == 'sdk':
        command = ['dotnet', 'run', '--nologo', '--project',
                   os.path.join(DOTNET, 'tests', 'TestGuard.csproj')]
        cwd = DOTNET
    else:
        csc, runtime = tools
        work = tempfile.mkdtemp(prefix='osk-dotnet-test-')
        dll, reason = run.csc_build(csc, runtime, work, 'TestGuard',
                                    sources, DOTNET)
        if dll is None:
            sys.stdout.write('dotnet: Bau mit csc gescheitert: %s\n' % reason)
            return 1
        command = ['dotnet', dll]
        cwd = DOTNET
    done = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          errors='replace')
    out = ((done.stdout or '') + (done.stderr or '')).strip()
    sys.stdout.write(out[-4000:] + '\n')
    if done.returncode != 0:
        sys.stdout.write('DOTNET ROT (Ausstiegscode %d)\n' % done.returncode)
        return 1
    if 'alles gruen' not in out:
        sys.stdout.write('DOTNET ROT: der Lauf sagt nicht, dass er gruen '
                         'ist\n')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
