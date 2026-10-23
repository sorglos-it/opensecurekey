# -*- coding: utf-8 -*-
"""Release vorbereiten: pruefen, alles bauen, Pruefsummen, Changelog, Dateien sammeln.

    python tools/set_version.py all --release      # zuerst: x + 1, y = 0, z = 0
    python tools/release.py                        # dann: Release-Ordner zusammenstellen
    python tools/release.py --write-changelog      # zusaetzlich CHANGELOG.md fortschreiben
    python tools/release.py --partial              # Teilprogramme ohne Inhalt weglassen

Ablauf, jeder Schritt bricht bei Fehler ab:
  1. Alle drei VERSION-Dateien sind gleich und haben die Form eines Releases (y = z = 0).
  2. Der Arbeitsbaum ist sauber (--allow-dirty hebt das auf) und der Tag v<Version> ist frei.
  3. check.py ist ohne Fehler durchgelaufen.
  4. Alle Teilprogramme sind gebaut. Uebersprungen heisst: Abbruch - ein Release, dem ein
     Teil fehlt, ist keins. Nur --partial erlaubt leere Teilprogramme (nicht fehlende Werkzeuge).
  5. dist/release/v<Version>/ enthaelt die Dateien, SHA256SUMS.txt und RELEASE-NOTES.md.

Das Skript taggt und pusht nie; die Befehle dafuer stehen am Ende. Standardbibliothek.
"""
import argparse
import datetime
import hashlib
import io
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import build
import check
import common
from common import OK, SKIPPED, FAILED

DESKTOP_TARGETS = [('windows', 'amd64'), ('linux', 'amd64'), ('darwin', 'amd64'), ('darwin', 'arm64')]
UNRELEASED = re.compile(r'^## \[Unreleased\][ \t]*\n', re.M)
NEXT_HEADING = re.compile(r'^## ', re.M)


def release_version(root):
    """Die gemeinsame Release-Nummer oder ein ValueError mit dem Grund."""
    versions = {part: common.read_version(part, root) for part in common.PARTS}
    if len(set(versions.values())) != 1:
        raise ValueError('Die Teilprogramme haben verschiedene Versionen (%s) - '
                         '"python tools/set_version.py all --release" zaehlt alle gleich.'
                         % ', '.join('%s %s' % kv for kv in versions.items()))
    version = next(iter(versions.values()))
    v, x, y, z = common.parse_version(version)
    if y != 0 or z != 0 or (v, x) == (0, 0):
        raise ValueError('%s ist kein Release (erwartet v.x.0.0 mit v oder x >= 1). '
                         'Zuerst: python tools/set_version.py all --release' % version)
    return version


def split_unreleased(text):
    """(Text davor inkl. Ueberschrift, Inhalt des Abschnitts, Rest ab dem naechsten ##)."""
    head = UNRELEASED.search(text)
    if not head:
        raise ValueError('CHANGELOG.md hat keinen Abschnitt "## [Unreleased]"')
    rest = text[head.end():]
    nxt = NEXT_HEADING.search(rest)
    body, after = (rest[:nxt.start()], rest[nxt.start():]) if nxt else (rest, '')
    return text[:head.end()], body.strip('\n'), after


def release_changelog(text, version, date):
    before, body, after = split_unreleased(text)
    if not body.strip():
        raise ValueError('Der Abschnitt "[Unreleased]" im CHANGELOG.md ist leer - nichts zu veroeffentlichen.')
    return '%s\n## [%s] - %s\n\n%s\n\n%s' % (before, version, date, body, after) if after else \
        '%s\n## [%s] - %s\n\n%s\n' % (before, version, date, body)


def sha256(path):
    digest = hashlib.sha256()
    with io.open(path, 'rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def git_clean(root):
    code, out = common.run(['git', 'status', '--porcelain'], cwd=root, capture=True, windows=False)
    return code == 0 and not out.strip(), out.strip()


def tag_exists(root, tag):
    code, out = common.run(['git', 'tag', '--list', tag], cwd=root, capture=True, windows=False)
    return code == 0 and bool(out.strip())


def collect(root, version):
    """Kopiert alle Dateien dieser Version aus dist/ nach dist/release/v<Version>/."""
    dist = os.path.join(root, 'dist')
    target = os.path.join(dist, 'release', 'v' + version)
    if os.path.isdir(target):
        shutil.rmtree(target)
    os.makedirs(target)
    names = sorted(n for n in os.listdir(dist)
                   if os.path.isfile(os.path.join(dist, n)) and ('-%s-' % version in n or '-%s.' % version in n))
    for name in names:
        shutil.copyfile(os.path.join(dist, name), os.path.join(target, name))
    with io.open(os.path.join(target, 'SHA256SUMS.txt'), 'w', encoding='utf-8', newline='\n') as handle:
        for name in names:
            handle.write('%s  %s\n' % (sha256(os.path.join(target, name)), name))
    return target, names


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--write-changelog', action='store_true', help='CHANGELOG.md fortschreiben')
    parser.add_argument('--allow-dirty', action='store_true', help='ungespeicherte Aenderungen erlauben')
    parser.add_argument('--partial', action='store_true', help='leere Teilprogramme weglassen')
    args = parser.parse_args(argv)

    def stop(text):
        common.say('ABBRUCH: ' + text)
        return 1

    try:
        version = release_version(root)
    except (OSError, ValueError) as error:
        return stop(str(error))
    common.say('Release v%s' % version)
    clean, listing = git_clean(root)
    if not clean and not args.allow_dirty:
        return stop('Arbeitsbaum nicht sauber (erst committen oder --allow-dirty):\n' + listing)
    if tag_exists(root, 'v' + version):
        return stop('Tag v%s gibt es schon.' % version)
    try:
        notes = split_unreleased(io.open(os.path.join(root, 'CHANGELOG.md'), encoding='utf-8').read())[1]
        if not notes.strip():
            return stop('[Unreleased] im CHANGELOG.md ist leer.')
    except (OSError, ValueError) as error:
        return stop(str(error))

    results = check.run_all(root)
    code, text = check.summarize(results)
    common.print_results([r for r in results if r.status == FAILED])
    if code != 0:
        return stop('check.py meldet Fehler:' + text)

    built = [build.build(p, root, targets=DESKTOP_TARGETS) for p in common.PARTS]
    common.print_results(built)
    for result in built:
        if result.status == FAILED:
            return stop('%s ist nicht baubar.' % result.part)
        if result.status == SKIPPED:
            empty = 'noch leer' in result.detail or 'fehlt - noch nicht baubar' in result.detail
            if not (args.partial and empty):
                return stop('%s wurde nicht gebaut (%s) - so ist es kein vollstaendiges Release.'
                            % (result.part, result.detail))

    target, names = collect(root, version)
    if not names:
        return stop('Kein Bauergebnis fuer v%s in dist/ gefunden.' % version)
    date = datetime.date.today().isoformat()
    notes_path = os.path.join(target, 'RELEASE-NOTES.md')
    with io.open(notes_path, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write('# opensecurekey v%s - %s\n\n%s\n\nSHA-256 siehe SHA256SUMS.txt\n' % (version, date, notes))
    if args.write_changelog:
        path = os.path.join(root, 'CHANGELOG.md')
        text = io.open(path, encoding='utf-8').read()
        with io.open(path, 'w', encoding='utf-8', newline='\n') as handle:
            handle.write(release_changelog(text, version, date))
        common.say('CHANGELOG.md fortgeschrieben.')
    common.say('Release-Ordner: %s' % target)
    for name in names:
        common.say('  ' + name)
    common.say('Naechste Schritte (von Hand):')
    common.say('  git tag v%s' % version)
    common.say('  git push origin v%s' % version)
    common.say('  gh release create v%s %s/* --notes-file %s' % (version, target, notes_path))
    return 0


if __name__ == '__main__':
    sys.exit(main())
